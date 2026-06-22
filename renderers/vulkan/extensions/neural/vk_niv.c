/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Neural Irradiance Volume — compact 3D neural probe field; G-buffer decode pass.
See docs/NEURAL_IRRADIANCE_VOLUME.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_niv.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_view_state.h"
#include "vk_deferred_gbuffer.h"
#include "vk_render_pass.h"
#include "vk_staging.h"
#include "vk_cmd.h"
#include "vk_neural_io.h"

#define NIV_MANIFEST_VERSION    1
#define NIV_MAGIC_WEIGHTS       0x3156494E /* 'NIV1' */
#define NIV_MAX_GRID_X          64
#define NIV_MAX_GRID_Y          32
#define NIV_MAX_GRID_Z          64
#define NIV_MAX_FEATURE_DIM     4
#define NIV_MAX_HIDDEN          32

typedef struct {
	int         version;
	int         gridX;
	int         gridY;
	int         gridZ;
	int         featureDim;
	int         hiddenDim;
	float       worldMin[3];
	float       worldMax[3];
	char        volumePath[MAX_QPATH];
	char        weightsPath[MAX_QPATH];
} nivManifest_t;

typedef struct {
	qboolean    loaded;
	qboolean    procedural;
	char        mapName[MAX_QPATH];
	nivManifest_t man;

	float       W1[NIV_MAX_HIDDEN * NIV_MAX_FEATURE_DIM];
	float       b1[NIV_MAX_HIDDEN];
	float       W2[3 * NIV_MAX_HIDDEN];
	float       b2[3];

	uint32_t    targetWidth;
	uint32_t    targetHeight;
} nivState_t;

static nivState_t niv;

static VkSampler NIV_DepthSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static VkSampler NIV_LinearSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	return vk_find_sampler( &sd );
}

static cvar_t *r_niv;
static cvar_t *r_niv_strength;
static cvar_t *r_niv_scale;
static cvar_t *r_niv_gridX;
static cvar_t *r_niv_gridY;
static cvar_t *r_niv_gridZ;
static cvar_t *r_niv_featureDim;
static cvar_t *r_niv_hiddenDim;
static cvar_t *r_niv_useGBuffer;
static cvar_t *r_niv_debug;
static cvar_t *r_niv_skipSky;

typedef struct {
	float invViewProj[16];
	float worldMin[4];
	float worldMax[4];
	uint32_t gridDim[3];
	uint32_t featureDim;
	uint32_t hiddenDim;
	uint32_t useGBufferNormal;
	uint32_t hasGBuffer;
	uint32_t extent[2];
	float strength;
} vk_niv_shade_push_t;

typedef struct {
	uint32_t extent[2];
	float strength;
	uint32_t skipSky;
} vk_niv_composite_push_t;

static void NIV_ClearGpu( void )
{
	if ( vk.niv.shade_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.niv.shade_pipeline, NULL );
		vk.niv.shade_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.niv.composite_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.niv.composite_pipeline, NULL );
		vk.niv.composite_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.niv.shade_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.niv.shade_pipeline_layout, NULL );
		vk.niv.shade_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.niv.composite_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.niv.composite_pipeline_layout, NULL );
		vk.niv.composite_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.niv.shade_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.niv.shade_layout, NULL );
		vk.niv.shade_layout = VK_NULL_HANDLE;
	}
	if ( vk.niv.composite_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.niv.composite_layout, NULL );
		vk.niv.composite_layout = VK_NULL_HANDLE;
	}
	if ( vk.niv.shade_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.niv.shade_pool, NULL );
		vk.niv.shade_pool = VK_NULL_HANDLE;
	}
	if ( vk.niv.composite_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.niv.composite_pool, NULL );
		vk.niv.composite_pool = VK_NULL_HANDLE;
	}
	if ( vk.niv.weights_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.niv.weights_buffer, NULL );
		vk.niv.weights_buffer = VK_NULL_HANDLE;
	}
	if ( vk.niv.weights_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.niv.weights_memory, NULL );
		vk.niv.weights_memory = VK_NULL_HANDLE;
	}
	if ( vk.niv.feature_volume != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.niv.feature_volume, NULL );
		vk.niv.feature_volume = VK_NULL_HANDLE;
	}
	if ( vk.niv.feature_volume_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.niv.feature_volume_view, NULL );
		vk.niv.feature_volume_view = VK_NULL_HANDLE;
	}
	if ( vk.niv.feature_volume_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.niv.feature_volume_memory, NULL );
		vk.niv.feature_volume_memory = VK_NULL_HANDLE;
	}
	if ( vk.niv.irradiance_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.niv.irradiance_image, NULL );
		vk.niv.irradiance_image = VK_NULL_HANDLE;
	}
	if ( vk.niv.irradiance_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.niv.irradiance_view, NULL );
		vk.niv.irradiance_view = VK_NULL_HANDLE;
	}
	if ( vk.niv.irradiance_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.niv.irradiance_memory, NULL );
		vk.niv.irradiance_memory = VK_NULL_HANDLE;
	}
	vk.niv.shade_descriptor = VK_NULL_HANDLE;
	vk.niv.composite_descriptor = VK_NULL_HANDLE;
	vk.niv.shade_ready = qfalse;
	vk.niv.composite_ready = qfalse;
	vk.niv.volume_ready = qfalse;
	vk.nivAllocated = qfalse;
}

static void NIV_BuildDefaultWeights( void )
{
	int h = niv.man.hiddenDim;
	int f = niv.man.featureDim;
	int i, c;

	Com_Memset( niv.W1, 0, sizeof( niv.W1 ) );
	Com_Memset( niv.b1, 0, sizeof( niv.b1 ) );
	Com_Memset( niv.W2, 0, sizeof( niv.W2 ) );
	Com_Memset( niv.b2, 0, sizeof( niv.b2 ) );

	for ( i = 0; i < h; i++ ) {
		niv.b1[i] = -0.05f;
		for ( int j = 0; j < f && j < 3; j++ ) {
			niv.W1[i * NIV_MAX_FEATURE_DIM + j] = ( i == j ) ? 0.85f : 0.04f;
		}
	}
	for ( c = 0; c < 3; c++ ) {
		for ( i = 0; i < h; i++ ) {
			niv.W2[c * NIV_MAX_HIDDEN + i] = ( i % 3 == c ) ? 0.75f : 0.03f;
		}
		niv.b2[c] = 0.1f;
	}
}

static qboolean NIV_ParseManifest( const char *text, nivManifest_t *man )
{
	const char *parse;
	const char *token;
	char key[64];
	char value[MAX_TOKEN_CHARS];

	Com_Memset( man, 0, sizeof( *man ) );
	man->version = NIV_MANIFEST_VERSION;
	man->gridX = 32;
	man->gridY = 16;
	man->gridZ = 32;
	man->featureDim = 4;
	man->hiddenDim = 16;
	man->worldMin[0] = -4096.0f;
	man->worldMin[1] = -4096.0f;
	man->worldMin[2] = -1024.0f;
	man->worldMax[0] = 4096.0f;
	man->worldMax[1] = 4096.0f;
	man->worldMax[2] = 1024.0f;

	parse = text;
	while ( 1 ) {
		token = COM_Parse( &parse );
		if ( !token[0] ) {
			break;
		}
		Q_strncpyz( key, token, sizeof( key ) );
		token = COM_Parse( &parse );
		if ( !token[0] ) {
			break;
		}
		Q_strncpyz( value, token, sizeof( value ) );

		if ( !Q_stricmp( key, "gridX" ) ) {
			man->gridX = atoi( value );
		} else if ( !Q_stricmp( key, "gridY" ) ) {
			man->gridY = atoi( value );
		} else if ( !Q_stricmp( key, "gridZ" ) ) {
			man->gridZ = atoi( value );
		} else if ( !Q_stricmp( key, "featureDim" ) ) {
			man->featureDim = atoi( value );
		} else if ( !Q_stricmp( key, "hiddenDim" ) ) {
			man->hiddenDim = atoi( value );
		} else if ( !Q_stricmp( key, "worldMin" ) ) {
			sscanf( value, "%f %f %f", &man->worldMin[0], &man->worldMin[1], &man->worldMin[2] );
		} else if ( !Q_stricmp( key, "worldMax" ) ) {
			sscanf( value, "%f %f %f", &man->worldMax[0], &man->worldMax[1], &man->worldMax[2] );
		} else if ( !Q_stricmp( key, "volumePath" ) ) {
			Q_strncpyz( man->volumePath, value, sizeof( man->volumePath ) );
		} else if ( !Q_stricmp( key, "weightsPath" ) ) {
			Q_strncpyz( man->weightsPath, value, sizeof( man->weightsPath ) );
		}
	}

	if ( man->gridX < 4 || man->gridY < 4 || man->gridZ < 4 ) {
		return qfalse;
	}
	if ( man->gridX > NIV_MAX_GRID_X || man->gridY > NIV_MAX_GRID_Y || man->gridZ > NIV_MAX_GRID_Z ) {
		return qfalse;
	}
	if ( man->featureDim < 1 || man->featureDim > NIV_MAX_FEATURE_DIM ) {
		return qfalse;
	}
	if ( man->hiddenDim < 1 || man->hiddenDim > NIV_MAX_HIDDEN ) {
		return qfalse;
	}
	return qtrue;
}

static void NIV_ApplyWorldBoundsFromMap( void )
{
	if ( !tr.world ) {
		return;
	}
	niv.man.worldMin[0] = tr.world->lightGridOrigin[0];
	niv.man.worldMin[1] = tr.world->lightGridOrigin[1];
	niv.man.worldMin[2] = tr.world->lightGridOrigin[2];
	niv.man.worldMax[0] = tr.world->lightGridOrigin[0] +
		tr.world->lightGridSize[0] * tr.world->lightGridBounds[0];
	niv.man.worldMax[1] = tr.world->lightGridOrigin[1] +
		tr.world->lightGridSize[1] * tr.world->lightGridBounds[1];
	niv.man.worldMax[2] = tr.world->lightGridOrigin[2] +
		tr.world->lightGridSize[2] * tr.world->lightGridBounds[2];
}

static void NIV_FillProceduralVolume( float *vox, int gx, int gy, int gz, int fdim )
{
	int z, y, x, f;
	for ( z = 0; z < gz; z++ ) {
		for ( y = 0; y < gy; y++ ) {
			for ( x = 0; x < gx; x++ ) {
				float u = (float)x / (float)gx;
				float v = (float)y / (float)gy;
				float w = (float)z / (float)gz;
				float *feat = vox + ( ( z * gy + y ) * gx + x ) * fdim;
				for ( f = 0; f < fdim; f++ ) {
					switch ( f % 4 ) {
					case 0:
						feat[f] = 0.4f + 0.45f * sinf( ( u + w ) * 6.28318f );
						break;
					case 1:
						feat[f] = 0.35f + 0.4f * cosf( ( v - w * 0.5f ) * 6.28318f );
						break;
					case 2:
						feat[f] = 0.3f + 0.35f * ( 1.0f - w );
						break;
					default:
						feat[f] = 0.45f + 0.2f * sinf( ( u + v ) * 12.56636f );
						break;
					}
				}
			}
		}
	}
}

static qboolean NIV_UploadFeatureVolume3D( const float *vox, int gx, int gy, int gz, int fdim )
{
	VkCommandBuffer cmd;
	VkDeviceSize uploadBytes;
	uint32_t width, height, depth;
	VkImageCreateInfo image_desc;
	VkImageViewCreateInfo view_desc;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_info;
	VkBufferImageCopy region;
	int i;
	float *packed;

	if ( !vox || !vk.device || vk.device_lost ) {
		return qfalse;
	}

	width = (uint32_t)gx;
	height = (uint32_t)gy;
	depth = (uint32_t)gz;
	uploadBytes = (VkDeviceSize)( gx * gy * gz ) * 4 * sizeof( float );

	packed = (float *)ri.Malloc( (size_t)uploadBytes );
	for ( i = 0; i < gx * gy * gz; i++ ) {
		int f;
		for ( f = 0; f < 4; f++ ) {
			packed[i * 4 + f] = ( f < fdim ) ? vox[i * fdim + f] : 0.0f;
		}
	}

	if ( vk.niv.feature_volume != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.niv.feature_volume_view, NULL );
		qvkDestroyImage( vk.device, vk.niv.feature_volume, NULL );
		qvkFreeMemory( vk.device, vk.niv.feature_volume_memory, NULL );
		vk.niv.feature_volume = VK_NULL_HANDLE;
		vk.niv.feature_volume_view = VK_NULL_HANDLE;
		vk.niv.feature_volume_memory = VK_NULL_HANDLE;
	}

	Com_Memset( &image_desc, 0, sizeof( image_desc ) );
	image_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_desc.imageType = VK_IMAGE_TYPE_3D;
	image_desc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	image_desc.extent.width = width;
	image_desc.extent.height = height;
	image_desc.extent.depth = depth;
	image_desc.mipLevels = 1;
	image_desc.arrayLayers = 1;
	image_desc.samples = VK_SAMPLE_COUNT_1_BIT;
	image_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	image_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if ( qvkCreateImage( vk.device, &image_desc, NULL, &vk.niv.feature_volume ) != VK_SUCCESS ) {
		ri.Free( packed );
		return qfalse;
	}

	qvkGetImageMemoryRequirements( vk.device, vk.niv.feature_volume, &mem_req );
	Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.niv.feature_volume_memory ) != VK_SUCCESS ||
		qvkBindImageMemory( vk.device, vk.niv.feature_volume, vk.niv.feature_volume_memory, 0 ) != VK_SUCCESS ) {
		qvkDestroyImage( vk.device, vk.niv.feature_volume, NULL );
		ri.Free( packed );
		return qfalse;
	}

	Com_Memset( &view_desc, 0, sizeof( view_desc ) );
	view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_desc.image = vk.niv.feature_volume;
	view_desc.viewType = VK_IMAGE_VIEW_TYPE_3D;
	view_desc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_desc.subresourceRange.levelCount = 1;
	view_desc.subresourceRange.layerCount = 1;
	if ( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.niv.feature_volume_view ) != VK_SUCCESS ) {
		NIV_ClearGpu();
		ri.Free( packed );
		return qfalse;
	}

	if ( vk.staging_buffer.size < uploadBytes ) {
		vk_alloc_staging_buffer( uploadBytes );
	}
	if ( !vk.staging_buffer.ptr || vk.staging_buffer.size < uploadBytes ) {
		ri.Free( packed );
		return qfalse;
	}

	Com_Memcpy( vk.staging_buffer.ptr, packed, (size_t)uploadBytes );
	ri.Free( packed );

	cmd = vk_begin_command_buffer();
	record_image_layout_transition( cmd, vk.niv.feature_volume, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );

	Com_Memset( &region, 0, sizeof( region ) );
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageExtent.width = width;
	region.imageExtent.height = height;
	region.imageExtent.depth = depth;
	region.bufferRowLength = width;
	region.bufferImageHeight = height;
	region.bufferOffset = 0;

	qvkCmdCopyBufferToImage( cmd, vk.staging_buffer.handle, vk.niv.feature_volume,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

	record_image_layout_transition( cmd, vk.niv.feature_volume, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	vk_end_command_buffer( cmd, __func__ );
	vk.niv.volume_ready = qtrue;
	return qtrue;
}

static qboolean NIV_UploadWeightsBuffer( void )
{
	int h = niv.man.hiddenDim;
	int f = niv.man.featureDim;
	VkDeviceSize size;
	float *cpu;
	VkBufferCreateInfo buf_ci;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_ci;
	void *mapped;

	size = (VkDeviceSize)( ( h * f ) + h + ( 3 * h ) + 3 ) * sizeof( float );
	cpu = (float *)ri.Malloc( (size_t)size );
	Com_Memcpy( cpu, niv.W1, h * f * sizeof( float ) );
	Com_Memcpy( cpu + h * f, niv.b1, h * sizeof( float ) );
	Com_Memcpy( cpu + h * f + h, niv.W2, 3 * h * sizeof( float ) );
	Com_Memcpy( cpu + h * f + h + 3 * h, niv.b2, 3 * sizeof( float ) );

	if ( vk.niv.weights_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.niv.weights_buffer, NULL );
		qvkFreeMemory( vk.device, vk.niv.weights_memory, NULL );
	}

	Com_Memset( &buf_ci, 0, sizeof( buf_ci ) );
	buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_ci.size = size;
	buf_ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_ci, NULL, &vk.niv.weights_buffer ) );

	qvkGetBufferMemoryRequirements( vk.device, vk.niv.weights_buffer, &mem_req );
	Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
	alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_ci.allocationSize = mem_req.size;
	alloc_ci.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_ci, NULL, &vk.niv.weights_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.niv.weights_buffer, vk.niv.weights_memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.niv.weights_memory, 0, size, 0, &mapped ) );
	Com_Memcpy( mapped, cpu, (size_t)size );
	qvkUnmapMemory( vk.device, vk.niv.weights_memory );
	ri.Free( cpu );
	vk.niv.weights_size = size;
	return qtrue;
}

static qboolean NIV_EnsureIrradianceTarget( uint32_t width, uint32_t height )
{
	if ( vk.niv.irradiance_image != VK_NULL_HANDLE &&
		niv.targetWidth == width && niv.targetHeight == height ) {
		return qtrue;
	}

	if ( vk.niv.irradiance_image != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.niv.irradiance_view, NULL );
		qvkDestroyImage( vk.device, vk.niv.irradiance_image, NULL );
		qvkFreeMemory( vk.device, vk.niv.irradiance_memory, NULL );
		vk.niv.irradiance_image = VK_NULL_HANDLE;
		vk.niv.irradiance_view = VK_NULL_HANDLE;
		vk.niv.irradiance_memory = VK_NULL_HANDLE;
	}

	{
		VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		VkImageCreateInfo image_desc;
		VkImageViewCreateInfo view_desc;
		VkMemoryRequirements mem_req;
		VkMemoryAllocateInfo alloc_info;

		Com_Memset( &image_desc, 0, sizeof( image_desc ) );
		image_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_desc.imageType = VK_IMAGE_TYPE_2D;
		image_desc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		image_desc.extent.width = width;
		image_desc.extent.height = height;
		image_desc.extent.depth = 1;
		image_desc.mipLevels = 1;
		image_desc.arrayLayers = 1;
		image_desc.samples = VK_SAMPLE_COUNT_1_BIT;
		image_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_desc.usage = usage;
		image_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if ( qvkCreateImage( vk.device, &image_desc, NULL, &vk.niv.irradiance_image ) != VK_SUCCESS ) {
			return qfalse;
		}
		qvkGetImageMemoryRequirements( vk.device, vk.niv.irradiance_image, &mem_req );
		Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		if ( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.niv.irradiance_memory ) != VK_SUCCESS ||
			qvkBindImageMemory( vk.device, vk.niv.irradiance_image, vk.niv.irradiance_memory, 0 ) != VK_SUCCESS ) {
			qvkDestroyImage( vk.device, vk.niv.irradiance_image, NULL );
			vk.niv.irradiance_image = VK_NULL_HANDLE;
			return qfalse;
		}
		Com_Memset( &view_desc, 0, sizeof( view_desc ) );
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.image = vk.niv.irradiance_image;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_desc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.layerCount = 1;
		if ( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.niv.irradiance_view ) != VK_SUCCESS ) {
			return qfalse;
		}
	}

	niv.targetWidth = width;
	niv.targetHeight = height;
	return qtrue;
}

static void NIV_CreateShadePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[5];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.niv.shade_ready ) {
		return;
	}
	if ( vk.modules.niv_shade_cs == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[4].binding = 4;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 5;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.niv.shade_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_niv_shade_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.niv.shade_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.niv.shade_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.niv_shade_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.niv.shade_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.niv.shade_pipeline ) );
	vk.niv.shade_ready = qtrue;
}

static void NIV_CreateCompositePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[4];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.niv.composite_ready ) {
		return;
	}
	if ( vk.modules.niv_composite_cs == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 4;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.niv.composite_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_niv_composite_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.niv.composite_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.niv.composite_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.niv_composite_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.niv.composite_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.niv.composite_pipeline ) );
	vk.niv.composite_ready = qtrue;
}

static void NIV_UpdateShadeDescriptors( VkImageView depthView, VkImageView normalView )
{
	VkDescriptorPoolSize pool_sizes[2];
	VkDescriptorPoolCreateInfo pool_ci;
	VkDescriptorSetAllocateInfo alloc_ci;
	VkDescriptorImageInfo img_infos[5];
	VkDescriptorBufferInfo buf_info;
	VkWriteDescriptorSet writes[5];
	if ( vk.niv.shade_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.niv.shade_pool, NULL );
		vk.niv.shade_pool = VK_NULL_HANDLE;
		vk.niv.shade_descriptor = VK_NULL_HANDLE;
	}

	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[0].descriptorCount = 3;
	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	pool_sizes[1].descriptorCount = 1;
	Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
	pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_ci.maxSets = 1;
	pool_ci.poolSizeCount = 2;
	pool_ci.pPoolSizes = pool_sizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.niv.shade_pool ) );

	Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
	alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_ci.descriptorPool = vk.niv.shade_pool;
	alloc_ci.descriptorSetCount = 1;
	alloc_ci.pSetLayouts = &vk.niv.shade_layout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &vk.niv.shade_descriptor ) );

	Com_Memset( img_infos, 0, sizeof( img_infos ) );
	img_infos[0].sampler = NIV_DepthSampler();
	img_infos[0].imageView = depthView;
	img_infos[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	img_infos[1].sampler = NIV_LinearSampler();
	img_infos[1].imageView = normalView ? normalView :
		( tr.whiteImage ? tr.whiteImage->view : vk.color_image_view );
	img_infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_infos[2].sampler = NIV_LinearSampler();
	img_infos[2].imageView = vk.niv.feature_volume_view;
	img_infos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	buf_info.buffer = vk.niv.weights_buffer;
	buf_info.offset = 0;
	buf_info.range = vk.niv.weights_size;

	img_infos[4].imageView = vk.niv.irradiance_view;
	img_infos[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.niv.shade_descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &img_infos[0];
	writes[1] = writes[0];
	writes[1].dstBinding = 1;
	writes[1].pImageInfo = &img_infos[1];
	writes[2] = writes[0];
	writes[2].dstBinding = 2;
	writes[2].pImageInfo = &img_infos[2];
	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = vk.niv.shade_descriptor;
	writes[3].dstBinding = 3;
	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[3].pBufferInfo = &buf_info;
	writes[4] = writes[0];
	writes[4].dstBinding = 4;
	writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[4].pImageInfo = &img_infos[4];
	qvkUpdateDescriptorSets( vk.device, 5, writes, 0, NULL );
}

static void NIV_FillInvViewProj( float *out16 )
{
	float viewProj[16];
	const float *view;
	const float *projection;

	view = backEnd.viewParms.world.modelViewMatrix;
	projection = backEnd.useFirstPersonProjection ?
		backEnd.firstPersonProjectionMatrix : backEnd.viewParms.projectionMatrix;
	myGlMultMatrix( view, projection, viewProj );
	if ( !vk_mat4_inverse( viewProj, out16 ) ) {
		Com_Memcpy( out16, viewProj, sizeof( viewProj ) );
	}
}

static void NIV_Cmd_Reload( void )
{
	if ( niv.mapName[0] ) {
		R_NIV_OnMapLoad( niv.mapName );
	}
}

static void NIV_Cmd_Status( void )
{
	size_t voxBytes = (size_t)niv.man.gridX * (size_t)niv.man.gridY * (size_t)niv.man.gridZ * 8;
	ri.Printf( PRINT_ALL,
		"[NIV] active=%d loaded=%d procedural=%d grid=%dx%dx%d ~%zuKB vol weights=%zuB\n",
		R_NIV_Active() ? 1 : 0,
		niv.loaded ? 1 : 0,
		niv.procedural ? 1 : 0,
		niv.man.gridX, niv.man.gridY, niv.man.gridZ,
		voxBytes / 1024,
		(size_t)vk.niv.weights_size );
}

void R_NIV_Init( void )
{
	r_niv = ri.Cvar_Get( "r_niv", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_niv_strength = ri.Cvar_Get( "r_niv_strength", "1", CVAR_ARCHIVE_ND );
	r_niv_scale = ri.Cvar_Get( "r_niv_scale", "1", CVAR_ARCHIVE_ND );
	r_niv_gridX = ri.Cvar_Get( "r_niv_gridX", "32", CVAR_ARCHIVE_ND );
	r_niv_gridY = ri.Cvar_Get( "r_niv_gridY", "16", CVAR_ARCHIVE_ND );
	r_niv_gridZ = ri.Cvar_Get( "r_niv_gridZ", "32", CVAR_ARCHIVE_ND );
	r_niv_featureDim = ri.Cvar_Get( "r_niv_featureDim", "4", CVAR_ARCHIVE_ND );
	r_niv_hiddenDim = ri.Cvar_Get( "r_niv_hiddenDim", "16", CVAR_ARCHIVE_ND );
	r_niv_useGBuffer = ri.Cvar_Get( "r_niv_useGBuffer", "1", CVAR_ARCHIVE_ND );
	r_niv_debug = ri.Cvar_Get( "r_niv_debug", "0", CVAR_ARCHIVE_ND );
	r_niv_skipSky = ri.Cvar_Get( "r_niv_skipSky", "1", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_niv, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_niv_scale, "0.25", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_niv,
		"Neural Irradiance Volume: G-buffer GI from compact 3D neural probe field (0=off, 1=on)." );
	ri.Cvar_SetDescription( r_niv_scale,
		"NIV decode resolution scale (1=full, 0.5=half) for ~1080p target cost." );

	ri.Cmd_AddCommand( "niv_reload", NIV_Cmd_Reload );
	ri.Cmd_AddCommand( "niv_status", NIV_Cmd_Status );

	if ( r_niv->integer ) {
		ri.Printf( PRINT_ALL,
			"[NIV] Neural Irradiance Volume enabled (experimental). See docs/NEURAL_IRRADIANCE_VOLUME.md\n" );
	}
}

void R_NIV_Shutdown( void )
{
	ri.Cmd_RemoveCommand( "niv_reload" );
	ri.Cmd_RemoveCommand( "niv_status" );
	NIV_ClearGpu();
	Com_Memset( &niv, 0, sizeof( niv ) );
}

qboolean R_NIV_Active( void )
{
	return ( r_niv && r_niv->integer && niv.loaded && vk.niv.volume_ready &&
		vk.fboActive && vk.depth_image != VK_NULL_HANDLE ) ? qtrue : qfalse;
}

void R_NIV_OnMapLoad( const char *mapBaseName )
{
	byte *buf;
	int len;
	float *vox;
	int gx, gy, gz, fdim;
	const char *tryPaths[2];
	int i;

	NIV_ClearGpu();
	Com_Memset( &niv, 0, sizeof( niv ) );

	if ( !r_niv || !r_niv->integer ) {
		return;
	}
	if ( !mapBaseName || !mapBaseName[0] ) {
		return;
	}

	Q_strncpyz( niv.mapName, mapBaseName, sizeof( niv.mapName ) );

	tryPaths[0] = va( "maps/%s.niv", mapBaseName );
	tryPaths[1] = va( "niv/%s.niv", mapBaseName );

	for ( i = 0; i < 2; i++ ) {
		len = ri.FS_ReadFile( tryPaths[i], (void **)&buf );
		if ( len > 0 && buf ) {
			if ( NIV_ParseManifest( (const char *)buf, &niv.man ) ) {
				ri.Printf( PRINT_ALL, "[NIV] Loaded manifest %s\n", tryPaths[i] );
			}
			ri.FS_FreeFile( buf );
			break;
		}
	}

	if ( r_niv_gridX && r_niv_gridX->integer > 0 ) {
		niv.man.gridX = r_niv_gridX->integer;
	}
	if ( r_niv_gridY && r_niv_gridY->integer > 0 ) {
		niv.man.gridY = r_niv_gridY->integer;
	}
	if ( r_niv_gridZ && r_niv_gridZ->integer > 0 ) {
		niv.man.gridZ = r_niv_gridZ->integer;
	}
	if ( r_niv_featureDim && r_niv_featureDim->integer > 0 ) {
		niv.man.featureDim = r_niv_featureDim->integer;
	}
	if ( r_niv_hiddenDim && r_niv_hiddenDim->integer > 0 ) {
		niv.man.hiddenDim = r_niv_hiddenDim->integer;
	}

	NIV_ApplyWorldBoundsFromMap();

	{
		char weightsPath[MAX_QPATH];
		if ( niv.man.weightsPath[0] ) {
			Q_strncpyz( weightsPath, niv.man.weightsPath, sizeof( weightsPath ) );
		} else {
			Com_sprintf( weightsPath, sizeof( weightsPath ), "niv/%s.nivb", mapBaseName );
		}
		if ( !vk_neural_load_mlp_rgb( VK_NEURAL_MAGIC_NIV1, weightsPath,
				niv.man.featureDim, niv.man.hiddenDim,
				niv.W1, NIV_MAX_FEATURE_DIM, niv.b1, niv.W2, niv.b2,
				NIV_MAX_FEATURE_DIM, NIV_MAX_HIDDEN ) ) {
			NIV_BuildDefaultWeights();
		}
	}
	NIV_UploadWeightsBuffer();

	gx = niv.man.gridX;
	gy = niv.man.gridY;
	gz = niv.man.gridZ;
	fdim = niv.man.featureDim;
	vox = (float *)ri.Malloc( (size_t)( gx * gy * gz * fdim ) * sizeof( float ) );
	Com_Memset( vox, 0, (size_t)( gx * gy * gz * fdim ) * sizeof( float ) );
	niv.procedural = qtrue;
	if ( niv.man.volumePath[0] ) {
		int lx = gx, ly = gy, lz = gz, lfd = fdim;
		size_t cap = (size_t)gx * (size_t)gy * (size_t)gz * (size_t)fdim;

		if ( vk_neural_load_volume_f32( VK_NEURAL_MAGIC_NIV2, niv.man.volumePath,
				&lx, &ly, &lz, &lfd, vox, cap ) ) {
			gx = lx;
			gy = ly;
			gz = lz;
			fdim = lfd;
			niv.man.gridX = gx;
			niv.man.gridY = gy;
			niv.man.gridZ = gz;
			niv.man.featureDim = fdim;
			niv.procedural = qfalse;
		}
	}
	if ( niv.procedural ) {
		NIV_FillProceduralVolume( vox, gx, gy, gz, fdim );
	}

	if ( !NIV_UploadFeatureVolume3D( vox, gx, gy, gz, fdim ) ) {
		ri.Printf( PRINT_WARNING, "[NIV] Failed to upload feature volume\n" );
		ri.Free( vox );
		return;
	}
	ri.Free( vox );

	niv.loaded = qtrue;
	vk.nivAllocated = qtrue;
	ri.Printf( PRINT_ALL,
		"[NIV] Ready on '%s': grid %dx%dx%d featureDim=%d bounds [%.0f,%.0f,%.0f]-[%.0f,%.0f,%.0f]\n",
		mapBaseName, gx, gy, gz, fdim,
		niv.man.worldMin[0], niv.man.worldMin[1], niv.man.worldMin[2],
		niv.man.worldMax[0], niv.man.worldMax[1], niv.man.worldMax[2] );
}

void vk_niv_apply_after_geometry( void )
{
	uint32_t fullW, fullH;
	uint32_t width, height;
	VkImageView depthView;
	VkImageView normalView;
	VkImageAspectFlags depth_aspect;
	vk_niv_shade_push_t shadePush;
	vk_niv_composite_push_t compPush;
	float scale;
	uint32_t gx, gy;
	qboolean useGbuf;
	VkImageView albedoView;

	if ( !R_NIV_Active() ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk.inRenderPass ) {
		return;
	}
	if ( vk.color_image == VK_NULL_HANDLE || vk.depth_image == VK_NULL_HANDLE ) {
		return;
	}

	fullW = vk_get_render_target_width();
	fullH = vk_get_render_target_height();
	if ( fullW == 0 || fullH == 0 ) {
		return;
	}

	scale = r_niv_scale ? r_niv_scale->value : 1.0f;
	if ( scale < 0.25f ) {
		scale = 0.25f;
	}
	if ( scale > 1.0f ) {
		scale = 1.0f;
	}
	width = (uint32_t)( (float)fullW * scale );
	height = (uint32_t)( (float)fullH * scale );
	if ( width < 8 ) {
		width = 8;
	}
	if ( height < 8 ) {
		height = 8;
	}

	if ( !NIV_EnsureIrradianceTarget( width, height ) ) {
		return;
	}

	NIV_CreateShadePipeline();
	NIV_CreateCompositePipeline();
	if ( !vk.niv.shade_ready || !vk.niv.composite_ready ) {
		return;
	}

	depthView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	useGbuf = ( r_niv_useGBuffer && r_niv_useGBuffer->integer &&
		vk.deferred_gbuffer_normal_view != VK_NULL_HANDLE &&
		vk_deferred_gbuffer_fill_wanted() ) ? qtrue : qfalse;
	normalView = useGbuf ? vk.deferred_gbuffer_normal_view :
		( tr.whiteImage ? tr.whiteImage->view : VK_NULL_HANDLE );
	albedoView = ( vk.deferred_gbuffer_albedo_view != VK_NULL_HANDLE && useGbuf ) ?
		vk.deferred_gbuffer_albedo_view : vk.color_image_view;

	NIV_UpdateShadeDescriptors( depthView, normalView );

	depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	record_image_layout_transition( vk.cmd->command_buffer, vk.niv.irradiance_image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );

	Com_Memset( &shadePush, 0, sizeof( shadePush ) );
	NIV_FillInvViewProj( shadePush.invViewProj );
	shadePush.worldMin[0] = niv.man.worldMin[0];
	shadePush.worldMin[1] = niv.man.worldMin[1];
	shadePush.worldMin[2] = niv.man.worldMin[2];
	shadePush.worldMin[3] = 0.0f;
	shadePush.worldMax[0] = niv.man.worldMax[0];
	shadePush.worldMax[1] = niv.man.worldMax[1];
	shadePush.worldMax[2] = niv.man.worldMax[2];
	shadePush.worldMax[3] = 0.0f;
	shadePush.gridDim[0] = (uint32_t)niv.man.gridX;
	shadePush.gridDim[1] = (uint32_t)niv.man.gridY;
	shadePush.gridDim[2] = (uint32_t)niv.man.gridZ;
	shadePush.featureDim = (uint32_t)niv.man.featureDim;
	shadePush.hiddenDim = (uint32_t)niv.man.hiddenDim;
	shadePush.useGBufferNormal = useGbuf ? 1u : 0u;
	shadePush.hasGBuffer = useGbuf ? 1u : 0u;
	shadePush.extent[0] = width;
	shadePush.extent[1] = height;
	shadePush.strength = r_niv_strength ? r_niv_strength->value : 1.0f;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.niv.shade_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.niv.shade_pipeline_layout, 0, 1, &vk.niv.shade_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.niv.shade_pipeline_layout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( shadePush ), &shadePush );
	gx = ( width + 7u ) / 8u;
	gy = ( height + 7u ) / 8u;
	qvkCmdDispatch( vk.cmd->command_buffer, gx, gy, 1 );

	record_image_layout_transition( vk.cmd->command_buffer, vk.niv.irradiance_image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );

	/* Composite at full resolution (upsample irradiance) */
	{
		VkDescriptorPoolSize pool_sizes[2];
		VkDescriptorPoolCreateInfo pool_ci;
		VkDescriptorSetAllocateInfo alloc_ci;
		VkDescriptorImageInfo img_infos[4];
		VkWriteDescriptorSet writes[4];
		if ( vk.niv.composite_pool != VK_NULL_HANDLE ) {
			qvkDestroyDescriptorPool( vk.device, vk.niv.composite_pool, NULL );
		}
		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[0].descriptorCount = 3;
		pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		pool_sizes[1].descriptorCount = 1;
		Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
		pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_ci.maxSets = 1;
		pool_ci.poolSizeCount = 2;
		pool_ci.pPoolSizes = pool_sizes;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.niv.composite_pool ) );
		Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
		alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_ci.descriptorPool = vk.niv.composite_pool;
		alloc_ci.descriptorSetCount = 1;
		alloc_ci.pSetLayouts = &vk.niv.composite_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &vk.niv.composite_descriptor ) );

		Com_Memset( img_infos, 0, sizeof( img_infos ) );
		img_infos[0].sampler = NIV_DepthSampler();
		img_infos[0].imageView = depthView;
		img_infos[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		img_infos[1].sampler = NIV_LinearSampler();
		img_infos[1].imageView = albedoView;
		img_infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		img_infos[2].sampler = NIV_LinearSampler();
		img_infos[2].imageView = vk.niv.irradiance_view;
		img_infos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		img_infos[3].imageView = vk.color_image_view;
		img_infos[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( writes, 0, sizeof( writes ) );
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = vk.niv.composite_descriptor;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].pImageInfo = &img_infos[0];
		writes[1] = writes[0];
		writes[1].dstBinding = 1;
		writes[1].pImageInfo = &img_infos[1];
		writes[2] = writes[0];
		writes[2].dstBinding = 2;
		writes[2].pImageInfo = &img_infos[2];
		writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].dstSet = vk.niv.composite_descriptor;
		writes[3].dstBinding = 3;
		writes[3].descriptorCount = 1;
		writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[3].pImageInfo = &img_infos[3];
		qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );
	}

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );

	Com_Memset( &compPush, 0, sizeof( compPush ) );
	compPush.extent[0] = fullW;
	compPush.extent[1] = fullH;
	compPush.strength = 1.0f;
	compPush.skipSky = ( r_niv_skipSky && r_niv_skipSky->integer ) ? 1u : 0u;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.niv.composite_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.niv.composite_pipeline_layout, 0, 1, &vk.niv.composite_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.niv.composite_pipeline_layout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( compPush ), &compPush );
	gx = ( fullW + 7u ) / 8u;
	gy = ( fullH + 7u ) / 8u;
	qvkCmdDispatch( vk.cmd->command_buffer, gx, gy, 1 );

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );

	if ( r_niv_debug && r_niv_debug->integer ) {
		ri.Printf( PRINT_DEVELOPER, "[NIV] shade %ux%u composite %ux%u\n", width, height, fullW, fullH );
	}
}
