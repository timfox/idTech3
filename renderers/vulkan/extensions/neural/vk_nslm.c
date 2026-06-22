/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Neural Six-way Lightmaps — volumetric fog/smoke/dust via froxel modulation.
See docs/NEURAL_SIXWAY_LIGHTMAPS.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_nslm.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_volumetric_params.h"
#include "vk_staging.h"
#include "vk_cmd.h"
#include "vk_neural_io.h"

#define NSLM_MANIFEST_VERSION    1
#define NSLM_MAGIC_WEIGHTS       0x314C534E /* NSL1' */
#define NSLM_MAX_GRID_X          64
#define NSLM_MAX_GRID_Y          32
#define NSLM_MAX_GRID_Z          64
#define NSLM_MAX_FEATURE_DIM     4
#define NSLM_MAX_HIDDEN          32

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
} nslmManifest_t;

typedef struct {
	qboolean    loaded;
	qboolean    procedural;
	char        mapName[MAX_QPATH];
	nslmManifest_t man;
	float       sixWaySharpness;

	float       W1[NSLM_MAX_HIDDEN * NSLM_MAX_FEATURE_DIM];
	float       b1[NSLM_MAX_HIDDEN];
	float       W2[3 * NSLM_MAX_HIDDEN];
	float       b2[3];

} nslmState_t;

static nslmState_t nslm;

static VkSampler NSLM_LinearSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	return vk_find_sampler( &sd );
}

static cvar_t *r_nslm;
static cvar_t *r_nslm_strength;
static cvar_t *r_nslm_gridX;
static cvar_t *r_nslm_gridY;
static cvar_t *r_nslm_gridZ;
static cvar_t *r_nslm_featureDim;
static cvar_t *r_nslm_hiddenDim;
static cvar_t *r_nslm_debug;
static cvar_t *r_nslm_sixWaySharpness;

typedef struct {
	vec4_t worldMin;
	vec4_t worldMax;
	vec4_t froxelDim;
	vec4_t featureHidden;
	vec4_t viewOrigin;
	vec4_t sunDirection;
	vec4_t modParams;
} vk_nslm_froxel_push_t;

static void NSLM_ClearGpu( void )
{
	if ( vk.nslm.froxel_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.nslm.froxel_pipeline, NULL );
		vk.nslm.froxel_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.nslm.froxel_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.nslm.froxel_pipeline_layout, NULL );
		vk.nslm.froxel_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.nslm.froxel_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.nslm.froxel_layout, NULL );
		vk.nslm.froxel_layout = VK_NULL_HANDLE;
	}
	if ( vk.nslm.froxel_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.nslm.froxel_pool, NULL );
		vk.nslm.froxel_pool = VK_NULL_HANDLE;
	}
	if ( vk.nslm.weights_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.nslm.weights_buffer, NULL );
		vk.nslm.weights_buffer = VK_NULL_HANDLE;
	}
	if ( vk.nslm.weights_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.nslm.weights_memory, NULL );
		vk.nslm.weights_memory = VK_NULL_HANDLE;
	}
	if ( vk.nslm.feature_volume != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.nslm.feature_volume, NULL );
		vk.nslm.feature_volume = VK_NULL_HANDLE;
	}
	if ( vk.nslm.feature_volume_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.nslm.feature_volume_view, NULL );
		vk.nslm.feature_volume_view = VK_NULL_HANDLE;
	}
	if ( vk.nslm.feature_volume_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.nslm.feature_volume_memory, NULL );
		vk.nslm.feature_volume_memory = VK_NULL_HANDLE;
	}
	vk.nslm.froxel_descriptor = VK_NULL_HANDLE;
	vk.nslm.froxel_ready = qfalse;
	vk.nslm.volume_ready = qfalse;
	vk.nslmAllocated = qfalse;
}

static void NSLM_BuildDefaultWeights( void )
{
	int h = nslm.man.hiddenDim;
	int f = nslm.man.featureDim;
	int i, c;

	Com_Memset( nslm.W1, 0, sizeof( nslm.W1 ) );
	Com_Memset( nslm.b1, 0, sizeof( nslm.b1 ) );
	Com_Memset( nslm.W2, 0, sizeof( nslm.W2 ) );
	Com_Memset( nslm.b2, 0, sizeof( nslm.b2 ) );

	for ( i = 0; i < h; i++ ) {
		nslm.b1[i] = -0.05f;
		for ( int j = 0; j < f && j < 3; j++ ) {
			nslm.W1[i * NSLM_MAX_FEATURE_DIM + j] = ( i == j ) ? 0.85f : 0.04f;
		}
	}
	for ( c = 0; c < 3; c++ ) {
		for ( i = 0; i < h; i++ ) {
			nslm.W2[c * NSLM_MAX_HIDDEN + i] = ( i % 3 == c ) ? 0.75f : 0.03f;
		}
		nslm.b2[c] = 0.1f;
	}
}

static qboolean NSLM_ParseManifest( const char *text, nslmManifest_t *man )
{
	const char *parse;
	const char *token;
	char key[64];
	char value[MAX_TOKEN_CHARS];

	Com_Memset( man, 0, sizeof( *man ) );
	man->version = NSLM_MANIFEST_VERSION;
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
	if ( man->gridX > NSLM_MAX_GRID_X || man->gridY > NSLM_MAX_GRID_Y || man->gridZ > NSLM_MAX_GRID_Z ) {
		return qfalse;
	}
	if ( man->featureDim < 1 || man->featureDim > NSLM_MAX_FEATURE_DIM ) {
		return qfalse;
	}
	if ( man->hiddenDim < 1 || man->hiddenDim > NSLM_MAX_HIDDEN ) {
		return qfalse;
	}
	return qtrue;
}

static void NSLM_ApplyWorldBoundsFromMap( void )
{
	if ( !tr.world ) {
		return;
	}
	nslm.man.worldMin[0] = tr.world->lightGridOrigin[0];
	nslm.man.worldMin[1] = tr.world->lightGridOrigin[1];
	nslm.man.worldMin[2] = tr.world->lightGridOrigin[2];
	nslm.man.worldMax[0] = tr.world->lightGridOrigin[0] +
		tr.world->lightGridSize[0] * tr.world->lightGridBounds[0];
	nslm.man.worldMax[1] = tr.world->lightGridOrigin[1] +
		tr.world->lightGridSize[1] * tr.world->lightGridBounds[1];
	nslm.man.worldMax[2] = tr.world->lightGridOrigin[2] +
		tr.world->lightGridSize[2] * tr.world->lightGridBounds[2];
}

static void NSLM_FillProceduralVolume( float *vox, int gx, int gy, int gz, int fdim )
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

static qboolean NSLM_UploadFeatureVolume3D( const float *vox, int gx, int gy, int gz, int fdim )
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

	if ( vk.nslm.feature_volume != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.nslm.feature_volume_view, NULL );
		qvkDestroyImage( vk.device, vk.nslm.feature_volume, NULL );
		qvkFreeMemory( vk.device, vk.nslm.feature_volume_memory, NULL );
		vk.nslm.feature_volume = VK_NULL_HANDLE;
		vk.nslm.feature_volume_view = VK_NULL_HANDLE;
		vk.nslm.feature_volume_memory = VK_NULL_HANDLE;
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

	if ( qvkCreateImage( vk.device, &image_desc, NULL, &vk.nslm.feature_volume ) != VK_SUCCESS ) {
		ri.Free( packed );
		return qfalse;
	}

	qvkGetImageMemoryRequirements( vk.device, vk.nslm.feature_volume, &mem_req );
	Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.nslm.feature_volume_memory ) != VK_SUCCESS ||
		qvkBindImageMemory( vk.device, vk.nslm.feature_volume, vk.nslm.feature_volume_memory, 0 ) != VK_SUCCESS ) {
		qvkDestroyImage( vk.device, vk.nslm.feature_volume, NULL );
		ri.Free( packed );
		return qfalse;
	}

	Com_Memset( &view_desc, 0, sizeof( view_desc ) );
	view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_desc.image = vk.nslm.feature_volume;
	view_desc.viewType = VK_IMAGE_VIEW_TYPE_3D;
	view_desc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_desc.subresourceRange.levelCount = 1;
	view_desc.subresourceRange.layerCount = 1;
	if ( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.nslm.feature_volume_view ) != VK_SUCCESS ) {
		NSLM_ClearGpu();
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
	record_image_layout_transition( cmd, vk.nslm.feature_volume, VK_IMAGE_ASPECT_COLOR_BIT,
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

	qvkCmdCopyBufferToImage( cmd, vk.staging_buffer.handle, vk.nslm.feature_volume,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

	record_image_layout_transition( cmd, vk.nslm.feature_volume, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	vk_end_command_buffer( cmd, __func__ );
	vk.nslm.volume_ready = qtrue;
	return qtrue;
}

static qboolean NSLM_UploadWeightsBuffer( void )
{
	int h = nslm.man.hiddenDim;
	int f = nslm.man.featureDim;
	VkDeviceSize size;
	float *cpu;
	VkBufferCreateInfo buf_ci;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_ci;
	void *mapped;

	size = (VkDeviceSize)( ( h * f ) + h + ( 3 * h ) + 3 ) * sizeof( float );
	cpu = (float *)ri.Malloc( (size_t)size );
	Com_Memcpy( cpu, nslm.W1, h * f * sizeof( float ) );
	Com_Memcpy( cpu + h * f, nslm.b1, h * sizeof( float ) );
	Com_Memcpy( cpu + h * f + h, nslm.W2, 3 * h * sizeof( float ) );
	Com_Memcpy( cpu + h * f + h + 3 * h, nslm.b2, 3 * sizeof( float ) );

	if ( vk.nslm.weights_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.nslm.weights_buffer, NULL );
		qvkFreeMemory( vk.device, vk.nslm.weights_memory, NULL );
	}

	Com_Memset( &buf_ci, 0, sizeof( buf_ci ) );
	buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_ci.size = size;
	buf_ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_ci, NULL, &vk.nslm.weights_buffer ) );

	qvkGetBufferMemoryRequirements( vk.device, vk.nslm.weights_buffer, &mem_req );
	Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
	alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_ci.allocationSize = mem_req.size;
	alloc_ci.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_ci, NULL, &vk.nslm.weights_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.nslm.weights_buffer, vk.nslm.weights_memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.nslm.weights_memory, 0, size, 0, &mapped ) );
	Com_Memcpy( mapped, cpu, (size_t)size );
	qvkUnmapMemory( vk.device, vk.nslm.weights_memory );
	ri.Free( cpu );
	vk.nslm.weights_size = size;
	return qtrue;
}

static void NSLM_CreateFroxelPipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[3];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.nslm.froxel_ready ) {
		return;
	}
	if ( vk.modules.nslm_froxel_cs == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 3;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.nslm.froxel_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_nslm_froxel_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.nslm.froxel_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.nslm.froxel_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.nslm_froxel_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.nslm.froxel_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.nslm.froxel_pipeline ) );
	vk.nslm.froxel_ready = qtrue;
}

static void NSLM_UpdateFroxelDescriptors( void )
{
	VkDescriptorPoolSize pool_sizes[3];
	VkDescriptorPoolCreateInfo pool_ci;
	VkDescriptorSetAllocateInfo alloc_ci;
	VkDescriptorImageInfo img_infos[2];
	VkDescriptorBufferInfo buf_info;
	VkWriteDescriptorSet writes[3];

	if ( vk.froxel_volume_view == VK_NULL_HANDLE || vk.nslm.feature_volume_view == VK_NULL_HANDLE ||
		vk.nslm.weights_buffer == VK_NULL_HANDLE ) {
		return;
	}

	if ( vk.nslm.froxel_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.nslm.froxel_pool, NULL );
		vk.nslm.froxel_pool = VK_NULL_HANDLE;
		vk.nslm.froxel_descriptor = VK_NULL_HANDLE;
	}

	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[0].descriptorCount = 1;
	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	pool_sizes[1].descriptorCount = 1;
	pool_sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	pool_sizes[2].descriptorCount = 1;
	Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
	pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_ci.maxSets = 1;
	pool_ci.poolSizeCount = 3;
	pool_ci.pPoolSizes = pool_sizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.nslm.froxel_pool ) );

	Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
	alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_ci.descriptorPool = vk.nslm.froxel_pool;
	alloc_ci.descriptorSetCount = 1;
	alloc_ci.pSetLayouts = &vk.nslm.froxel_layout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &vk.nslm.froxel_descriptor ) );

	Com_Memset( img_infos, 0, sizeof( img_infos ) );
	img_infos[0].sampler = NSLM_LinearSampler();
	img_infos[0].imageView = vk.nslm.feature_volume_view;
	img_infos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	buf_info.buffer = vk.nslm.weights_buffer;
	buf_info.offset = 0;
	buf_info.range = vk.nslm.weights_size;

	img_infos[1].imageView = vk.froxel_volume_view;
	img_infos[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.nslm.froxel_descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &img_infos[0];
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vk.nslm.froxel_descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].pBufferInfo = &buf_info;
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = vk.nslm.froxel_descriptor;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[2].pImageInfo = &img_infos[1];
	qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );
}

static void NSLM_Cmd_Reload( void )
{
	if ( nslm.mapName[0] ) {
		R_NSLM_OnMapLoad( nslm.mapName );
	}
}

static void NSLM_Cmd_Status( void )
{
	size_t voxBytes = (size_t)nslm.man.gridX * (size_t)nslm.man.gridY * (size_t)nslm.man.gridZ * 8;
	ri.Printf( PRINT_ALL,
		"[NSLM] active=%d loaded=%d procedural=%d grid=%dx%dx%d ~%zuKB vol weights=%zuB\n",
		R_NSLM_Active() ? 1 : 0,
		nslm.loaded ? 1 : 0,
		nslm.procedural ? 1 : 0,
		nslm.man.gridX, nslm.man.gridY, nslm.man.gridZ,
		voxBytes / 1024,
		(size_t)vk.nslm.weights_size );
}

void R_NSLM_Init( void )
{
	r_nslm = ri.Cvar_Get( "r_nslm", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_nslm_strength = ri.Cvar_Get( "r_nslm_strength", "1", CVAR_ARCHIVE_ND );
	r_nslm_gridX = ri.Cvar_Get( "r_nslm_gridX", "32", CVAR_ARCHIVE_ND );
	r_nslm_gridY = ri.Cvar_Get( "r_nslm_gridY", "16", CVAR_ARCHIVE_ND );
	r_nslm_gridZ = ri.Cvar_Get( "r_nslm_gridZ", "32", CVAR_ARCHIVE_ND );
	r_nslm_featureDim = ri.Cvar_Get( "r_nslm_featureDim", "4", CVAR_ARCHIVE_ND );
	r_nslm_hiddenDim = ri.Cvar_Get( "r_nslm_hiddenDim", "16", CVAR_ARCHIVE_ND );
	r_nslm_debug = ri.Cvar_Get( "r_nslm_debug", "0", CVAR_ARCHIVE_ND );
	r_nslm_sixWaySharpness = ri.Cvar_Get( "r_nslm_sixWaySharpness", "2", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_nslm, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_nslm_sixWaySharpness, "0.5", "8", CV_FLOAT );
	ri.Cvar_SetDescription( r_nslm,
		"Neural Six-way Lightmaps: volumetric fog/smoke GI in froxels (requires r_volumetricFog 1)." );
	ri.Cvar_SetDescription( r_nslm_sixWaySharpness,
		"Six-way basis exponent from view direction (higher = sharper axis lobes)." );

	ri.Cmd_AddCommand( "nslm_reload", NSLM_Cmd_Reload );
	ri.Cmd_AddCommand( "nslm_status", NSLM_Cmd_Status );

	if ( r_nslm->integer ) {
		ri.Printf( PRINT_ALL,
			"[NSLM] Neural Six-way Lightmaps enabled (experimental). See docs/NEURAL_SIXWAY_LIGHTMAPS.md\n" );
	}
}

void R_NSLM_Shutdown( void )
{
	ri.Cmd_RemoveCommand( "nslm_reload" );
	ri.Cmd_RemoveCommand( "nslm_status" );
	NSLM_ClearGpu();
	Com_Memset( &nslm, 0, sizeof( nslm ) );
}

qboolean R_NSLM_Active( void )
{
	return ( r_nslm && r_nslm->integer && r_volumetricFog && r_volumetricFog->integer &&
		nslm.loaded && vk.nslm.volume_ready && vk.froxel_volume_image != VK_NULL_HANDLE &&
		vk.froxel_volume_view != VK_NULL_HANDLE && vk.froxel_width > 0 &&
		vk.froxel_height > 0 && vk.froxel_slices > 0 ) ? qtrue : qfalse;
}

void R_NSLM_OnMapLoad( const char *mapBaseName )
{
	byte *buf;
	int len;
	float *vox;
	int gx, gy, gz, fdim;
	const char *tryPaths[2];
	int i;

	NSLM_ClearGpu();
	Com_Memset( &nslm, 0, sizeof( nslm ) );

	if ( !r_nslm || !r_nslm->integer ) {
		return;
	}
	if ( !mapBaseName || !mapBaseName[0] ) {
		return;
	}

	Q_strncpyz( nslm.mapName, mapBaseName, sizeof( nslm.mapName ) );

	tryPaths[0] = va( "maps/%s.nslm", mapBaseName );
	tryPaths[1] = va( "nslm/%s.nslm", mapBaseName );

	for ( i = 0; i < 2; i++ ) {
		len = ri.FS_ReadFile( tryPaths[i], (void **)&buf );
		if ( len > 0 && buf ) {
			if ( NSLM_ParseManifest( (const char *)buf, &nslm.man ) ) {
				ri.Printf( PRINT_ALL, "[NSLM] Loaded manifest %s\n", tryPaths[i] );
			}
			ri.FS_FreeFile( buf );
			break;
		}
	}

	if ( r_nslm_gridX && r_nslm_gridX->integer > 0 ) {
		nslm.man.gridX = r_nslm_gridX->integer;
	}
	if ( r_nslm_gridY && r_nslm_gridY->integer > 0 ) {
		nslm.man.gridY = r_nslm_gridY->integer;
	}
	if ( r_nslm_gridZ && r_nslm_gridZ->integer > 0 ) {
		nslm.man.gridZ = r_nslm_gridZ->integer;
	}
	if ( r_nslm_featureDim && r_nslm_featureDim->integer > 0 ) {
		nslm.man.featureDim = r_nslm_featureDim->integer;
	}
	if ( r_nslm_hiddenDim && r_nslm_hiddenDim->integer > 0 ) {
		nslm.man.hiddenDim = r_nslm_hiddenDim->integer;
	}

	NSLM_ApplyWorldBoundsFromMap();

	{
		char weightsPath[MAX_QPATH];
		if ( nslm.man.weightsPath[0] ) {
			Q_strncpyz( weightsPath, nslm.man.weightsPath, sizeof( weightsPath ) );
		} else {
			Com_sprintf( weightsPath, sizeof( weightsPath ), "nslm/%s.nslb", mapBaseName );
		}
		if ( !vk_neural_load_mlp_rgb( VK_NEURAL_MAGIC_NSL1, weightsPath,
				nslm.man.featureDim, nslm.man.hiddenDim,
				nslm.W1, NSLM_MAX_FEATURE_DIM, nslm.b1, nslm.W2, nslm.b2,
				NSLM_MAX_FEATURE_DIM, NSLM_MAX_HIDDEN ) ) {
			NSLM_BuildDefaultWeights();
		}
	}
	NSLM_UploadWeightsBuffer();

	gx = nslm.man.gridX;
	gy = nslm.man.gridY;
	gz = nslm.man.gridZ;
	fdim = nslm.man.featureDim;
	vox = (float *)ri.Malloc( (size_t)( gx * gy * gz * fdim ) * sizeof( float ) );
	Com_Memset( vox, 0, (size_t)( gx * gy * gz * fdim ) * sizeof( float ) );
	nslm.procedural = qtrue;
	if ( nslm.man.volumePath[0] ) {
		int lx = gx, ly = gy, lz = gz, lfd = fdim;
		size_t cap = (size_t)gx * (size_t)gy * (size_t)gz * (size_t)fdim;

		if ( vk_neural_load_volume_f32( VK_NEURAL_MAGIC_NSL2, nslm.man.volumePath,
				&lx, &ly, &lz, &lfd, vox, cap ) ) {
			gx = lx;
			gy = ly;
			gz = lz;
			fdim = lfd;
			nslm.man.gridX = gx;
			nslm.man.gridY = gy;
			nslm.man.gridZ = gz;
			nslm.man.featureDim = fdim;
			nslm.procedural = qfalse;
		}
	}
	if ( nslm.procedural ) {
		NSLM_FillProceduralVolume( vox, gx, gy, gz, fdim );
	}

	if ( !NSLM_UploadFeatureVolume3D( vox, gx, gy, gz, fdim ) ) {
		ri.Printf( PRINT_WARNING, "[NSLM] Failed to upload feature volume\n" );
		ri.Free( vox );
		return;
	}
	ri.Free( vox );

	if ( r_nslm_sixWaySharpness ) {
		nslm.sixWaySharpness = r_nslm_sixWaySharpness->value;
	}

	nslm.loaded = qtrue;
	vk.nslmAllocated = qtrue;
	ri.Printf( PRINT_ALL,
		"[NSLM] Ready on '%s': grid %dx%dx%d featureDim=%d bounds [%.0f,%.0f,%.0f]-[%.0f,%.0f,%.0f]\n",
		mapBaseName, gx, gy, gz, fdim,
		nslm.man.worldMin[0], nslm.man.worldMin[1], nslm.man.worldMin[2],
		nslm.man.worldMax[0], nslm.man.worldMax[1], nslm.man.worldMax[2] );
}

void vk_nslm_apply_to_froxels( uint32_t groups_x, uint32_t groups_y, uint32_t groups_z )
{
	vk_nslm_froxel_push_t push;
	const volumetric_params_t *vparams;

	if ( !R_NSLM_Active() ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( groups_x == 0 || groups_y == 0 || groups_z == 0 ) {
		return;
	}

	NSLM_CreateFroxelPipeline();
	if ( !vk.nslm.froxel_ready ) {
		return;
	}

	NSLM_UpdateFroxelDescriptors();
	if ( vk.nslm.froxel_descriptor == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &push, 0, sizeof( push ) );
	push.worldMin[0] = nslm.man.worldMin[0];
	push.worldMin[1] = nslm.man.worldMin[1];
	push.worldMin[2] = nslm.man.worldMin[2];
	push.worldMax[0] = nslm.man.worldMax[0];
	push.worldMax[1] = nslm.man.worldMax[1];
	push.worldMax[2] = nslm.man.worldMax[2];
	push.froxelDim[0] = (float)vk.froxel_width;
	push.froxelDim[1] = (float)vk.froxel_height;
	push.froxelDim[2] = (float)vk.froxel_slices;
	push.featureHidden[0] = (float)nslm.man.featureDim;
	push.featureHidden[1] = (float)nslm.man.hiddenDim;
	push.modParams[0] = r_nslm_strength ? r_nslm_strength->value : 1.0f;
	push.modParams[1] = r_nslm_sixWaySharpness ? r_nslm_sixWaySharpness->value : nslm.sixWaySharpness;
	push.modParams[2] = backEnd.refdef.time * 0.001f;

	vparams = vk.volumetric_params_ptr ? (const volumetric_params_t *)vk.volumetric_params_ptr : NULL;
	if ( vparams ) {
		push.worldMin[0] = vparams->worldMin[0];
		push.worldMin[1] = vparams->worldMin[1];
		push.worldMin[2] = vparams->worldMin[2];
		push.worldMax[0] = vparams->worldMax[0];
		push.worldMax[1] = vparams->worldMax[1];
		push.worldMax[2] = vparams->worldMax[2];
		push.viewOrigin[0] = vparams->viewOrigin[0];
		push.viewOrigin[1] = vparams->viewOrigin[1];
		push.viewOrigin[2] = vparams->viewOrigin[2];
		push.sunDirection[0] = vparams->sunDirection[0];
		push.sunDirection[1] = vparams->sunDirection[1];
		push.sunDirection[2] = vparams->sunDirection[2];
	} else {
		push.viewOrigin[0] = backEnd.viewParms.or.origin[0];
		push.viewOrigin[1] = backEnd.viewParms.or.origin[1];
		push.viewOrigin[2] = backEnd.viewParms.or.origin[2];
		push.sunDirection[0] = tr.sunDirection[0];
		push.sunDirection[1] = tr.sunDirection[1];
		push.sunDirection[2] = tr.sunDirection[2];
	}

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.nslm.froxel_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.nslm.froxel_pipeline_layout, 0, 1, &vk.nslm.froxel_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.nslm.froxel_pipeline_layout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, groups_z );

	if ( r_nslm_debug && r_nslm_debug->integer ) {
		ri.Printf( PRINT_DEVELOPER,
			"[NSLM] froxel %ux%ux%u dispatch %ux%ux%u strength=%.2f\n",
			(uint32_t)push.froxelDim[0], (uint32_t)push.froxelDim[1], (uint32_t)push.froxelDim[2],
			groups_x, groups_y, groups_z, push.modParams[0] );
	}
}
