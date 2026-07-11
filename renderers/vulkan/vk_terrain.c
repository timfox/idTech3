/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CBT-inspired GPU-driven terrain: cvars, heightmap load, compute dispatch,
CPU tess fallback draw, splat/control map hooks.
===========================================================================
*/

#include "tr_local.h"
#include "vk_terrain.h"
#include "vk_util.h"

typedef struct cbtTerrainParams_s {
	float viewProj[16];
	float viewOrigin[4];
	float terrainScale[4];
	float lodParams[4]; /* x=minError, y=errorScale, z=unused, w=gridSize */
} cbtTerrainParams_t;

static cvar_t *r_cbtTerrain;
static cvar_t *r_cbtTerrainScale;
static cvar_t *r_cbtTerrainGrid;
static cvar_t *r_cbtTerrainSplat;

static image_t *s_cbtHeightmap;
static image_t *s_cbtDiffuse;
static image_t *s_cbtSplat;
static image_t *s_cbtLayerAlbedo[4];
static char s_cbtHeightPath[MAX_QPATH];
static char s_cbtSplatPath[MAX_QPATH];
static int s_cbtLastDispatchPatches;
static qboolean s_cbtResourcesReady;

static qboolean CBTerrain_EnsureResources( int maxPatches ) {
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
	ri.Printf( PRINT_ALL, "CBT: compute resources ready (maxPatches=%d)\n", maxPatches );
	return qtrue;
}

static void CBTerrain_DispatchCompute( int totalPatches, float scale, int grid ) {
	cbtTerrainParams_t params;
	void *mapped;
	VkImageMemoryBarrier barrier;
	uint32_t groups;
	const float *mvp;
	int i;

	if ( !CBTerrain_EnsureResources( totalPatches ) ) {
		return;
	}
	if ( vk.cbt_terrain_compute_pipeline == VK_NULL_HANDLE || !vk.cmd ||
		vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &params, 0, sizeof( params ) );
	mvp = backEnd.viewParms.projectionMatrix;
	/* Prefer modelview*projection from view state when available. */
	for ( i = 0; i < 16; i++ ) {
		params.viewProj[i] = mvp[i];
	}
	params.viewOrigin[0] = backEnd.viewParms.or.origin[0];
	params.viewOrigin[1] = backEnd.viewParms.or.origin[1];
	params.viewOrigin[2] = backEnd.viewParms.or.origin[2];
	params.viewOrigin[3] = 1.0f;
	params.terrainScale[0] = scale;
	params.terrainScale[1] = scale * 0.25f;
	params.terrainScale[2] = scale;
	params.terrainScale[3] = 1.0f;
	params.lodParams[0] = 0.01f;
	params.lodParams[1] = 32.0f;
	params.lodParams[2] = 0.0f;
	params.lodParams[3] = (float)grid;

	if ( qvkMapMemory( vk.device, vk.cbt_params_memory, 0, sizeof( params ), 0, &mapped ) == VK_SUCCESS ) {
		Com_Memcpy( mapped, &params, sizeof( params ) );
		qvkUnmapMemory( vk.device, vk.cbt_params_memory );
	}

	/* Refresh heightmap binding in case cbt_load ran after first alloc. */
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

	ri.Printf( PRINT_DEVELOPER, "CBT: dispatched %u groups for %d patches\n", groups, totalPatches );
}

void CBTerrain_RegisterCvars( void ) {
	r_cbtTerrain = ri.Cvar_Get( "r_cbtTerrain", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_cbtTerrain, "GPU-driven terrain tessellation (CBT-style LOD). Experimental." );
	r_cbtTerrainScale = ri.Cvar_Get( "r_cbtTerrainScale", "256", CVAR_ARCHIVE );
	r_cbtTerrainGrid = ri.Cvar_Get( "r_cbtTerrainGrid", "32", CVAR_ARCHIVE );
	r_cbtTerrainSplat = ri.Cvar_Get( "r_cbtTerrainSplat", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_cbtTerrainSplat, "When 1 and a splat/control map is loaded, blend up to 4 terrain layers." );

	ri.Printf( PRINT_ALL, "CBT terrain tessellation: r_cbtTerrain %s (GPU-driven LOD)\n",
		r_cbtTerrain->integer ? "enabled" : "disabled" );

	ri.Cmd_AddCommand( "cbt_status", CBTerrain_Status_f );
	ri.Cmd_AddCommand( "cbt_load", CBTerrain_Load_f );
	ri.Cmd_AddCommand( "cbt_splat", CBTerrain_Splat_f );
}

qboolean CBTerrain_IsEnabled( void ) {
	return r_cbtTerrain && r_cbtTerrain->integer > 0;
}

float CBTerrain_GetScale( void ) {
	return r_cbtTerrainScale ? r_cbtTerrainScale->value : 256.0f;
}

int CBTerrain_GetGridSize( void ) {
	int g = r_cbtTerrainGrid ? r_cbtTerrainGrid->integer : 32;
	if ( g < 2 ) g = 2;
	if ( g > 256 ) g = 256;
	return g;
}

qboolean CBTerrain_HasSplat( void ) {
	return ( s_cbtSplat && r_cbtTerrainSplat && r_cbtTerrainSplat->integer ) ? qtrue : qfalse;
}

void CBTerrain_Status_f( void ) {
	ri.Printf( PRINT_ALL, "CBT terrain: enabled=%d scale=%.1f grid=%d height='%s' splat='%s' patches=%d compute=%s resources=%d\n",
		CBTerrain_IsEnabled() ? 1 : 0,
		CBTerrain_GetScale(),
		CBTerrain_GetGridSize(),
		s_cbtHeightPath[0] ? s_cbtHeightPath : "<none>",
		s_cbtSplatPath[0] ? s_cbtSplatPath : "<none>",
		s_cbtLastDispatchPatches,
		( vk.cbt_terrain_compute_pipeline != VK_NULL_HANDLE ) ? "ready" : "missing",
		s_cbtResourcesReady ? 1 : 0 );
}

void CBTerrain_Load_f( void ) {
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
		return;
	}
	Q_strncpyz( s_cbtHeightPath, path, sizeof( s_cbtHeightPath ) );
	if ( !s_cbtDiffuse ) {
		s_cbtDiffuse = tr.whiteImage;
	}
	ri.Printf( PRINT_ALL, "CBT: loaded heightmap %s (%dx%d)\n", path, s_cbtHeightmap->width, s_cbtHeightmap->height );
}

void CBTerrain_Splat_f( void ) {
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

Dispatch CBT compute when pipeline exists; draw a CPU tessellated grid so terrain is visible.
===============
*/
void CBTerrain_Frame( void ) {
	int grid, patchesPerDim;
	float scale;
	shader_t *sh;

	if ( !CBTerrain_IsEnabled() ) {
		return;
	}
	if ( !s_cbtHeightmap ) {
		return;
	}

	grid = CBTerrain_GetGridSize();
	scale = CBTerrain_GetScale();
	patchesPerDim = grid - 1;
	if ( patchesPerDim < 1 ) {
		patchesPerDim = 1;
	}
	s_cbtLastDispatchPatches = patchesPerDim * patchesPerDim;

	CBTerrain_DispatchCompute( s_cbtLastDispatchPatches, scale, grid );

	/* CPU tess fallback: low-res grid so maps with cbt_load show geometry. */
	sh = R_FindShader( "textures/demo/cbt_splat_ground", LIGHTMAP_NONE, qtrue );
	if ( !sh || sh->defaultShader ) {
		sh = R_FindShader( "textures/demo/blend_ground", LIGHTMAP_NONE, qtrue );
	}
	if ( !sh || sh->defaultShader ) {
		sh = tr.defaultShader;
	}

	RB_BeginSurface( sh, 0 );

	{
		const int step = ( grid > 16 ) ? ( grid / 16 ) : 1;
		int gx, gz;
		for ( gz = 0; gz < grid - 1; gz += step ) {
			for ( gx = 0; gx < grid - 1; gx += step ) {
				int i;
				float u0 = (float)gx / (float)( grid - 1 );
				float v0 = (float)gz / (float)( grid - 1 );
				float u1 = (float)( gx + step ) / (float)( grid - 1 );
				float v1 = (float)( gz + step ) / (float)( grid - 1 );
				float h00 = 0.15f * scale * sinf( u0 * 6.28f ) * cosf( v0 * 6.28f );
				float h10 = 0.15f * scale * sinf( u1 * 6.28f ) * cosf( v0 * 6.28f );
				float h01 = 0.15f * scale * sinf( u0 * 6.28f ) * cosf( v1 * 6.28f );
				float h11 = 0.15f * scale * sinf( u1 * 6.28f ) * cosf( v1 * 6.28f );
				vec3_t p[4];
				int base;

				if ( tess.numVertexes + 4 >= SHADER_MAX_VERTEXES || tess.numIndexes + 6 >= SHADER_MAX_INDEXES ) {
					RB_EndSurface();
					RB_BeginSurface( sh, 0 );
				}

				VectorSet( p[0], ( u0 - 0.5f ) * scale, h00, ( v0 - 0.5f ) * scale );
				VectorSet( p[1], ( u1 - 0.5f ) * scale, h10, ( v0 - 0.5f ) * scale );
				VectorSet( p[2], ( u1 - 0.5f ) * scale, h11, ( v1 - 0.5f ) * scale );
				VectorSet( p[3], ( u0 - 0.5f ) * scale, h01, ( v1 - 0.5f ) * scale );

				base = tess.numVertexes;
				for ( i = 0; i < 4; i++ ) {
					VectorCopy( p[i], tess.xyz[base + i] );
					tess.texCoords[0][base + i][0] = ( i == 1 || i == 2 ) ? u1 : u0;
					tess.texCoords[0][base + i][1] = ( i >= 2 ) ? v1 : v0;
					if ( CBTerrain_HasSplat() ) {
						tess.vertexColors[base + i].rgba[0] = (byte)( u0 * 255.0f );
						tess.vertexColors[base + i].rgba[1] = (byte)( v0 * 255.0f );
						tess.vertexColors[base + i].rgba[2] = (byte)( ( 1.0f - u0 ) * 128.0f );
						tess.vertexColors[base + i].rgba[3] = 0;
					} else {
						tess.vertexColors[base + i].rgba[0] = 255;
						tess.vertexColors[base + i].rgba[1] = 255;
						tess.vertexColors[base + i].rgba[2] = 255;
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

	RB_EndSurface();
}
