/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vertex Features Neural GI — per-mesh vertex features + spatial index decode.
See docs/VERTEX_FEATURES_NEURAL_GI.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_vfgi.h"
#include "vk_vfgi_world.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_view_state.h"
#include "vk_deferred_gbuffer.h"
#include "vk_cmd.h"
#include "vk_neural_io.h"

#define VFGI_MANIFEST_VERSION   1
#define VFGI_MAX_FEATURE_DIM    4
#define VFGI_MAX_HIDDEN         32

typedef struct {
	int         version;
	int         featureDim;
	int         hiddenDim;
	int         gridX;
	int         gridY;
	int         gridZ;
	float       quantUnits;
	char        weightsPath[MAX_QPATH];
} vfgiManifest_t;

typedef struct {
	qboolean            loaded;
	char                mapName[MAX_QPATH];
	vfgiManifest_t      man;
	vfgiWorldData_t     world;
	float               W1[VFGI_MAX_HIDDEN * VFGI_MAX_FEATURE_DIM];
	float               b1[VFGI_MAX_HIDDEN];
	float               W2[3 * VFGI_MAX_HIDDEN];
	float               b2[3];
	uint32_t            targetWidth;
	uint32_t            targetHeight;
} vfgiState_t;

static vfgiState_t vfgi;

static cvar_t *r_vfgi;
static cvar_t *r_vfgi_strength;
static cvar_t *r_vfgi_scale;
static cvar_t *r_vfgi_featureDim;
static cvar_t *r_vfgi_hiddenDim;
static cvar_t *r_vfgi_vertCap;
static cvar_t *r_vfgi_quant;
static cvar_t *r_vfgi_gridX;
static cvar_t *r_vfgi_gridY;
static cvar_t *r_vfgi_gridZ;
static cvar_t *r_vfgi_useGBuffer;
static cvar_t *r_vfgi_debug;
static cvar_t *r_vfgi_skipSky;
static cvar_t *r_vfgi_normalAtten;
static cvar_t *r_vfgi_ao;

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
} vk_vfgi_decode_push_t;

typedef struct {
	uint32_t extent[2];
	float strength;
	uint32_t skipSky;
	float normalAtten;
	float aoStrength;
	uint32_t hasNormal;
	uint32_t hasAO;
} vk_vfgi_composite_push_t;

static VkSampler VFGI_DepthSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static VkSampler VFGI_LinearSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	return vk_find_sampler( &sd );
}

static void VFGI_ClearGpu( void )
{
	if ( vk.vfgi.decode_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.vfgi.decode_pipeline, NULL );
		vk.vfgi.decode_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.vfgi.composite_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.vfgi.composite_pipeline, NULL );
		vk.vfgi.composite_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.vfgi.decode_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.vfgi.decode_pipeline_layout, NULL );
		vk.vfgi.decode_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.vfgi.composite_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.vfgi.composite_pipeline_layout, NULL );
		vk.vfgi.composite_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.vfgi.decode_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.vfgi.decode_layout, NULL );
		vk.vfgi.decode_layout = VK_NULL_HANDLE;
	}
	if ( vk.vfgi.composite_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.vfgi.composite_layout, NULL );
		vk.vfgi.composite_layout = VK_NULL_HANDLE;
	}
	if ( vk.vfgi.decode_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.vfgi.decode_pool, NULL );
		vk.vfgi.decode_pool = VK_NULL_HANDLE;
	}
	if ( vk.vfgi.composite_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.vfgi.composite_pool, NULL );
		vk.vfgi.composite_pool = VK_NULL_HANDLE;
	}
	if ( vk.vfgi.vertex_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.vfgi.vertex_buffer, NULL );
		vk.vfgi.vertex_buffer = VK_NULL_HANDLE;
	}
	if ( vk.vfgi.vertex_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.vfgi.vertex_memory, NULL );
		vk.vfgi.vertex_memory = VK_NULL_HANDLE;
	}
	if ( vk.vfgi.grid_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.vfgi.grid_buffer, NULL );
		vk.vfgi.grid_buffer = VK_NULL_HANDLE;
	}
	if ( vk.vfgi.grid_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.vfgi.grid_memory, NULL );
		vk.vfgi.grid_memory = VK_NULL_HANDLE;
	}
	if ( vk.vfgi.weights_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.vfgi.weights_buffer, NULL );
		vk.vfgi.weights_buffer = VK_NULL_HANDLE;
	}
	if ( vk.vfgi.weights_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.vfgi.weights_memory, NULL );
		vk.vfgi.weights_memory = VK_NULL_HANDLE;
	}
	if ( vk.vfgi.irradiance_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.vfgi.irradiance_image, NULL );
		vk.vfgi.irradiance_image = VK_NULL_HANDLE;
	}
	if ( vk.vfgi.irradiance_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.vfgi.irradiance_view, NULL );
		vk.vfgi.irradiance_view = VK_NULL_HANDLE;
	}
	if ( vk.vfgi.irradiance_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.vfgi.irradiance_memory, NULL );
		vk.vfgi.irradiance_memory = VK_NULL_HANDLE;
	}
	vk.vfgi.decode_descriptor = VK_NULL_HANDLE;
	vk.vfgi.composite_descriptor = VK_NULL_HANDLE;
	vk.vfgi.decode_ready = qfalse;
	vk.vfgi.composite_ready = qfalse;
	vk.vfgi.buffers_ready = qfalse;
	vk.vfgiAllocated = qfalse;
}

static qboolean VFGI_ParseManifest( const char *text, vfgiManifest_t *man )
{
	const char *parse;
	const char *token;
	char key[64];
	char value[MAX_TOKEN_CHARS];

	Com_Memset( man, 0, sizeof( *man ) );
	man->version = VFGI_MANIFEST_VERSION;
	man->featureDim = 4;
	man->hiddenDim = 16;
	man->gridX = 48;
	man->gridY = 48;
	man->gridZ = 48;
	man->quantUnits = 8.0f;

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

		if ( !Q_stricmp( key, "featureDim" ) ) {
			man->featureDim = atoi( value );
		} else if ( !Q_stricmp( key, "hiddenDim" ) ) {
			man->hiddenDim = atoi( value );
		} else if ( !Q_stricmp( key, "gridX" ) ) {
			man->gridX = atoi( value );
		} else if ( !Q_stricmp( key, "gridY" ) ) {
			man->gridY = atoi( value );
		} else if ( !Q_stricmp( key, "gridZ" ) ) {
			man->gridZ = atoi( value );
		} else if ( !Q_stricmp( key, "quantUnits" ) ) {
			man->quantUnits = (float)atof( value );
		} else if ( !Q_stricmp( key, "weightsPath" ) ) {
			Q_strncpyz( man->weightsPath, value, sizeof( man->weightsPath ) );
		}
	}

	if ( man->featureDim < 1 || man->featureDim > VFGI_MAX_FEATURE_DIM ) {
		return qfalse;
	}
	if ( man->hiddenDim < 1 || man->hiddenDim > VFGI_MAX_HIDDEN ) {
		return qfalse;
	}
	if ( man->gridX < 4 || man->gridY < 4 || man->gridZ < 4 ) {
		return qfalse;
	}
	return qtrue;
}

static void VFGI_BuildDefaultWeights( void )
{
	int h = vfgi.man.hiddenDim;
	int i, c;

	Com_Memset( vfgi.W1, 0, sizeof( vfgi.W1 ) );
	Com_Memset( vfgi.b1, 0, sizeof( vfgi.b1 ) );
	Com_Memset( vfgi.W2, 0, sizeof( vfgi.W2 ) );
	Com_Memset( vfgi.b2, 0, sizeof( vfgi.b2 ) );

	for ( i = 0; i < h; i++ ) {
		vfgi.b1[i] = -0.04f;
		vfgi.W1[i * VFGI_MAX_FEATURE_DIM + 0] = 0.9f;
		vfgi.W1[i * VFGI_MAX_FEATURE_DIM + 1] = 0.15f;
		vfgi.W1[i * VFGI_MAX_FEATURE_DIM + 2] = 0.15f;
		vfgi.W1[i * VFGI_MAX_FEATURE_DIM + 3] = 0.15f;
	}
	for ( c = 0; c < 3; c++ ) {
		for ( i = 0; i < h; i++ ) {
			vfgi.W2[c * VFGI_MAX_HIDDEN + i] = ( i % 3 == c ) ? 0.7f : 0.04f;
		}
		vfgi.b2[c] = 0.12f;
	}
}

static qboolean VFGI_UploadWeightsBuffer( void )
{
	int h = vfgi.man.hiddenDim;
	int f = vfgi.man.featureDim;
	VkDeviceSize size;
	float *cpu;
	VkBufferCreateInfo buf_ci;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_ci;
	void *mapped;

	size = (VkDeviceSize)( ( h * f ) + h + 3 * h + 3 ) * sizeof( float );
	cpu = (float *)ri.Malloc( (size_t)size );
	Com_Memcpy( cpu, vfgi.W1, (size_t)( h * f ) * sizeof( float ) );
	Com_Memcpy( cpu + h * f, vfgi.b1, (size_t)h * sizeof( float ) );
	Com_Memcpy( cpu + h * f + h, vfgi.W2, (size_t)( 3 * h ) * sizeof( float ) );
	Com_Memcpy( cpu + h * f + h + 3 * h, vfgi.b2, 3 * sizeof( float ) );

	if ( vk.vfgi.weights_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.vfgi.weights_buffer, NULL );
		qvkFreeMemory( vk.device, vk.vfgi.weights_memory, NULL );
	}

	Com_Memset( &buf_ci, 0, sizeof( buf_ci ) );
	buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_ci.size = size;
	buf_ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_ci, NULL, &vk.vfgi.weights_buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.vfgi.weights_buffer, &mem_req );
	Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
	alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_ci.allocationSize = mem_req.size;
	alloc_ci.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_ci, NULL, &vk.vfgi.weights_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.vfgi.weights_buffer, vk.vfgi.weights_memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.vfgi.weights_memory, 0, size, 0, &mapped ) );
	Com_Memcpy( mapped, cpu, (size_t)size );
	qvkUnmapMemory( vk.device, vk.vfgi.weights_memory );
	ri.Free( cpu );
	vk.vfgi.weights_size = size;
	return qtrue;
}

static qboolean VFGI_UploadWorldBuffers( void )
{
	VkDeviceSize vertBytes;
	VkDeviceSize gridBytes;
	VkBufferCreateInfo buf_ci;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_ci;
	void *mapped;

	if ( !vfgi.world.valid || vfgi.world.vertexCount == 0 ) {
		return qfalse;
	}

	vertBytes = (VkDeviceSize)vfgi.world.vertexCount * sizeof( vfgiVertexRecord_t );
	gridBytes = (VkDeviceSize)( vfgi.world.gridX * vfgi.world.gridY * vfgi.world.gridZ ) * sizeof( vfgiGridCell_t );

	if ( vk.vfgi.vertex_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.vfgi.vertex_buffer, NULL );
		qvkFreeMemory( vk.device, vk.vfgi.vertex_memory, NULL );
	}
	if ( vk.vfgi.grid_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.vfgi.grid_buffer, NULL );
		qvkFreeMemory( vk.device, vk.vfgi.grid_memory, NULL );
	}

	Com_Memset( &buf_ci, 0, sizeof( buf_ci ) );
	buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_ci.size = vertBytes;
	buf_ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_ci, NULL, &vk.vfgi.vertex_buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.vfgi.vertex_buffer, &mem_req );
	Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
	alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_ci.allocationSize = mem_req.size;
	alloc_ci.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_ci, NULL, &vk.vfgi.vertex_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.vfgi.vertex_buffer, vk.vfgi.vertex_memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.vfgi.vertex_memory, 0, vertBytes, 0, &mapped ) );
	Com_Memcpy( mapped, vfgi.world.vertices, (size_t)vertBytes );
	qvkUnmapMemory( vk.device, vk.vfgi.vertex_memory );
	vk.vfgi.vertex_count = vfgi.world.vertexCount;

	buf_ci.size = gridBytes;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_ci, NULL, &vk.vfgi.grid_buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.vfgi.grid_buffer, &mem_req );
	alloc_ci.allocationSize = mem_req.size;
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_ci, NULL, &vk.vfgi.grid_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.vfgi.grid_buffer, vk.vfgi.grid_memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.vfgi.grid_memory, 0, gridBytes, 0, &mapped ) );
	Com_Memcpy( mapped, vfgi.world.cells, (size_t)gridBytes );
	qvkUnmapMemory( vk.device, vk.vfgi.grid_memory );

	vk.vfgi.buffers_ready = qtrue;
	return qtrue;
}

static qboolean VFGI_EnsureIrradianceTarget( uint32_t width, uint32_t height )
{
	if ( vk.vfgi.irradiance_image != VK_NULL_HANDLE &&
		vfgi.targetWidth == width && vfgi.targetHeight == height ) {
		return qtrue;
	}

	if ( vk.vfgi.irradiance_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.vfgi.irradiance_view, NULL );
		vk.vfgi.irradiance_view = VK_NULL_HANDLE;
	}
	if ( vk.vfgi.irradiance_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.vfgi.irradiance_image, NULL );
		qvkFreeMemory( vk.device, vk.vfgi.irradiance_memory, NULL );
		vk.vfgi.irradiance_image = VK_NULL_HANDLE;
		vk.vfgi.irradiance_memory = VK_NULL_HANDLE;
	}

	{
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
		image_desc.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		image_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if ( qvkCreateImage( vk.device, &image_desc, NULL, &vk.vfgi.irradiance_image ) != VK_SUCCESS ) {
			return qfalse;
		}
		qvkGetImageMemoryRequirements( vk.device, vk.vfgi.irradiance_image, &mem_req );
		Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		if ( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.vfgi.irradiance_memory ) != VK_SUCCESS ||
			qvkBindImageMemory( vk.device, vk.vfgi.irradiance_image, vk.vfgi.irradiance_memory, 0 ) != VK_SUCCESS ) {
			return qfalse;
		}
		Com_Memset( &view_desc, 0, sizeof( view_desc ) );
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.image = vk.vfgi.irradiance_image;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_desc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.layerCount = 1;
		if ( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.vfgi.irradiance_view ) != VK_SUCCESS ) {
			return qfalse;
		}
	}

	vfgi.targetWidth = width;
	vfgi.targetHeight = height;
	return qtrue;
}

static void VFGI_CreateDecodePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[6];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.vfgi.decode_ready ) {
		return;
	}
	if ( vk.modules.vfgi_decode_cs == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[4].binding = 4;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[5].binding = 5;
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[5].descriptorCount = 1;
	bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 6;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.vfgi.decode_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_vfgi_decode_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.vfgi.decode_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.vfgi.decode_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.vfgi_decode_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.vfgi.decode_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.vfgi.decode_pipeline ) );
	vk.vfgi.decode_ready = qtrue;
}

static void VFGI_CreateCompositePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[6];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.vfgi.composite_ready ) {
		return;
	}
	if ( vk.modules.vfgi_composite_cs == VK_NULL_HANDLE ) {
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
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[4].binding = 4;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[5].binding = 5;
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[5].descriptorCount = 1;
	bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 6;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.vfgi.composite_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_vfgi_composite_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.vfgi.composite_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.vfgi.composite_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.vfgi_composite_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.vfgi.composite_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.vfgi.composite_pipeline ) );
	vk.vfgi.composite_ready = qtrue;
}

static void VFGI_FillInvViewProj( float *out16 )
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

static void VFGI_Cmd_Reload( void )
{
	if ( vfgi.mapName[0] ) {
		R_VFGI_OnMapLoad( vfgi.mapName );
	}
}

static void VFGI_Cmd_Status( void )
{
	size_t vertKb = vfgi.world.valid ?
		(size_t)( vfgi.world.vertexCount * sizeof( vfgiVertexRecord_t ) ) / 1024 : 0;
	size_t gridKb = vfgi.world.valid ?
		(size_t)( vfgi.world.gridX * vfgi.world.gridY * vfgi.world.gridZ * sizeof( vfgiGridCell_t ) ) / 1024 : 0;

	ri.Printf( PRINT_ALL,
		"[VFGI] active=%d loaded=%d verts=%u grid=%ux%ux%u mem~%zuKB+%zuKB\n",
		R_VFGI_Active() ? 1 : 0,
		vfgi.loaded ? 1 : 0,
		vfgi.world.vertexCount,
		vfgi.world.gridX, vfgi.world.gridY, vfgi.world.gridZ,
		vertKb, gridKb );
}

void R_VFGI_Init( void )
{
	r_vfgi = ri.Cvar_Get( "r_vfgi", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_vfgi_strength = ri.Cvar_Get( "r_vfgi_strength", "1", CVAR_ARCHIVE_ND );
	r_vfgi_scale = ri.Cvar_Get( "r_vfgi_scale", "1", CVAR_ARCHIVE_ND );
	r_vfgi_featureDim = ri.Cvar_Get( "r_vfgi_featureDim", "4", CVAR_ARCHIVE_ND );
	r_vfgi_hiddenDim = ri.Cvar_Get( "r_vfgi_hiddenDim", "16", CVAR_ARCHIVE_ND );
	r_vfgi_vertCap = ri.Cvar_Get( "r_vfgi_vertCap", "524288", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_vfgi_quant = ri.Cvar_Get( "r_vfgi_quant", "8", CVAR_ARCHIVE_ND );
	r_vfgi_gridX = ri.Cvar_Get( "r_vfgi_gridX", "48", CVAR_ARCHIVE_ND );
	r_vfgi_gridY = ri.Cvar_Get( "r_vfgi_gridY", "48", CVAR_ARCHIVE_ND );
	r_vfgi_gridZ = ri.Cvar_Get( "r_vfgi_gridZ", "48", CVAR_ARCHIVE_ND );
	r_vfgi_useGBuffer = ri.Cvar_Get( "r_vfgi_useGBuffer", "1", CVAR_ARCHIVE_ND );
	r_vfgi_debug = ri.Cvar_Get( "r_vfgi_debug", "0", CVAR_ARCHIVE_ND );
	r_vfgi_skipSky = ri.Cvar_Get( "r_vfgi_skipSky", "1", CVAR_ARCHIVE_ND );
	r_vfgi_normalAtten = ri.Cvar_Get( "r_vfgi_normalAtten", "0.6", CVAR_ARCHIVE_ND );
	r_vfgi_ao = ri.Cvar_Get( "r_vfgi_ao", "0.75", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_vfgi, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_vfgi_scale, "0.25", "1", CV_FLOAT );
	ri.Cvar_CheckRange( r_vfgi_normalAtten, "0", "1", CV_FLOAT );
	ri.Cvar_CheckRange( r_vfgi_ao, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_vfgi,
		"Vertex Features Neural GI: per-vertex features on world meshes (0=off, 1=on)." );
	ri.Cvar_SetDescription( r_vfgi_vertCap,
		"Max unique world vertices for VFGI index (latched)." );
	ri.Cvar_SetDescription( r_vfgi_normalAtten,
		"VFGI composite normal attenuation for indirect GI leak reduction (0=off, 1=max)." );
	ri.Cvar_SetDescription( r_vfgi_ao,
		"VFGI composite AO coupling: scales indirect GI by SSAO/HBAO when available (0=off, 1=full)." );

	ri.Cmd_AddCommand( "vfgi_reload", VFGI_Cmd_Reload );
	ri.Cmd_AddCommand( "vfgi_status", VFGI_Cmd_Status );

	if ( r_vfgi->integer ) {
		ri.Printf( PRINT_ALL,
			"[VFGI] Vertex Features Neural GI enabled (experimental). See docs/VERTEX_FEATURES_NEURAL_GI.md\n" );
	}
}

void R_VFGI_Shutdown( void )
{
	ri.Cmd_RemoveCommand( "vfgi_reload" );
	ri.Cmd_RemoveCommand( "vfgi_status" );
	VFGI_ClearGpu();
	VFGI_World_Free( &vfgi.world );
	Com_Memset( &vfgi, 0, sizeof( vfgi ) );
}

qboolean R_VFGI_Active( void )
{
	return ( r_vfgi && r_vfgi->integer && vfgi.loaded && vk.vfgi.buffers_ready &&
		vk.fboActive && vk.depth_image != VK_NULL_HANDLE ) ? qtrue : qfalse;
}

void R_VFGI_OnMapLoad( const char *mapBaseName )
{
	uint32_t maxVerts;
	float quant;
	byte *buf;
	int len;
	const char *tryPaths[2];
	int i;

	VFGI_ClearGpu();
	VFGI_World_Free( &vfgi.world );
	Com_Memset( &vfgi, 0, sizeof( vfgi ) );

	if ( !r_vfgi || !r_vfgi->integer ) {
		return;
	}
	if ( !mapBaseName || !mapBaseName[0] || !tr.world ) {
		return;
	}

	Q_strncpyz( vfgi.mapName, mapBaseName, sizeof( vfgi.mapName ) );
	vfgi.man.version = VFGI_MANIFEST_VERSION;
	vfgi.man.featureDim = 4;
	vfgi.man.hiddenDim = 16;
	vfgi.man.gridX = 48;
	vfgi.man.gridY = 48;
	vfgi.man.gridZ = 48;
	vfgi.man.quantUnits = 8.0f;

	tryPaths[0] = va( "maps/%s.vfgi", mapBaseName );
	tryPaths[1] = va( "vfgi/%s.vfgi", mapBaseName );

	for ( i = 0; i < 2; i++ ) {
		len = ri.FS_ReadFile( tryPaths[i], (void **)&buf );
		if ( len > 0 && buf ) {
			if ( VFGI_ParseManifest( (const char *)buf, &vfgi.man ) ) {
				ri.Printf( PRINT_ALL, "[VFGI] Loaded manifest %s\n", tryPaths[i] );
			}
			ri.FS_FreeFile( buf );
			break;
		}
	}

	if ( r_vfgi_featureDim && r_vfgi_featureDim->integer > 0 ) {
		vfgi.man.featureDim = r_vfgi_featureDim->integer;
	}
	if ( r_vfgi_hiddenDim && r_vfgi_hiddenDim->integer > 0 ) {
		vfgi.man.hiddenDim = r_vfgi_hiddenDim->integer;
	}
	if ( r_vfgi_quant && r_vfgi_quant->value > 0.0f ) {
		vfgi.man.quantUnits = r_vfgi_quant->value;
	}

	maxVerts = r_vfgi_vertCap ? (uint32_t)r_vfgi_vertCap->integer : 524288u;
	if ( maxVerts > VFGI_MAX_VERTICES ) {
		maxVerts = VFGI_MAX_VERTICES;
	}
	if ( maxVerts < 1024 ) {
		maxVerts = 1024;
	}

	quant = vfgi.man.quantUnits;
	if ( r_vfgi_gridX && r_vfgi_gridX->integer > 0 ) {
		vfgi.man.gridX = r_vfgi_gridX->integer;
	}
	if ( r_vfgi_gridY && r_vfgi_gridY->integer > 0 ) {
		vfgi.man.gridY = r_vfgi_gridY->integer;
	}
	if ( r_vfgi_gridZ && r_vfgi_gridZ->integer > 0 ) {
		vfgi.man.gridZ = r_vfgi_gridZ->integer;
	}

	if ( !VFGI_World_BuildFromMap( &vfgi.world, tr.world, maxVerts, quant,
			(uint32_t)vfgi.man.gridX, (uint32_t)vfgi.man.gridY, (uint32_t)vfgi.man.gridZ, qtrue ) ) {
		ri.Printf( PRINT_WARNING, "[VFGI] Failed to build vertex features from world '%s'\n", mapBaseName );
		return;
	}

	{
		char weightsPath[MAX_QPATH];

		if ( vfgi.man.weightsPath[0] ) {
			Q_strncpyz( weightsPath, vfgi.man.weightsPath, sizeof( weightsPath ) );
		} else {
			Com_sprintf( weightsPath, sizeof( weightsPath ), "vfgi/%s.vfgb", mapBaseName );
		}
		if ( !vk_neural_load_mlp_rgb( VK_NEURAL_MAGIC_VFG1, weightsPath,
				vfgi.man.featureDim, vfgi.man.hiddenDim,
				vfgi.W1, VFGI_MAX_FEATURE_DIM, vfgi.b1, vfgi.W2, vfgi.b2,
				VFGI_MAX_FEATURE_DIM, VFGI_MAX_HIDDEN ) ) {
			VFGI_BuildDefaultWeights();
		}
	}
	if ( !VFGI_UploadWeightsBuffer() || !VFGI_UploadWorldBuffers() ) {
		ri.Printf( PRINT_WARNING, "[VFGI] GPU upload failed\n" );
		VFGI_World_Free( &vfgi.world );
		return;
	}

	vfgi.loaded = qtrue;
	vk.vfgiAllocated = qtrue;
	ri.Printf( PRINT_ALL,
		"[VFGI] Ready on '%s': %u vertices, grid %ux%ux%u (~%zuKB verts + ~%zuKB index)\n",
		mapBaseName, vfgi.world.vertexCount,
		vfgi.world.gridX, vfgi.world.gridY, vfgi.world.gridZ,
		(size_t)( vfgi.world.vertexCount * sizeof( vfgiVertexRecord_t ) ) / 1024,
		(size_t)( vfgi.world.gridX * vfgi.world.gridY * vfgi.world.gridZ * sizeof( vfgiGridCell_t ) ) / 1024 );
}

void vk_vfgi_apply_after_geometry( void )
{
	uint32_t fullW, fullH, width, height;
	VkImageView depthView, normalView, albedoView, aoView;
	VkImageAspectFlags depth_aspect;
	vk_vfgi_decode_push_t decodePush;
	vk_vfgi_composite_push_t compPush;
	float scale;
	uint32_t gx, gy;
	qboolean useGbuf;
	qboolean hasAO;

	if ( !R_VFGI_Active() ) {
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

	scale = r_vfgi_scale ? r_vfgi_scale->value : 1.0f;
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

	if ( !VFGI_EnsureIrradianceTarget( width, height ) ) {
		return;
	}

	VFGI_CreateDecodePipeline();
	VFGI_CreateCompositePipeline();
	if ( !vk.vfgi.decode_ready || !vk.vfgi.composite_ready ) {
		return;
	}

	depthView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	useGbuf = ( r_vfgi_useGBuffer && r_vfgi_useGBuffer->integer &&
		vk.deferred_gbuffer_normal_view != VK_NULL_HANDLE &&
		vk_deferred_gbuffer_fill_wanted() ) ? qtrue : qfalse;
	normalView = useGbuf ? vk.deferred_gbuffer_normal_view :
		( tr.whiteImage ? tr.whiteImage->view : VK_NULL_HANDLE );
	albedoView = ( vk.deferred_gbuffer_albedo_view != VK_NULL_HANDLE && useGbuf ) ?
		vk.deferred_gbuffer_albedo_view : vk.color_image_view;
	hasAO = ( r_ssao && r_ssao->integer && vk.ssao_blur_image_view != VK_NULL_HANDLE ) ? qtrue : qfalse;
	aoView = hasAO ? vk.ssao_blur_image_view :
		( tr.whiteImage ? tr.whiteImage->view : VK_NULL_HANDLE );

	depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	record_image_layout_transition( vk.cmd->command_buffer, vk.vfgi.irradiance_image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );

	{
		VkDescriptorPoolSize pool_sizes[3];
		VkDescriptorPoolCreateInfo pool_ci;
		VkDescriptorSetAllocateInfo alloc_ci;
		VkDescriptorBufferInfo buf_infos[3];
		VkDescriptorImageInfo depth_img, normal_img, out_img;
		VkWriteDescriptorSet writes[6];

		if ( vk.vfgi.decode_pool != VK_NULL_HANDLE ) {
			qvkDestroyDescriptorPool( vk.device, vk.vfgi.decode_pool, NULL );
		}
		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		pool_sizes[0].descriptorCount = 3;
		pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[1].descriptorCount = 2;
		pool_sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		pool_sizes[2].descriptorCount = 1;
		Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
		pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_ci.maxSets = 1;
		pool_ci.poolSizeCount = 3;
		pool_ci.pPoolSizes = pool_sizes;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.vfgi.decode_pool ) );
		Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
		alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_ci.descriptorPool = vk.vfgi.decode_pool;
		alloc_ci.descriptorSetCount = 1;
		alloc_ci.pSetLayouts = &vk.vfgi.decode_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &vk.vfgi.decode_descriptor ) );

		buf_infos[0].buffer = vk.vfgi.vertex_buffer;
		buf_infos[0].offset = 0;
		buf_infos[0].range = VK_WHOLE_SIZE;
		buf_infos[1].buffer = vk.vfgi.grid_buffer;
		buf_infos[1].offset = 0;
		buf_infos[1].range = VK_WHOLE_SIZE;
		buf_infos[2].buffer = vk.vfgi.weights_buffer;
		buf_infos[2].offset = 0;
		buf_infos[2].range = vk.vfgi.weights_size;

		depth_img.sampler = VFGI_DepthSampler();
		depth_img.imageView = depthView;
		depth_img.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		normal_img.sampler = VFGI_LinearSampler();
		normal_img.imageView = normalView;
		normal_img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		out_img.imageView = vk.vfgi.irradiance_view;
		out_img.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( writes, 0, sizeof( writes ) );
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = vk.vfgi.decode_descriptor;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[0].pBufferInfo = &buf_infos[0];
		writes[1] = writes[0];
		writes[1].dstBinding = 1;
		writes[1].pBufferInfo = &buf_infos[1];
		writes[2] = writes[0];
		writes[2].dstBinding = 2;
		writes[2].pBufferInfo = &buf_infos[2];
		writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].dstSet = vk.vfgi.decode_descriptor;
		writes[3].dstBinding = 3;
		writes[3].descriptorCount = 1;
		writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[3].pImageInfo = &depth_img;
		writes[4] = writes[3];
		writes[4].dstBinding = 4;
		writes[4].pImageInfo = &normal_img;
		writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[5].dstSet = vk.vfgi.decode_descriptor;
		writes[5].dstBinding = 5;
		writes[5].descriptorCount = 1;
		writes[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[5].pImageInfo = &out_img;
		qvkUpdateDescriptorSets( vk.device, 6, writes, 0, NULL );
	}

	Com_Memset( &decodePush, 0, sizeof( decodePush ) );
	VFGI_FillInvViewProj( decodePush.invViewProj );
	decodePush.worldMin[0] = vfgi.world.worldMin[0];
	decodePush.worldMin[1] = vfgi.world.worldMin[1];
	decodePush.worldMin[2] = vfgi.world.worldMin[2];
	decodePush.worldMin[3] = 0.0f;
	decodePush.worldMax[0] = vfgi.world.worldMax[0];
	decodePush.worldMax[1] = vfgi.world.worldMax[1];
	decodePush.worldMax[2] = vfgi.world.worldMax[2];
	decodePush.worldMax[3] = 0.0f;
	decodePush.gridDim[0] = vfgi.world.gridX;
	decodePush.gridDim[1] = vfgi.world.gridY;
	decodePush.gridDim[2] = vfgi.world.gridZ;
	decodePush.featureDim = (uint32_t)vfgi.man.featureDim;
	decodePush.hiddenDim = (uint32_t)vfgi.man.hiddenDim;
	decodePush.useGBufferNormal = useGbuf ? 1u : 0u;
	decodePush.hasGBuffer = useGbuf ? 1u : 0u;
	decodePush.extent[0] = width;
	decodePush.extent[1] = height;
	decodePush.strength = r_vfgi_strength ? r_vfgi_strength->value : 1.0f;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.vfgi.decode_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.vfgi.decode_pipeline_layout, 0, 1, &vk.vfgi.decode_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.vfgi.decode_pipeline_layout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( decodePush ), &decodePush );
	gx = ( width + 7u ) / 8u;
	gy = ( height + 7u ) / 8u;
	qvkCmdDispatch( vk.cmd->command_buffer, gx, gy, 1 );

	record_image_layout_transition( vk.cmd->command_buffer, vk.vfgi.irradiance_image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );

	{
		VkDescriptorPoolSize pool_sizes[2];
		VkDescriptorPoolCreateInfo pool_ci;
		VkDescriptorSetAllocateInfo alloc_ci;
		VkDescriptorImageInfo img[6];
		VkWriteDescriptorSet writes[6];

		if ( vk.vfgi.composite_pool != VK_NULL_HANDLE ) {
			qvkDestroyDescriptorPool( vk.device, vk.vfgi.composite_pool, NULL );
		}
		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[0].descriptorCount = 5;
		pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		pool_sizes[1].descriptorCount = 1;
		Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
		pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_ci.maxSets = 1;
		pool_ci.poolSizeCount = 2;
		pool_ci.pPoolSizes = pool_sizes;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.vfgi.composite_pool ) );
		Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
		alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_ci.descriptorPool = vk.vfgi.composite_pool;
		alloc_ci.descriptorSetCount = 1;
		alloc_ci.pSetLayouts = &vk.vfgi.composite_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &vk.vfgi.composite_descriptor ) );

		img[0].sampler = VFGI_DepthSampler();
		img[0].imageView = depthView;
		img[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		img[1].sampler = VFGI_LinearSampler();
		img[1].imageView = albedoView;
		img[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		img[2].sampler = VFGI_LinearSampler();
		img[2].imageView = vk.vfgi.irradiance_view;
		img[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		img[3].sampler = VFGI_LinearSampler();
		img[3].imageView = normalView;
		img[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		img[4].sampler = VFGI_LinearSampler();
		img[4].imageView = aoView;
		img[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		img[5].imageView = vk.color_image_view;
		img[5].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( writes, 0, sizeof( writes ) );
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = vk.vfgi.composite_descriptor;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].pImageInfo = &img[0];
		writes[1] = writes[0];
		writes[1].dstBinding = 1;
		writes[1].pImageInfo = &img[1];
		writes[2] = writes[0];
		writes[2].dstBinding = 2;
		writes[2].pImageInfo = &img[2];
		writes[3] = writes[0];
		writes[3].dstBinding = 3;
		writes[3].pImageInfo = &img[3];
		writes[4] = writes[0];
		writes[4].dstBinding = 4;
		writes[4].pImageInfo = &img[4];
		writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[5].dstSet = vk.vfgi.composite_descriptor;
		writes[5].dstBinding = 5;
		writes[5].descriptorCount = 1;
		writes[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[5].pImageInfo = &img[5];
		qvkUpdateDescriptorSets( vk.device, 6, writes, 0, NULL );
	}

	Com_Memset( &compPush, 0, sizeof( compPush ) );
	compPush.extent[0] = fullW;
	compPush.extent[1] = fullH;
	compPush.strength = 1.0f;
	compPush.skipSky = ( r_vfgi_skipSky && r_vfgi_skipSky->integer ) ? 1u : 0u;
	compPush.normalAtten = ( useGbuf && r_vfgi_normalAtten ) ? r_vfgi_normalAtten->value : 0.0f;
	compPush.aoStrength = ( hasAO && r_vfgi_ao ) ? r_vfgi_ao->value : 0.0f;
	compPush.hasNormal = useGbuf ? 1u : 0u;
	compPush.hasAO = hasAO ? 1u : 0u;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.vfgi.composite_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.vfgi.composite_pipeline_layout, 0, 1, &vk.vfgi.composite_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.vfgi.composite_pipeline_layout,
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

	if ( r_vfgi_debug && r_vfgi_debug->integer ) {
		ri.Printf( PRINT_DEVELOPER, "[VFGI] decode %ux%u composite %ux%u verts=%u\n",
			width, height, fullW, fullH, vfgi.world.vertexCount );
	}
}
