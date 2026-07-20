/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CBT-inspired GPU-driven terrain: cvars, heightmap load, compute dispatch,
CPU heightfield tess with screen-space LOD, splat/control map hooks.

Raster Ultra 1.14 primary representation: tiled heightfield mesh.
Does not replace classic BSP; inactive without heightmap metadata.
RT stays off. Temporal reconstruction is not required for correctness.
===========================================================================
*/

#include "tr_local.h"
#include "vk_terrain.h"
#include "vk_util.h"
#include "vk_pass_registry.h"
#include "tr_common.h"

#include <math.h>
#include <stdlib.h>

#define CBT_CHUNK_DIM           8
#define CBT_MAX_CHUNKS          ( CBT_CHUNK_DIM * CBT_CHUNK_DIM )
#define CBT_DEFORM_TEX_SIZE     64
#define CBT_HEIGHT_AMPLITUDE    0.35f

typedef struct cbtTerrainParams_s {
	float viewProj[16];
	float viewOrigin[4];
	float terrainScale[4];
	float lodParams[4]; /* x=minError, y=errorScale, z=hysteresis, w=gridSize */
} cbtTerrainParams_t;

typedef struct cbtChunk_s {
	int lod;              /* 0 fine .. 3 coarse */
	int prevLod;
	float geoError;
	int resident;         /* 0 missing, 1 fallback, 2 resident */
	int culled;
	int stitchMask;       /* bit0=+X bit1=-X bit2=+Z bit3=-Z need finer edge */
} cbtChunk_t;

static cvar_t *r_cbtTerrain;
static cvar_t *r_cbtTerrainScale;
static cvar_t *r_cbtTerrainGrid;
static cvar_t *r_cbtTerrainSplat;
static cvar_t *r_cbtTerrainLodHysteresis;
static cvar_t *r_cbtTerrainDebug;
static cvar_t *r_cbtTerrainQuality;
static cvar_t *r_cbtTerrainDeform;

static image_t *s_cbtHeightmap;
static image_t *s_cbtDiffuse;
static image_t *s_cbtSplat;
static image_t *s_cbtLayerAlbedo[4];
static char s_cbtHeightPath[MAX_QPATH];
static char s_cbtSplatPath[MAX_QPATH];
static int s_cbtLastDispatchPatches;
static qboolean s_cbtResourcesReady;

static float *s_heightSamples; /* normalized 0..1, row-major */
static int s_heightW;
static int s_heightH;
static uint32_t s_terrainGeneration;
static cbtChunk_t s_chunks[CBT_MAX_CHUNKS];
static int s_chunksVisible;
static int s_chunksCulled;
static int s_lodOscillationGuards;
static float s_deformDelta[CBT_DEFORM_TEX_SIZE * CBT_DEFORM_TEX_SIZE];
static vec3_t s_originBias;
static char s_fallbackReason[64];

/*
===============
CBTerrain_FreeHeightCPU
===============
*/
static void CBTerrain_FreeHeightCPU( void )
{
	if ( s_heightSamples ) {
		ri.Free( s_heightSamples );
		s_heightSamples = NULL;
	}
	s_heightW = 0;
	s_heightH = 0;
}

/*
===============
CBTerrain_LoadHeightCPU

Keep a CPU height buffer for LOD, normals, biome, and vegetation queries.
Uses TGA path (same as neural loaders); fails soft → procedural fallback heights.
===============
*/
static qboolean CBTerrain_LoadHeightCPU( const char *path )
{
	byte *pic = NULL;
	int w = 0, h = 0, x, y;
	size_t count;

	CBTerrain_FreeHeightCPU();
	R_LoadTGA( path, &pic, &w, &h );
	if ( !pic || w < 2 || h < 2 ) {
		if ( pic ) {
			ri.Free( pic );
		}
		Q_strncpyz( s_fallbackReason, "height_cpu_load_failed", sizeof( s_fallbackReason ) );
		return qfalse;
	}

	count = (size_t)w * (size_t)h;
	s_heightSamples = ri.Malloc( count * sizeof( float ) );
	if ( !s_heightSamples ) {
		ri.Free( pic );
		Q_strncpyz( s_fallbackReason, "height_oom", sizeof( s_fallbackReason ) );
		return qfalse;
	}

	for ( y = 0; y < h; y++ ) {
		for ( x = 0; x < w; x++ ) {
			byte *px = pic + ( y * w + x ) * 4;
			/* Luminance; authored heightmaps often store height in R or RGB gray. */
			s_heightSamples[y * w + x] =
				( (float)px[0] * 0.299f + (float)px[1] * 0.587f + (float)px[2] * 0.114f ) / 255.0f;
		}
	}
	s_heightW = w;
	s_heightH = h;
	ri.Free( pic );
	s_fallbackReason[0] = '\0';
	return qtrue;
}

/*
===============
CBTerrain_SampleHeightUV

Bilinear sample of normalized height (0..1). Procedural sine if no CPU map.
===============
*/
static float CBTerrain_SampleHeightUV( float u, float v )
{
	float x, y, fx, fy;
	int x0, y0, x1, y1;
	float h00, h10, h01, h11;

	if ( u < 0.0f ) {
		u = 0.0f;
	} else if ( u > 1.0f ) {
		u = 1.0f;
	}
	if ( v < 0.0f ) {
		v = 0.0f;
	} else if ( v > 1.0f ) {
		v = 1.0f;
	}

	if ( !s_heightSamples || s_heightW < 2 || s_heightH < 2 ) {
		return 0.15f * sinf( u * 6.2831853f ) * cosf( v * 6.2831853f );
	}

	x = u * (float)( s_heightW - 1 );
	y = v * (float)( s_heightH - 1 );
	x0 = (int)floorf( x );
	y0 = (int)floorf( y );
	x1 = x0 + 1;
	y1 = y0 + 1;
	if ( x0 < 0 ) {
		x0 = 0;
	}
	if ( y0 < 0 ) {
		y0 = 0;
	}
	if ( x1 >= s_heightW ) {
		x1 = s_heightW - 1;
	}
	if ( y1 >= s_heightH ) {
		y1 = s_heightH - 1;
	}
	fx = x - (float)x0;
	fy = y - (float)y0;
	h00 = s_heightSamples[y0 * s_heightW + x0];
	h10 = s_heightSamples[y0 * s_heightW + x1];
	h01 = s_heightSamples[y1 * s_heightW + x0];
	h11 = s_heightSamples[y1 * s_heightW + x1];
	return h00 * ( 1.0f - fx ) * ( 1.0f - fy ) +
		h10 * fx * ( 1.0f - fy ) +
		h01 * ( 1.0f - fx ) * fy +
		h11 * fx * fy;
}

static float CBTerrain_WorldHeightFromUV( float u, float v, float scale )
{
	float h = CBTerrain_SampleHeightUV( u, v ) * CBT_HEIGHT_AMPLITUDE * scale;
	/* Sparse deformation delta (clipmap-style local edits). */
	if ( r_cbtTerrainDeform && r_cbtTerrainDeform->integer ) {
		int dx = (int)( u * ( CBT_DEFORM_TEX_SIZE - 1 ) );
		int dz = (int)( v * ( CBT_DEFORM_TEX_SIZE - 1 ) );
		if ( dx < 0 ) {
			dx = 0;
		}
		if ( dz < 0 ) {
			dz = 0;
		}
		if ( dx >= CBT_DEFORM_TEX_SIZE ) {
			dx = CBT_DEFORM_TEX_SIZE - 1;
		}
		if ( dz >= CBT_DEFORM_TEX_SIZE ) {
			dz = CBT_DEFORM_TEX_SIZE - 1;
		}
		h += s_deformDelta[dz * CBT_DEFORM_TEX_SIZE + dx] * scale;
	}
	return h;
}

static void CBTerrain_UVFromWorld( float worldX, float worldZ, float scale, float *u, float *v )
{
	float inv = ( scale > 1e-3f ) ? ( 1.0f / scale ) : 0.0f;
	*u = ( worldX - s_originBias[0] ) * inv + 0.5f;
	*v = ( worldZ - s_originBias[2] ) * inv + 0.5f;
}

qboolean CBTerrain_SampleHeight( float worldX, float worldZ, float *outHeight )
{
	float scale = CBTerrain_GetScale();
	float u, v;

	if ( !outHeight || !CBTerrain_HasMetadata() ) {
		return qfalse;
	}
	CBTerrain_UVFromWorld( worldX, worldZ, scale, &u, &v );
	*outHeight = CBTerrain_WorldHeightFromUV( u, v, scale ) + s_originBias[1];
	return qtrue;
}

qboolean CBTerrain_SampleNormal( float worldX, float worldZ, vec3_t outNormal )
{
	float scale = CBTerrain_GetScale();
	float u, v, eps, hL, hR, hD, hU;
	vec3_t n;

	if ( !outNormal || !CBTerrain_HasMetadata() ) {
		return qfalse;
	}
	CBTerrain_UVFromWorld( worldX, worldZ, scale, &u, &v );
	eps = 1.0f / (float)( ( s_heightW > 2 ) ? s_heightW : 64 );
	hL = CBTerrain_WorldHeightFromUV( u - eps, v, scale );
	hR = CBTerrain_WorldHeightFromUV( u + eps, v, scale );
	hD = CBTerrain_WorldHeightFromUV( u, v - eps, scale );
	hU = CBTerrain_WorldHeightFromUV( u, v + eps, scale );
	/* Quake Y-up: derivative in XZ plane. */
	n[0] = ( hL - hR );
	n[1] = 2.0f * eps * scale;
	n[2] = ( hD - hU );
	VectorNormalize( n );
	VectorCopy( n, outNormal );
	return qtrue;
}

float CBTerrain_SampleSlope( float worldX, float worldZ )
{
	vec3_t n;
	if ( !CBTerrain_SampleNormal( worldX, worldZ, n ) ) {
		return 0.0f;
	}
	return 1.0f - Com_Clamp( 0.0f, 1.0f, n[1] );
}

static qboolean CBTerrain_EnsureResources( int maxPatches )
{
	VkDeviceSize cmdBytes;
	VkBufferCreateInfo bci;
	VkMemoryAllocateInfo mai;
	VkMemoryRequirements memReq;
	VkImageCreateInfo ici;
	VkImageViewCreateInfo vci;
	VkDescriptorSetAllocateInfo dai;
	VkDescriptorBufferInfo bufInfos[2];
	VkDescriptorImageInfo imgInfos[2];
	VkWriteDescriptorSet writes[4];
	Vk_Sampler_Def sd;

	/* Never allocate terrain GPU resources without heightmap metadata. */
	if ( !CBTerrain_HasMetadata() ) {
		return qfalse;
	}
	if ( s_cbtResourcesReady && vk.cbt_terrain_descriptor != VK_NULL_HANDLE ) {
		return qtrue;
	}
	if ( vk.device == VK_NULL_HANDLE || vk.cbt_terrain_layout == VK_NULL_HANDLE ||
		vk.descriptor_pool == VK_NULL_HANDLE ) {
		return qfalse;
	}
	if ( maxPatches < 1 ) {
		maxPatches = 1;
	}

	cmdBytes = (VkDeviceSize)maxPatches * 5u * sizeof( uint32_t );
	if ( cmdBytes < 256 ) {
		cmdBytes = 256;
	}
	vk.cbt_draw_commands_size = cmdBytes;

	Com_Memset( &bci, 0, sizeof( bci ) );
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = cmdBytes;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if ( qvkCreateBuffer( vk.device, &bci, NULL, &vk.cbt_draw_commands_buffer ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetBufferMemoryRequirements( vk.device, vk.cbt_draw_commands_buffer, &memReq );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = memReq.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, memReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( qvkAllocateMemory( vk.device, &mai, NULL, &vk.cbt_draw_commands_memory ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkBindBufferMemory( vk.device, vk.cbt_draw_commands_buffer, vk.cbt_draw_commands_memory, 0 );

	bci.size = sizeof( cbtTerrainParams_t );
	bci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if ( qvkCreateBuffer( vk.device, &bci, NULL, &vk.cbt_params_buffer ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetBufferMemoryRequirements( vk.device, vk.cbt_params_buffer, &memReq );
	mai.allocationSize = memReq.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, memReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	if ( qvkAllocateMemory( vk.device, &mai, NULL, &vk.cbt_params_memory ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkBindBufferMemory( vk.device, vk.cbt_params_buffer, vk.cbt_params_memory, 0 );

	Com_Memset( &ici, 0, sizeof( ici ) );
	ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.format = VK_FORMAT_R32_UINT;
	ici.extent.width = 1;
	ici.extent.height = 1;
	ici.extent.depth = 1;
	ici.mipLevels = 1;
	ici.arrayLayers = 1;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( qvkCreateImage( vk.device, &ici, NULL, &vk.cbt_patch_counter_image ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetImageMemoryRequirements( vk.device, vk.cbt_patch_counter_image, &memReq );
	mai.allocationSize = memReq.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, memReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( qvkAllocateMemory( vk.device, &mai, NULL, &vk.cbt_patch_counter_memory ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkBindImageMemory( vk.device, vk.cbt_patch_counter_image, vk.cbt_patch_counter_memory, 0 );

	Com_Memset( &vci, 0, sizeof( vci ) );
	vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vci.image = vk.cbt_patch_counter_image;
	vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vci.format = VK_FORMAT_R32_UINT;
	vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vci.subresourceRange.levelCount = 1;
	vci.subresourceRange.layerCount = 1;
	if ( qvkCreateImageView( vk.device, &vci, NULL, &vk.cbt_patch_counter_view ) != VK_SUCCESS ) {
		return qfalse;
	}

	Com_Memset( &dai, 0, sizeof( dai ) );
	dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dai.descriptorPool = vk.descriptor_pool;
	dai.descriptorSetCount = 1;
	dai.pSetLayouts = &vk.cbt_terrain_layout;
	if ( qvkAllocateDescriptorSets( vk.device, &dai, &vk.cbt_terrain_descriptor ) != VK_SUCCESS ) {
		return qfalse;
	}

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;

	bufInfos[0].buffer = vk.cbt_draw_commands_buffer;
	bufInfos[0].offset = 0;
	bufInfos[0].range = cmdBytes;
	bufInfos[1].buffer = vk.cbt_params_buffer;
	bufInfos[1].offset = 0;
	bufInfos[1].range = sizeof( cbtTerrainParams_t );

	imgInfos[0].sampler = VK_NULL_HANDLE;
	imgInfos[0].imageView = vk.cbt_patch_counter_view;
	imgInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfos[1].sampler = s_cbtHeightmap && s_cbtHeightmap->vk_sampler
		? s_cbtHeightmap->vk_sampler : vk_find_sampler( &sd );
	imgInfos[1].imageView = s_cbtHeightmap && s_cbtHeightmap->view
		? s_cbtHeightmap->view : tr.whiteImage->view;
	imgInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.cbt_terrain_descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &bufInfos[0];

	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vk.cbt_terrain_descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[1].pImageInfo = &imgInfos[0];

	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = vk.cbt_terrain_descriptor;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[2].pImageInfo = &imgInfos[1];

	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = vk.cbt_terrain_descriptor;
	writes[3].dstBinding = 3;
	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writes[3].pBufferInfo = &bufInfos[1];

	qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );
	s_cbtResourcesReady = qtrue;
	ri.Printf( PRINT_ALL, "CBT: compute resources ready (maxPatches=%d gen=%u)\n",
		maxPatches, s_terrainGeneration );
	return qtrue;
}

static void CBTerrain_DispatchCompute( int totalPatches, float scale, int grid )
{
	cbtTerrainParams_t params;
	void *mapped;
	VkImageMemoryBarrier barrier;
	uint32_t groups;
	const float *mvp;
	int i;
	float hyst = ( r_cbtTerrainLodHysteresis && r_cbtTerrainLodHysteresis->integer ) ? 1.0f : 0.0f;

	if ( !CBTerrain_EnsureResources( totalPatches ) ) {
		return;
	}
	if ( vk.cbt_terrain_compute_pipeline == VK_NULL_HANDLE || !vk.cmd ||
		vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}

	vk_spine_pass_begin( VK_SPINE_PASS_TERRAIN_LOD );
	Com_Memset( &params, 0, sizeof( params ) );
	mvp = backEnd.viewParms.projectionMatrix;
	for ( i = 0; i < 16; i++ ) {
		params.viewProj[i] = mvp[i];
	}
	params.viewOrigin[0] = backEnd.viewParms.or.origin[0];
	params.viewOrigin[1] = backEnd.viewParms.or.origin[1];
	params.viewOrigin[2] = backEnd.viewParms.or.origin[2];
	params.viewOrigin[3] = 1.0f;
	params.terrainScale[0] = scale;
	params.terrainScale[1] = scale * CBT_HEIGHT_AMPLITUDE;
	params.terrainScale[2] = scale;
	params.terrainScale[3] = 1.0f;
	params.lodParams[0] = 0.01f;
	params.lodParams[1] = 32.0f * ( r_cbtTerrainQuality ? ( 1.0f + 0.25f * r_cbtTerrainQuality->value ) : 1.0f );
	params.lodParams[2] = hyst;
	params.lodParams[3] = (float)grid;

	if ( qvkMapMemory( vk.device, vk.cbt_params_memory, 0, sizeof( params ), 0, &mapped ) == VK_SUCCESS ) {
		Com_Memcpy( mapped, &params, sizeof( params ) );
		qvkUnmapMemory( vk.device, vk.cbt_params_memory );
	}

	if ( s_cbtHeightmap && s_cbtHeightmap->view ) {
		VkDescriptorImageInfo hi;
		VkWriteDescriptorSet w;
		Vk_Sampler_Def sd;
		Com_Memset( &sd, 0, sizeof( sd ) );
		sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
		sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd.noAnisotropy = qtrue;
		hi.sampler = s_cbtHeightmap->vk_sampler ? s_cbtHeightmap->vk_sampler : vk_find_sampler( &sd );
		hi.imageView = s_cbtHeightmap->view;
		hi.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		Com_Memset( &w, 0, sizeof( w ) );
		w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w.dstSet = vk.cbt_terrain_descriptor;
		w.dstBinding = 2;
		w.descriptorCount = 1;
		w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w.pImageInfo = &hi;
		qvkUpdateDescriptorSets( vk.device, 1, &w, 0, NULL );
	}

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = vk.cbt_patch_counter_image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.cbt_terrain_compute_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.cbt_terrain_compute_layout, 0, 1, &vk.cbt_terrain_descriptor, 0, NULL );
	groups = ( (uint32_t)totalPatches + 63u ) / 64u;
	qvkCmdDispatch( vk.cmd->command_buffer, groups, 1, 1 );
	vk_spine_pass_end( VK_SPINE_PASS_TERRAIN_LOD );

	ri.Printf( PRINT_DEVELOPER, "CBT: dispatched %u groups for %d patches\n", groups, totalPatches );
}

/*
===============
CBTerrain_UpdateLOD

Screen-space geometric error per chunk with hysteresis and edge stitch flags.
===============
*/
static void CBTerrain_UpdateLOD( float scale )
{
	const float *origin = backEnd.viewParms.or.origin;
	float half = scale * 0.5f;
	float chunkSize = scale / (float)CBT_CHUNK_DIM;
	float quality = r_cbtTerrainQuality ? r_cbtTerrainQuality->value : 2.0f;
	float pixelError = 4.0f / ( 1.0f + quality );
	int cx, cz, i;
	qboolean useHyst = ( !r_cbtTerrainLodHysteresis || r_cbtTerrainLodHysteresis->integer ) ? qtrue : qfalse;

	s_chunksVisible = 0;
	s_chunksCulled = 0;

	for ( cz = 0; cz < CBT_CHUNK_DIM; cz++ ) {
		for ( cx = 0; cx < CBT_CHUNK_DIM; cx++ ) {
			cbtChunk_t *ch = &s_chunks[cz * CBT_CHUNK_DIM + cx];
			float u0 = (float)cx / (float)CBT_CHUNK_DIM;
			float v0 = (float)cz / (float)CBT_CHUNK_DIM;
			float u1 = (float)( cx + 1 ) / (float)CBT_CHUNK_DIM;
			float v1 = (float)( cz + 1 ) / (float)CBT_CHUNK_DIM;
			float wx = ( ( u0 + u1 ) * 0.5f - 0.5f ) * scale + s_originBias[0];
			float wz = ( ( v0 + v1 ) * 0.5f - 0.5f ) * scale + s_originBias[2];
			float hy = CBTerrain_WorldHeightFromUV( ( u0 + u1 ) * 0.5f, ( v0 + v1 ) * 0.5f, scale ) + s_originBias[1];
			float dx = wx - origin[0];
			float dy = hy - origin[1];
			float dz = wz - origin[2];
			float dist = sqrtf( dx * dx + dy * dy + dz * dz );
			float hVar = fabsf( CBTerrain_SampleHeightUV( u1, v1 ) - CBTerrain_SampleHeightUV( u0, v0 ) );
			float slope = CBTerrain_SampleSlope( wx, wz );
			float err = ( chunkSize * ( 0.15f + hVar + slope * 0.5f ) ) / ( dist + 1.0f );
			int targetLod;
			float promote = pixelError * 0.85f;
			float demote = pixelError * 1.15f;

			ch->geoError = err;
			ch->resident = s_heightSamples ? 2 : 1;
			ch->culled = 0;
			/* Conservative frustum: keep near chunks; cull only far behind camera soft. */
			if ( dist > scale * 3.0f && ( dx * backEnd.viewParms.or.axis[0][0] +
				dz * backEnd.viewParms.or.axis[0][2] ) < -chunkSize ) {
				ch->culled = 1;
				s_chunksCulled++;
				continue;
			}
			s_chunksVisible++;

			if ( err > promote * 4.0f ) {
				targetLod = 0;
			} else if ( err > promote * 2.0f ) {
				targetLod = 1;
			} else if ( err > promote ) {
				targetLod = 2;
			} else {
				targetLod = 3;
			}

			ch->prevLod = ch->lod;
			if ( useHyst ) {
				if ( targetLod < ch->lod ) {
					/* Refine immediately when error demands it. */
					if ( err > promote ) {
						ch->lod = targetLod;
					}
				} else if ( targetLod > ch->lod ) {
					/* Coarsen only past demote threshold (hysteresis). */
					if ( err < demote ) {
						ch->lod = targetLod;
					}
				}
				if ( abs( ch->lod - ch->prevLod ) > 1 ) {
					/* Prevent one-frame holes: step LOD by at most 1. */
					ch->lod = ch->prevLod + ( ( ch->lod > ch->prevLod ) ? 1 : -1 );
					s_lodOscillationGuards++;
				}
			} else {
				ch->lod = targetLod;
			}
		}
	}

	/* Edge stitching: shared edge uses the finer (lower) LOD. */
	for ( cz = 0; cz < CBT_CHUNK_DIM; cz++ ) {
		for ( cx = 0; cx < CBT_CHUNK_DIM; cx++ ) {
			cbtChunk_t *ch = &s_chunks[cz * CBT_CHUNK_DIM + cx];
			ch->stitchMask = 0;
			if ( cx + 1 < CBT_CHUNK_DIM && s_chunks[cz * CBT_CHUNK_DIM + cx + 1].lod < ch->lod ) {
				ch->stitchMask |= 1;
			}
			if ( cx > 0 && s_chunks[cz * CBT_CHUNK_DIM + cx - 1].lod < ch->lod ) {
				ch->stitchMask |= 2;
			}
			if ( cz + 1 < CBT_CHUNK_DIM && s_chunks[( cz + 1 ) * CBT_CHUNK_DIM + cx].lod < ch->lod ) {
				ch->stitchMask |= 4;
			}
			if ( cz > 0 && s_chunks[( cz - 1 ) * CBT_CHUNK_DIM + cx].lod < ch->lod ) {
				ch->stitchMask |= 8;
			}
		}
	}

	(void)half;
	(void)i;
}

void CBTerrain_RegisterCvars( void )
{
	r_cbtTerrain = ri.Cvar_Get( "r_cbtTerrain", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_cbtTerrain,
		"Raster Ultra 1.14 tiled heightfield terrain (CBT LOD). Off by default.\n"
		"Requires cbt_load heightmap metadata. Does not replace classic BSP." );
	r_cbtTerrainScale = ri.Cvar_Get( "r_cbtTerrainScale", "256", CVAR_ARCHIVE );
	r_cbtTerrainGrid = ri.Cvar_Get( "r_cbtTerrainGrid", "32", CVAR_ARCHIVE );
	r_cbtTerrainSplat = ri.Cvar_Get( "r_cbtTerrainSplat", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_cbtTerrainSplat, "When 1 and a splat/control map is loaded, blend up to 4 terrain layers." );
	r_cbtTerrainLodHysteresis = ri.Cvar_Get( "r_cbtTerrainLodHysteresis", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_cbtTerrainLodHysteresis, "0", "1", CV_INTEGER );
	r_cbtTerrainDebug = ri.Cvar_Get( "r_cbtTerrainDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_cbtTerrainDebug, "0", "7", CV_INTEGER );
	ri.Cvar_SetDescription( r_cbtTerrainDebug,
		"Terrain debug: 0 off, 1 LOD, 2 error, 3 chunks, 4 stitch, 5 residency, 6 normals, 7 layers" );
	r_cbtTerrainQuality = ri.Cvar_Get( "r_cbtTerrainQuality", "2", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_cbtTerrainQuality, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_cbtTerrainQuality, "Terrain LOD quality tier: 0 low .. 4 reference" );
	r_cbtTerrainDeform = ri.Cvar_Get( "r_cbtTerrainDeform", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_cbtTerrainDeform, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_cbtTerrainDeform, "Enable sparse height deformation hooks (footprints/craters)." );

	ri.Printf( PRINT_ALL, "CBT terrain (Raster Ultra 1.14): r_cbtTerrain %s (tiled heightfield LOD)\n",
		r_cbtTerrain->integer ? "enabled" : "disabled" );

	ri.Cmd_AddCommand( "cbt_status", CBTerrain_Status_f );
	ri.Cmd_AddCommand( "terrain_status", CBTerrain_Status_f );
	ri.Cmd_AddCommand( "cbt_load", CBTerrain_Load_f );
	ri.Cmd_AddCommand( "cbt_splat", CBTerrain_Splat_f );
}

qboolean CBTerrain_IsEnabled( void )
{
	return r_cbtTerrain && r_cbtTerrain->integer > 0;
}

float CBTerrain_GetScale( void )
{
	return r_cbtTerrainScale ? r_cbtTerrainScale->value : 256.0f;
}

int CBTerrain_GetGridSize( void )
{
	int g = r_cbtTerrainGrid ? r_cbtTerrainGrid->integer : 32;
	if ( g < 2 ) {
		g = 2;
	}
	if ( g > 256 ) {
		g = 256;
	}
	return g;
}

qboolean CBTerrain_HasSplat( void )
{
	return ( s_cbtSplat && r_cbtTerrainSplat && r_cbtTerrainSplat->integer ) ? qtrue : qfalse;
}

qboolean CBTerrain_HasMetadata( void )
{
	return ( s_cbtHeightPath[0] && s_cbtHeightmap ) ? qtrue : qfalse;
}

qboolean CBTerrain_ResourcesReady( void )
{
	return ( CBTerrain_HasMetadata() && s_cbtResourcesReady ) ? qtrue : qfalse;
}

void CBTerrain_OnWorldLoad( void )
{
	s_terrainGeneration++;
	Com_Memset( s_chunks, 0, sizeof( s_chunks ) );
	s_chunksVisible = 0;
	s_chunksCulled = 0;
	s_lodOscillationGuards = 0;
	/* Keep authored heightmap across map loads if still valid; clear deform. */
	Com_Memset( s_deformDelta, 0, sizeof( s_deformDelta ) );
	ri.Printf( PRINT_DEVELOPER, "CBT: world load gen=%u metadata=%d\n",
		s_terrainGeneration, CBTerrain_HasMetadata() ? 1 : 0 );
}

void CBTerrain_OnWorldUnload( void )
{
	/* Drop GPU readiness so descriptors are not reused across map/device resets. */
	s_cbtResourcesReady = qfalse;
	Com_Memset( s_chunks, 0, sizeof( s_chunks ) );
	s_cbtLastDispatchPatches = 0;
}

void CBTerrain_OnOriginRebase( void )
{
	VectorCopy( backEnd.viewParms.or.origin, s_originBias );
	s_originBias[1] = 0.0f;
	Com_Memset( s_chunks, 0, sizeof( s_chunks ) );
	s_lodOscillationGuards = 0;
	ri.Printf( PRINT_DEVELOPER, "CBT: origin rebase (%.0f, %.0f)\n", s_originBias[0], s_originBias[2] );
}

void CBTerrain_Status_f( void )
{
	int i, lodHist[4] = { 0, 0, 0, 0 };
	for ( i = 0; i < CBT_MAX_CHUNKS; i++ ) {
		int l = s_chunks[i].lod;
		if ( l < 0 ) {
			l = 0;
		}
		if ( l > 3 ) {
			l = 3;
		}
		lodHist[l]++;
	}

	ri.Printf( PRINT_ALL, "======== Terrain (Raster Ultra 1.14 / CBT) ========\n" );
	ri.Printf( PRINT_ALL, "enabled      : %d\n", CBTerrain_IsEnabled() ? 1 : 0 );
	ri.Printf( PRINT_ALL, "metadata     : %s\n", CBTerrain_HasMetadata() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "resources    : %s\n", CBTerrain_ResourcesReady() ? "ready" : "idle" );
	ri.Printf( PRINT_ALL, "fallback     : %s\n", s_fallbackReason[0] ? s_fallbackReason : "none" );
	ri.Printf( PRINT_ALL, "representation: tiled heightfield (primary)\n" );
	ri.Printf( PRINT_ALL, "scale/grid   : %.1f / %d\n", CBTerrain_GetScale(), CBTerrain_GetGridSize() );
	ri.Printf( PRINT_ALL, "height       : '%s' cpu=%dx%d samples=%s\n",
		s_cbtHeightPath[0] ? s_cbtHeightPath : "<none>",
		s_heightW, s_heightH, s_heightSamples ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "splat        : '%s'\n", s_cbtSplatPath[0] ? s_cbtSplatPath : "<none>" );
	ri.Printf( PRINT_ALL, "patches      : %d compute=%s\n",
		s_cbtLastDispatchPatches,
		( vk.cbt_terrain_compute_pipeline != VK_NULL_HANDLE ) ? "ready" : "missing" );
	ri.Printf( PRINT_ALL, "chunks       : visible=%d culled=%d lod=[%d %d %d %d] stitch_guards=%d\n",
		s_chunksVisible, s_chunksCulled, lodHist[0], lodHist[1], lodHist[2], lodHist[3],
		s_lodOscillationGuards );
	ri.Printf( PRINT_ALL, "quality/hyst : %d / %d debug=%d deform=%d\n",
		r_cbtTerrainQuality ? r_cbtTerrainQuality->integer : 0,
		r_cbtTerrainLodHysteresis ? r_cbtTerrainLodHysteresis->integer : 0,
		r_cbtTerrainDebug ? r_cbtTerrainDebug->integer : 0,
		r_cbtTerrainDeform ? r_cbtTerrainDeform->integer : 0 );
	ri.Printf( PRINT_ALL, "generation   : %u | BSP ownership preserved\n", s_terrainGeneration );
	ri.Printf( PRINT_ALL, "==================================================\n" );
}

void CBTerrain_Load_f( void )
{
	const char *path;
	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "Usage: cbt_load <heightmap>\n" );
		return;
	}
	path = ri.Cmd_Argv( 1 );
	s_cbtHeightmap = R_FindImageFile( path, IMGFLAG_CLAMPTOEDGE, 0 );
	if ( !s_cbtHeightmap ) {
		ri.Printf( PRINT_WARNING, "CBT: could not load heightmap '%s'\n", path );
		s_cbtHeightPath[0] = '\0';
		CBTerrain_FreeHeightCPU();
		s_cbtResourcesReady = qfalse;
		return;
	}
	Q_strncpyz( s_cbtHeightPath, path, sizeof( s_cbtHeightPath ) );
	if ( !s_cbtDiffuse ) {
		s_cbtDiffuse = tr.whiteImage;
	}
	if ( !CBTerrain_LoadHeightCPU( path ) ) {
		ri.Printf( PRINT_WARNING, "CBT: GPU heightmap ok but CPU samples missing — using procedural heights\n" );
	}
	s_cbtResourcesReady = qfalse; /* rebind on next dispatch */
	s_terrainGeneration++;
	Com_Memset( s_chunks, 0, sizeof( s_chunks ) );
	ri.Printf( PRINT_ALL, "CBT: loaded heightmap %s (%dx%d) cpu=%dx%d\n",
		path, s_cbtHeightmap->width, s_cbtHeightmap->height, s_heightW, s_heightH );
}

void CBTerrain_Splat_f( void )
{
	const char *path;
	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "Usage: cbt_splat <controlMap> [layer0] [layer1] [layer2] [layer3]\n" );
		return;
	}
	path = ri.Cmd_Argv( 1 );
	s_cbtSplat = R_FindImageFile( path, IMGFLAG_CLAMPTOEDGE, 0 );
	if ( !s_cbtSplat ) {
		ri.Printf( PRINT_WARNING, "CBT: could not load splat '%s'\n", path );
		s_cbtSplatPath[0] = '\0';
		return;
	}
	Q_strncpyz( s_cbtSplatPath, path, sizeof( s_cbtSplatPath ) );
	{
		int i;
		for ( i = 0; i < 4 && ( i + 2 ) < ri.Cmd_Argc(); i++ ) {
			s_cbtLayerAlbedo[i] = R_FindImageFile( ri.Cmd_Argv( i + 2 ), IMGFLAG_NONE, 0 );
		}
	}
	ri.Printf( PRINT_ALL, "CBT: splat control %s (materialBlend splat path)\n", path );
}

/*
===============
CBTerrain_Frame

Dispatch CBT compute when pipeline exists; draw heightmap-sampled tess grid.
Inactive without metadata — never clears BSP or leaves scene color unwritten.
===============
*/
void CBTerrain_Frame( void )
{
	int grid, patchesPerDim;
	float scale;
	shader_t *sh;
	int cx, cz;

	if ( !CBTerrain_IsEnabled() ) {
		return;
	}
	if ( !CBTerrain_HasMetadata() ) {
		return;
	}

	grid = CBTerrain_GetGridSize();
	scale = CBTerrain_GetScale();
	patchesPerDim = grid - 1;
	if ( patchesPerDim < 1 ) {
		patchesPerDim = 1;
	}
	s_cbtLastDispatchPatches = patchesPerDim * patchesPerDim;

	CBTerrain_UpdateLOD( scale );
	CBTerrain_DispatchCompute( s_cbtLastDispatchPatches, scale, grid );

	sh = R_FindShader( "textures/demo/cbt_splat_ground", LIGHTMAP_NONE, qtrue );
	if ( !sh || sh->defaultShader ) {
		sh = R_FindShader( "textures/demo/blend_ground", LIGHTMAP_NONE, qtrue );
	}
	if ( !sh || sh->defaultShader ) {
		sh = tr.defaultShader;
	}

	vk_spine_pass_begin( VK_SPINE_PASS_TERRAIN_DRAW );
	RB_BeginSurface( sh, 0 );

	for ( cz = 0; cz < CBT_CHUNK_DIM; cz++ ) {
		for ( cx = 0; cx < CBT_CHUNK_DIM; cx++ ) {
			cbtChunk_t *ch = &s_chunks[cz * CBT_CHUNK_DIM + cx];
			int step;
			int gx0, gz0, gx1, gz1, gx, gz;
			float uChunk0, vChunk0, uChunk1, vChunk1;

			if ( ch->culled ) {
				continue;
			}

			/* LOD → tess step within chunk (power-of-two). */
			step = 1 << ch->lod;
			if ( r_cbtTerrainQuality && r_cbtTerrainQuality->integer >= 3 && step > 1 ) {
				step = step / 2;
				if ( step < 1 ) {
					step = 1;
				}
			}
			/* Map chunk into grid indices. */
			gx0 = ( cx * ( grid - 1 ) ) / CBT_CHUNK_DIM;
			gz0 = ( cz * ( grid - 1 ) ) / CBT_CHUNK_DIM;
			gx1 = ( ( cx + 1 ) * ( grid - 1 ) ) / CBT_CHUNK_DIM;
			gz1 = ( ( cz + 1 ) * ( grid - 1 ) ) / CBT_CHUNK_DIM;
			if ( gx1 <= gx0 ) {
				gx1 = gx0 + 1;
			}
			if ( gz1 <= gz0 ) {
				gz1 = gz0 + 1;
			}
			uChunk0 = (float)cx / (float)CBT_CHUNK_DIM;
			vChunk0 = (float)cz / (float)CBT_CHUNK_DIM;
			uChunk1 = (float)( cx + 1 ) / (float)CBT_CHUNK_DIM;
			vChunk1 = (float)( cz + 1 ) / (float)CBT_CHUNK_DIM;
			(void)uChunk0;
			(void)vChunk0;
			(void)uChunk1;
			(void)vChunk1;

			for ( gz = gz0; gz < gz1; gz += step ) {
				for ( gx = gx0; gx < gx1; gx += step ) {
					int i, base;
					int gxB = gx + step;
					int gzB = gz + step;
					float u0, v0, u1, v1;
					float h00, h10, h01, h11;
					vec3_t p[4], n[4];
					byte debugTint;

					if ( gxB > gx1 ) {
						gxB = gx1;
					}
					if ( gzB > gz1 ) {
						gzB = gz1;
					}
					if ( gxB <= gx || gzB <= gz ) {
						continue;
					}

					if ( tess.numVertexes + 4 >= SHADER_MAX_VERTEXES ||
						tess.numIndexes + 6 >= SHADER_MAX_INDEXES ) {
						RB_EndSurface();
						RB_BeginSurface( sh, 0 );
					}

					u0 = (float)gx / (float)( grid - 1 );
					v0 = (float)gz / (float)( grid - 1 );
					u1 = (float)gxB / (float)( grid - 1 );
					v1 = (float)gzB / (float)( grid - 1 );
					h00 = CBTerrain_WorldHeightFromUV( u0, v0, scale );
					h10 = CBTerrain_WorldHeightFromUV( u1, v0, scale );
					h01 = CBTerrain_WorldHeightFromUV( u0, v1, scale );
					h11 = CBTerrain_WorldHeightFromUV( u1, v1, scale );

					VectorSet( p[0], ( u0 - 0.5f ) * scale + s_originBias[0], h00 + s_originBias[1],
						( v0 - 0.5f ) * scale + s_originBias[2] );
					VectorSet( p[1], ( u1 - 0.5f ) * scale + s_originBias[0], h10 + s_originBias[1],
						( v0 - 0.5f ) * scale + s_originBias[2] );
					VectorSet( p[2], ( u1 - 0.5f ) * scale + s_originBias[0], h11 + s_originBias[1],
						( v1 - 0.5f ) * scale + s_originBias[2] );
					VectorSet( p[3], ( u0 - 0.5f ) * scale + s_originBias[0], h01 + s_originBias[1],
						( v1 - 0.5f ) * scale + s_originBias[2] );

					CBTerrain_SampleNormal( p[0][0], p[0][2], n[0] );
					CBTerrain_SampleNormal( p[1][0], p[1][2], n[1] );
					CBTerrain_SampleNormal( p[2][0], p[2][2], n[2] );
					CBTerrain_SampleNormal( p[3][0], p[3][2], n[3] );

					debugTint = 255;
					if ( r_cbtTerrainDebug && r_cbtTerrainDebug->integer == 1 ) {
						debugTint = (byte)( 255 - ch->lod * 60 );
					}

					base = tess.numVertexes;
					for ( i = 0; i < 4; i++ ) {
						VectorCopy( p[i], tess.xyz[base + i] );
						VectorCopy( n[i], tess.normal[base + i] );
						tess.texCoords[0][base + i][0] = ( i == 1 || i == 2 ) ? u1 : u0;
						tess.texCoords[0][base + i][1] = ( i >= 2 ) ? v1 : v0;
						if ( CBTerrain_HasSplat() ) {
							tess.vertexColors[base + i].rgba[0] = (byte)( u0 * 255.0f );
							tess.vertexColors[base + i].rgba[1] = (byte)( v0 * 255.0f );
							tess.vertexColors[base + i].rgba[2] = (byte)( ( 1.0f - u0 ) * 128.0f );
							tess.vertexColors[base + i].rgba[3] = 0;
						} else {
							tess.vertexColors[base + i].rgba[0] = debugTint;
							tess.vertexColors[base + i].rgba[1] = debugTint;
							tess.vertexColors[base + i].rgba[2] = debugTint;
							tess.vertexColors[base + i].rgba[3] = 255;
						}
					}
					tess.indexes[tess.numIndexes + 0] = base + 0;
					tess.indexes[tess.numIndexes + 1] = base + 1;
					tess.indexes[tess.numIndexes + 2] = base + 2;
					tess.indexes[tess.numIndexes + 3] = base + 0;
					tess.indexes[tess.numIndexes + 4] = base + 2;
					tess.indexes[tess.numIndexes + 5] = base + 3;
					tess.numVertexes += 4;
					tess.numIndexes += 6;
				}
			}
		}
	}

	RB_EndSurface();
	vk_spine_pass_end( VK_SPINE_PASS_TERRAIN_DRAW );
}
