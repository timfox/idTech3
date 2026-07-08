/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Neural Visibility Cache — ReSTIR-style direct lighting over Forward+ with
neural visibility weighting (disocclusion-heavy dynamic lights). See
docs/NEURAL_VISIBILITY_CACHE.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_nvc.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_view_state.h"
#include "vk_deferred_gbuffer.h"
#include "vk_cmd.h"
#include "vk_neural_io.h"

#define NVC_MANIFEST_VERSION    1
#define NVC_MAGIC_WEIGHTS       0x3156434E /* 'NVC1' */
#define NVC_MAX_FEATURE_DIM     4
#define NVC_MAX_HIDDEN          16

typedef struct {
	int         version;
	int         featureDim;
	int         hiddenDim;
	char        weightsPath[MAX_QPATH];
} nvcManifest_t;

typedef struct {
	qboolean    loaded;
	char        mapName[MAX_QPATH];
	nvcManifest_t man;
	float       W1[NVC_MAX_HIDDEN * NVC_MAX_FEATURE_DIM];
	float       b1[NVC_MAX_HIDDEN];
	float       W2[NVC_MAX_HIDDEN];
	float       b2;
	uint32_t    targetWidth;
	uint32_t    targetHeight;
} nvcState_t;

static nvcState_t nvc;

static cvar_t *r_nvc;
static cvar_t *r_nvc_strength;
static cvar_t *r_nvc_scale;
static cvar_t *r_nvc_disocclusionBoost;
static cvar_t *r_nvc_reservoirM;
static cvar_t *r_nvc_restirMode;
static cvar_t *r_nvc_featureDim;
static cvar_t *r_nvc_hiddenDim;
static cvar_t *r_nvc_useGBuffer;
static cvar_t *r_nvc_debug;
static cvar_t *r_nvc_skipSky;

typedef struct {
	vec4_t extent;
	vec4_t featureHidden;
	uint32_t useGBufferNormal;
	uint32_t hasGBuffer;
	uint32_t pad0;
	uint32_t pad1;
} vk_nvc_cache_push_t;

typedef struct {
	float invViewProj[16];
	vec4_t extent;
	vec4_t params2;
	vec4_t featureHidden;
	uint32_t useGBufferNormal;
	uint32_t hasGBuffer;
	uint32_t restirMode;
	uint32_t pad0;
} vk_nvc_restir_push_t;

typedef struct {
	vec4_t extent;
	vec4_t params;
} vk_nvc_composite_push_t;

static VkSampler NVC_DepthSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static VkSampler NVC_LinearSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	return vk_find_sampler( &sd );
}

static void NVC_ClearGpu( void )
{
	if ( vk.nvc.cache_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.nvc.cache_pipeline, NULL );
		vk.nvc.cache_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.nvc.restir_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.nvc.restir_pipeline, NULL );
		vk.nvc.restir_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.nvc.composite_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.nvc.composite_pipeline, NULL );
		vk.nvc.composite_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.nvc.cache_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.nvc.cache_pipeline_layout, NULL );
		vk.nvc.cache_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.nvc.restir_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.nvc.restir_pipeline_layout, NULL );
		vk.nvc.restir_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.nvc.composite_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.nvc.composite_pipeline_layout, NULL );
		vk.nvc.composite_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.nvc.cache_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.nvc.cache_layout, NULL );
		vk.nvc.cache_layout = VK_NULL_HANDLE;
	}
	if ( vk.nvc.restir_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.nvc.restir_layout, NULL );
		vk.nvc.restir_layout = VK_NULL_HANDLE;
	}
	if ( vk.nvc.composite_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.nvc.composite_layout, NULL );
		vk.nvc.composite_layout = VK_NULL_HANDLE;
	}
	if ( vk.nvc.cache_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.nvc.cache_pool, NULL );
		vk.nvc.cache_pool = VK_NULL_HANDLE;
	}
	if ( vk.nvc.restir_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.nvc.restir_pool, NULL );
		vk.nvc.restir_pool = VK_NULL_HANDLE;
	}
	if ( vk.nvc.composite_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.nvc.composite_pool, NULL );
		vk.nvc.composite_pool = VK_NULL_HANDLE;
	}
	if ( vk.nvc.weights_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.nvc.weights_buffer, NULL );
		vk.nvc.weights_buffer = VK_NULL_HANDLE;
	}
	if ( vk.nvc.weights_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.nvc.weights_memory, NULL );
		vk.nvc.weights_memory = VK_NULL_HANDLE;
	}
	if ( vk.nvc.cache_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.nvc.cache_image, NULL );
		vk.nvc.cache_image = VK_NULL_HANDLE;
	}
	if ( vk.nvc.cache_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.nvc.cache_view, NULL );
		vk.nvc.cache_view = VK_NULL_HANDLE;
	}
	if ( vk.nvc.cache_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.nvc.cache_memory, NULL );
		vk.nvc.cache_memory = VK_NULL_HANDLE;
	}
	if ( vk.nvc.direct_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.nvc.direct_image, NULL );
		vk.nvc.direct_image = VK_NULL_HANDLE;
	}
	if ( vk.nvc.direct_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.nvc.direct_view, NULL );
		vk.nvc.direct_view = VK_NULL_HANDLE;
	}
	if ( vk.nvc.direct_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.nvc.direct_memory, NULL );
		vk.nvc.direct_memory = VK_NULL_HANDLE;
	}
	vk.nvc.cache_descriptor = VK_NULL_HANDLE;
	vk.nvc.restir_descriptor = VK_NULL_HANDLE;
	vk.nvc.composite_descriptor = VK_NULL_HANDLE;
	vk.nvc.cache_ready = qfalse;
	vk.nvc.restir_ready = qfalse;
	vk.nvc.composite_ready = qfalse;
	vk.nvc.weights_ready = qfalse;
	vk.nvcAllocated = qfalse;
}

static void NVC_BuildDefaultWeights( void )
{
	int h = nvc.man.hiddenDim;
	int i;

	Com_Memset( nvc.W1, 0, sizeof( nvc.W1 ) );
	Com_Memset( nvc.b1, 0, sizeof( nvc.b1 ) );
	Com_Memset( nvc.W2, 0, sizeof( nvc.W2 ) );
	nvc.b2 = 0.55f;

	for ( i = 0; i < h; i++ ) {
		nvc.b1[i] = -0.12f;
		nvc.W1[i * NVC_MAX_FEATURE_DIM + 0] = -0.85f;
		nvc.W1[i * NVC_MAX_FEATURE_DIM + 1] = 0.45f;
		nvc.W1[i * NVC_MAX_FEATURE_DIM + 2] = 0.25f;
		nvc.W1[i * NVC_MAX_FEATURE_DIM + 3] = -0.7f;
	}
	for ( i = 0; i < h; i++ ) {
		nvc.W2[i] = ( i % 2 == 0 ) ? 0.6f : 0.35f;
	}
}

static qboolean NVC_ParseManifest( const char *text, nvcManifest_t *man )
{
	const char *parse;
	const char *token;
	char key[64];
	char value[MAX_TOKEN_CHARS];

	Com_Memset( man, 0, sizeof( *man ) );
	man->version = NVC_MANIFEST_VERSION;
	man->featureDim = 4;
	man->hiddenDim = 8;

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
		} else if ( !Q_stricmp( key, "weightsPath" ) ) {
			Q_strncpyz( man->weightsPath, value, sizeof( man->weightsPath ) );
		}
	}

	if ( man->featureDim < 1 || man->featureDim > NVC_MAX_FEATURE_DIM ) {
		return qfalse;
	}
	if ( man->hiddenDim < 1 || man->hiddenDim > NVC_MAX_HIDDEN ) {
		return qfalse;
	}
	return qtrue;
}

static qboolean NVC_UploadWeightsBuffer( void )
{
	int h = nvc.man.hiddenDim;
	int f = nvc.man.featureDim;
	VkDeviceSize size;
	float *cpu;
	VkBufferCreateInfo buf_ci;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_ci;
	void *mapped;

	size = (VkDeviceSize)( ( h * f ) + h + h + 1 ) * sizeof( float );
	cpu = (float *)ri.Malloc( (size_t)size );
	Com_Memcpy( cpu, nvc.W1, (size_t)( h * f ) * sizeof( float ) );
	Com_Memcpy( cpu + h * f, nvc.b1, (size_t)h * sizeof( float ) );
	Com_Memcpy( cpu + h * f + h, nvc.W2, (size_t)h * sizeof( float ) );
	cpu[h * f + h + h] = nvc.b2;

	if ( vk.nvc.weights_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.nvc.weights_buffer, NULL );
		qvkFreeMemory( vk.device, vk.nvc.weights_memory, NULL );
	}

	Com_Memset( &buf_ci, 0, sizeof( buf_ci ) );
	buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_ci.size = size;
	buf_ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_ci, NULL, &vk.nvc.weights_buffer ) );

	qvkGetBufferMemoryRequirements( vk.device, vk.nvc.weights_buffer, &mem_req );
	Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
	alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_ci.allocationSize = mem_req.size;
	alloc_ci.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_ci, NULL, &vk.nvc.weights_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.nvc.weights_buffer, vk.nvc.weights_memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.nvc.weights_memory, 0, size, 0, &mapped ) );
	Com_Memcpy( mapped, cpu, (size_t)size );
	qvkUnmapMemory( vk.device, vk.nvc.weights_memory );
	ri.Free( cpu );
	vk.nvc.weights_size = size;
	vk.nvc.weights_ready = qtrue;
	return qtrue;
}

static qboolean NVC_EnsureTargets( uint32_t width, uint32_t height )
{
	VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	VkImageCreateInfo image_desc;
	VkImageViewCreateInfo view_desc;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_info;

	if ( vk.nvc.cache_image != VK_NULL_HANDLE &&
		nvc.targetWidth == width && nvc.targetHeight == height ) {
		return qtrue;
	}

	if ( vk.nvc.cache_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.nvc.cache_view, NULL );
		vk.nvc.cache_view = VK_NULL_HANDLE;
	}
	if ( vk.nvc.cache_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.nvc.cache_image, NULL );
		qvkFreeMemory( vk.device, vk.nvc.cache_memory, NULL );
		vk.nvc.cache_image = VK_NULL_HANDLE;
		vk.nvc.cache_memory = VK_NULL_HANDLE;
	}
	if ( vk.nvc.direct_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.nvc.direct_view, NULL );
		vk.nvc.direct_view = VK_NULL_HANDLE;
	}
	if ( vk.nvc.direct_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.nvc.direct_image, NULL );
		qvkFreeMemory( vk.device, vk.nvc.direct_memory, NULL );
		vk.nvc.direct_image = VK_NULL_HANDLE;
		vk.nvc.direct_memory = VK_NULL_HANDLE;
	}

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

	if ( qvkCreateImage( vk.device, &image_desc, NULL, &vk.nvc.cache_image ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetImageMemoryRequirements( vk.device, vk.nvc.cache_image, &mem_req );
	Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.nvc.cache_memory ) != VK_SUCCESS ||
		qvkBindImageMemory( vk.device, vk.nvc.cache_image, vk.nvc.cache_memory, 0 ) != VK_SUCCESS ) {
		return qfalse;
	}
	Com_Memset( &view_desc, 0, sizeof( view_desc ) );
	view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_desc.image = vk.nvc.cache_image;
	view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_desc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_desc.subresourceRange.levelCount = 1;
	view_desc.subresourceRange.layerCount = 1;
	if ( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.nvc.cache_view ) != VK_SUCCESS ) {
		return qfalse;
	}

	if ( qvkCreateImage( vk.device, &image_desc, NULL, &vk.nvc.direct_image ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetImageMemoryRequirements( vk.device, vk.nvc.direct_image, &mem_req );
	alloc_info.allocationSize = mem_req.size;
	if ( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.nvc.direct_memory ) != VK_SUCCESS ||
		qvkBindImageMemory( vk.device, vk.nvc.direct_image, vk.nvc.direct_memory, 0 ) != VK_SUCCESS ) {
		return qfalse;
	}
	view_desc.image = vk.nvc.direct_image;
	if ( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.nvc.direct_view ) != VK_SUCCESS ) {
		return qfalse;
	}

	nvc.targetWidth = width;
	nvc.targetHeight = height;
	return qtrue;
}

static void NVC_CreateCachePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[4];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.nvc.cache_ready ) {
		return;
	}
	if ( vk.modules.nvc_cache_cs == VK_NULL_HANDLE ) {
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
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
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
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.nvc.cache_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_nvc_cache_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.nvc.cache_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.nvc.cache_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.nvc_cache_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.nvc.cache_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.nvc.cache_pipeline ) );
	vk.nvc.cache_ready = qtrue;
}

static void NVC_CreateRestirPipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[8];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.nvc.restir_ready ) {
		return;
	}
	if ( vk.modules.nvc_restir_cs == VK_NULL_HANDLE ) {
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
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[5].binding = 5;
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[5].descriptorCount = 1;
	bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[6].binding = 6;
	bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[6].descriptorCount = 1;
	bindings[6].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[7].binding = 7;
	bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[7].descriptorCount = 1;
	bindings[7].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 8;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.nvc.restir_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_nvc_restir_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.nvc.restir_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.nvc.restir_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.nvc_restir_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.nvc.restir_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.nvc.restir_pipeline ) );
	vk.nvc.restir_ready = qtrue;
}

static void NVC_CreateCompositePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[3];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.nvc.composite_ready ) {
		return;
	}
	if ( vk.modules.nvc_composite_cs == VK_NULL_HANDLE ) {
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
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 3;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.nvc.composite_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_nvc_composite_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.nvc.composite_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.nvc.composite_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.nvc_composite_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.nvc.composite_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.nvc.composite_pipeline ) );
	vk.nvc.composite_ready = qtrue;
}

static void NVC_FillInvViewProj( float *out16 )
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

static void NVC_Cmd_Reload( void )
{
	if ( nvc.mapName[0] ) {
		R_NVC_OnMapLoad( nvc.mapName );
	}
}

static void NVC_Cmd_Status( void )
{
	ri.Printf( PRINT_ALL,
		"[NVC] active=%d loaded=%d forwardPlus=%d restirMode=%d weights=%zuB target=%ux%u\n",
		R_NVC_Active() ? 1 : 0,
		nvc.loaded ? 1 : 0,
		( r_forwardPlus && r_forwardPlus->integer ) ? 1 : 0,
		r_nvc_restirMode ? r_nvc_restirMode->integer : 0,
		(size_t)vk.nvc.weights_size,
		nvc.targetWidth, nvc.targetHeight );
}

void R_NVC_Init( void )
{
	r_nvc = ri.Cvar_Get( "r_nvc", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_nvc_strength = ri.Cvar_Get( "r_nvc_strength", "1", CVAR_ARCHIVE_ND );
	r_nvc_scale = ri.Cvar_Get( "r_nvc_scale", "1", CVAR_ARCHIVE_ND );
	r_nvc_disocclusionBoost = ri.Cvar_Get( "r_nvc_disocclusionBoost", "1.5", CVAR_ARCHIVE_ND );
	r_nvc_reservoirM = ri.Cvar_Get( "r_nvc_reservoirM", "4", CVAR_ARCHIVE_ND );
	r_nvc_restirMode = ri.Cvar_Get( "r_nvc_restirMode", "1", CVAR_ARCHIVE_ND );
	r_nvc_featureDim = ri.Cvar_Get( "r_nvc_featureDim", "4", CVAR_ARCHIVE_ND );
	r_nvc_hiddenDim = ri.Cvar_Get( "r_nvc_hiddenDim", "8", CVAR_ARCHIVE_ND );
	r_nvc_useGBuffer = ri.Cvar_Get( "r_nvc_useGBuffer", "1", CVAR_ARCHIVE_ND );
	r_nvc_debug = ri.Cvar_Get( "r_nvc_debug", "0", CVAR_ARCHIVE_ND );
	r_nvc_skipSky = ri.Cvar_Get( "r_nvc_skipSky", "1", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_nvc, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_nvc_scale, "0.25", "1", CV_FLOAT );
	ri.Cvar_CheckRange( r_nvc_restirMode, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_nvc_reservoirM, "1", "16", CV_INTEGER );
	ri.Cvar_SetDescription( r_nvc,
		"Neural Visibility Cache: ReSTIR-style Forward+ direct refine for dynamic many-light scenes (0=off, 1=on)." );
	ri.Cvar_SetDescription( r_nvc_scale,
		"NVC cache/restir resolution scale (1=full, 0.5=half)." );
	ri.Cvar_SetDescription( r_nvc_restirMode,
		"NVC ReSTIR pass: 0=cache only, 1=reservoir direct lighting refine." );
	ri.Cvar_SetDescription( r_nvc_disocclusionBoost,
		"Extra candidate weight at depth disocclusions (neon/torches/muzzle flashes)." );

	ri.Cmd_AddCommand( "nvc_reload", NVC_Cmd_Reload );
	ri.Cmd_AddCommand( "nvc_status", NVC_Cmd_Status );

	if ( r_nvc->integer ) {
		ri.Printf( PRINT_ALL,
			"[NVC] Neural Visibility Cache enabled (experimental). Requires r_forwardPlus 1. See docs/NEURAL_VISIBILITY_CACHE.md\n" );
	}
}

void R_NVC_Shutdown( void )
{
	ri.Cmd_RemoveCommand( "nvc_reload" );
	ri.Cmd_RemoveCommand( "nvc_status" );
	NVC_ClearGpu();
	Com_Memset( &nvc, 0, sizeof( nvc ) );
}

qboolean R_NVC_Active( void )
{
	if ( !r_nvc || !r_nvc->integer || !nvc.loaded || !vk.nvc.weights_ready ) {
		return qfalse;
	}
	if ( !r_forwardPlus || !r_forwardPlus->integer ) {
		return qfalse;
	}
	if ( !vk.fboActive || vk.depth_image == VK_NULL_HANDLE ) {
		return qfalse;
	}
	if ( vk.forward_plus.buffer == VK_NULL_HANDLE || vk.forward_plus.tile_buffer == VK_NULL_HANDLE ) {
		return qfalse;
	}
	return qtrue;
}

void R_NVC_OnMapLoad( const char *mapBaseName )
{
	byte *buf;
	int len;
	const char *tryPaths[2];
	int i;

	NVC_ClearGpu();
	Com_Memset( &nvc, 0, sizeof( nvc ) );

	if ( !r_nvc || !r_nvc->integer ) {
		return;
	}
	if ( !mapBaseName || !mapBaseName[0] ) {
		return;
	}

	Q_strncpyz( nvc.mapName, mapBaseName, sizeof( nvc.mapName ) );

	tryPaths[0] = va( "maps/%s.nvc", mapBaseName );
	tryPaths[1] = va( "nvc/%s.nvc", mapBaseName );

	for ( i = 0; i < 2; i++ ) {
		len = ri.FS_ReadFile( tryPaths[i], (void **)&buf );
		if ( len > 0 && buf ) {
			if ( NVC_ParseManifest( (const char *)buf, &nvc.man ) ) {
				ri.Printf( PRINT_ALL, "[NVC] Loaded manifest %s\n", tryPaths[i] );
			}
			ri.FS_FreeFile( buf );
			break;
		}
	}

	if ( r_nvc_featureDim && r_nvc_featureDim->integer > 0 ) {
		nvc.man.featureDim = r_nvc_featureDim->integer;
	}
	if ( r_nvc_hiddenDim && r_nvc_hiddenDim->integer > 0 ) {
		nvc.man.hiddenDim = r_nvc_hiddenDim->integer;
	}

	{
		char weightsPath[MAX_QPATH];
		if ( nvc.man.weightsPath[0] ) {
			Q_strncpyz( weightsPath, nvc.man.weightsPath, sizeof( weightsPath ) );
		} else {
			Com_sprintf( weightsPath, sizeof( weightsPath ), "nvc/%s.nvcb", mapBaseName );
		}
		if ( !vk_neural_load_mlp_scalar( VK_NEURAL_MAGIC_NVC1, weightsPath,
				nvc.man.featureDim, nvc.man.hiddenDim,
				nvc.W1, NVC_MAX_FEATURE_DIM, nvc.b1, nvc.W2, &nvc.b2,
				NVC_MAX_FEATURE_DIM, NVC_MAX_HIDDEN ) ) {
			NVC_BuildDefaultWeights();
		}
	}
	if ( !NVC_UploadWeightsBuffer() ) {
		ri.Printf( PRINT_WARNING, "[NVC] Failed to upload weights\n" );
		return;
	}

	nvc.loaded = qtrue;
	vk.nvcAllocated = qtrue;
	ri.Printf( PRINT_ALL, "[NVC] Ready on '%s': featureDim=%d hiddenDim=%d (pair with r_forwardPlus 1)\n",
		mapBaseName, nvc.man.featureDim, nvc.man.hiddenDim );
}

void vk_nvc_apply_after_geometry( void )
{
	uint32_t fullW, fullH;
	uint32_t width, height;
	VkImageView depthView;
	VkImageView normalView;
	VkImageAspectFlags depth_aspect;
	vk_nvc_cache_push_t cachePush;
	vk_nvc_restir_push_t restirPush;
	vk_nvc_composite_push_t compPush;
	float scale;
	uint32_t gx, gy;
	qboolean useGbuf;

	if ( !R_NVC_Active() ) {
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

	scale = r_nvc_scale ? r_nvc_scale->value : 1.0f;
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

	if ( !NVC_EnsureTargets( width, height ) ) {
		return;
	}

	NVC_CreateCachePipeline();
	NVC_CreateRestirPipeline();
	NVC_CreateCompositePipeline();
	if ( !vk.nvc.cache_ready || !vk.nvc.restir_ready || !vk.nvc.composite_ready ) {
		return;
	}

	depthView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	useGbuf = ( r_nvc_useGBuffer && r_nvc_useGBuffer->integer &&
		vk.deferred_gbuffer_normal_view != VK_NULL_HANDLE &&
		vk_deferred_gbuffer_fill_wanted() ) ? qtrue : qfalse;
	normalView = useGbuf ? vk.deferred_gbuffer_normal_view :
		( tr.whiteImage ? tr.whiteImage->view : vk.color_image_view );

	depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	record_image_layout_transition( vk.cmd->command_buffer, vk.nvc.cache_image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );

	{
		VkDescriptorPoolSize pool_sizes[3];
		VkDescriptorPoolCreateInfo pool_ci;
		VkDescriptorSetAllocateInfo alloc_ci;
		VkDescriptorImageInfo depth_img;
		VkDescriptorImageInfo normal_img;
		VkDescriptorImageInfo cache_out;
		VkDescriptorBufferInfo buf_info;
		VkWriteDescriptorSet writes[4];

		if ( vk.nvc.cache_pool != VK_NULL_HANDLE ) {
			qvkDestroyDescriptorPool( vk.device, vk.nvc.cache_pool, NULL );
		}
		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[0].descriptorCount = 2;
		pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		pool_sizes[1].descriptorCount = 1;
		pool_sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		pool_sizes[2].descriptorCount = 1;
		Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
		pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_ci.maxSets = 1;
		pool_ci.poolSizeCount = 3;
		pool_ci.pPoolSizes = pool_sizes;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.nvc.cache_pool ) );
		Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
		alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_ci.descriptorPool = vk.nvc.cache_pool;
		alloc_ci.descriptorSetCount = 1;
		alloc_ci.pSetLayouts = &vk.nvc.cache_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &vk.nvc.cache_descriptor ) );

		Com_Memset( &depth_img, 0, sizeof( depth_img ) );
		depth_img.sampler = NVC_DepthSampler();
		depth_img.imageView = depthView;
		depth_img.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		Com_Memset( &normal_img, 0, sizeof( normal_img ) );
		normal_img.sampler = NVC_LinearSampler();
		normal_img.imageView = normalView;
		normal_img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		Com_Memset( &cache_out, 0, sizeof( cache_out ) );
		cache_out.imageView = vk.nvc.cache_view;
		cache_out.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		buf_info.buffer = vk.nvc.weights_buffer;
		buf_info.offset = 0;
		buf_info.range = vk.nvc.weights_size;

		Com_Memset( writes, 0, sizeof( writes ) );
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = vk.nvc.cache_descriptor;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].pImageInfo = &depth_img;
		writes[1] = writes[0];
		writes[1].dstBinding = 1;
		writes[1].pImageInfo = &normal_img;
		writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstSet = vk.nvc.cache_descriptor;
		writes[2].dstBinding = 2;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[2].pBufferInfo = &buf_info;
		writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].dstSet = vk.nvc.cache_descriptor;
		writes[3].dstBinding = 3;
		writes[3].descriptorCount = 1;
		writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[3].pImageInfo = &cache_out;
		qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );
	}

	Com_Memset( &cachePush, 0, sizeof( cachePush ) );
	cachePush.extent[0] = (float)width;
	cachePush.extent[1] = (float)height;
	cachePush.featureHidden[0] = (float)nvc.man.featureDim;
	cachePush.featureHidden[1] = (float)nvc.man.hiddenDim;
	cachePush.useGBufferNormal = useGbuf ? 1u : 0u;
	cachePush.hasGBuffer = useGbuf ? 1u : 0u;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.nvc.cache_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.nvc.cache_pipeline_layout, 0, 1, &vk.nvc.cache_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.nvc.cache_pipeline_layout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( cachePush ), &cachePush );
	gx = ( width + 7u ) / 8u;
	gy = ( height + 7u ) / 8u;
	qvkCmdDispatch( vk.cmd->command_buffer, gx, gy, 1 );

	record_image_layout_transition( vk.cmd->command_buffer, vk.nvc.cache_image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );

	record_image_layout_transition( vk.cmd->command_buffer, vk.nvc.direct_image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );

	{
		VkDescriptorPoolSize pool_sizes[3];
		VkDescriptorPoolCreateInfo pool_ci;
		VkDescriptorSetAllocateInfo alloc_ci;
		VkDescriptorImageInfo depth_img;
		VkDescriptorImageInfo normal_img;
		VkDescriptorImageInfo cache_img;
		VkDescriptorImageInfo direct_out;
		VkDescriptorBufferInfo buf_infos[4];
		VkWriteDescriptorSet writes[8];

		if ( vk.nvc.restir_pool != VK_NULL_HANDLE ) {
			qvkDestroyDescriptorPool( vk.device, vk.nvc.restir_pool, NULL );
		}
		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[0].descriptorCount = 3;
		pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		pool_sizes[1].descriptorCount = 4;
		pool_sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		pool_sizes[2].descriptorCount = 1;
		Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
		pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_ci.maxSets = 1;
		pool_ci.poolSizeCount = 3;
		pool_ci.pPoolSizes = pool_sizes;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.nvc.restir_pool ) );
		Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
		alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_ci.descriptorPool = vk.nvc.restir_pool;
		alloc_ci.descriptorSetCount = 1;
		alloc_ci.pSetLayouts = &vk.nvc.restir_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &vk.nvc.restir_descriptor ) );

		Com_Memset( &depth_img, 0, sizeof( depth_img ) );
		depth_img.sampler = NVC_DepthSampler();
		depth_img.imageView = depthView;
		depth_img.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		Com_Memset( &normal_img, 0, sizeof( normal_img ) );
		normal_img.sampler = NVC_LinearSampler();
		normal_img.imageView = normalView;
		normal_img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		Com_Memset( &cache_img, 0, sizeof( cache_img ) );
		cache_img.sampler = NVC_LinearSampler();
		cache_img.imageView = vk.nvc.cache_view;
		cache_img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		Com_Memset( &direct_out, 0, sizeof( direct_out ) );
		direct_out.imageView = vk.nvc.direct_view;
		direct_out.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		buf_infos[0].buffer = vk.forward_plus.buffer;
		buf_infos[0].offset = 0;
		buf_infos[0].range = VK_WHOLE_SIZE;
		buf_infos[1].buffer = vk.forward_plus.tile_buffer;
		buf_infos[1].offset = 0;
		buf_infos[1].range = VK_WHOLE_SIZE;
		buf_infos[2].buffer = vk.forward_plus.param_buffer;
		buf_infos[2].offset = 0;
		buf_infos[2].range = VK_WHOLE_SIZE;
		buf_infos[3].buffer = vk.nvc.weights_buffer;
		buf_infos[3].offset = 0;
		buf_infos[3].range = vk.nvc.weights_size;

		Com_Memset( writes, 0, sizeof( writes ) );
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = vk.nvc.restir_descriptor;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].pImageInfo = &depth_img;
		writes[1] = writes[0];
		writes[1].dstBinding = 1;
		writes[1].pImageInfo = &normal_img;
		writes[2] = writes[0];
		writes[2].dstBinding = 2;
		writes[2].pImageInfo = &cache_img;
		writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].dstSet = vk.nvc.restir_descriptor;
		writes[3].dstBinding = 3;
		writes[3].descriptorCount = 1;
		writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[3].pBufferInfo = &buf_infos[0];
		writes[4] = writes[3];
		writes[4].dstBinding = 4;
		writes[4].pBufferInfo = &buf_infos[1];
		writes[5] = writes[3];
		writes[5].dstBinding = 5;
		writes[5].pBufferInfo = &buf_infos[2];
		writes[6] = writes[3];
		writes[6].dstBinding = 6;
		writes[6].pBufferInfo = &buf_infos[3];
		writes[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[7].dstSet = vk.nvc.restir_descriptor;
		writes[7].dstBinding = 7;
		writes[7].descriptorCount = 1;
		writes[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[7].pImageInfo = &direct_out;
		qvkUpdateDescriptorSets( vk.device, 8, writes, 0, NULL );
	}

	Com_Memset( &restirPush, 0, sizeof( restirPush ) );
	NVC_FillInvViewProj( restirPush.invViewProj );
	restirPush.extent[0] = (float)width;
	restirPush.extent[1] = (float)height;
	restirPush.params2[0] = r_nvc_strength ? r_nvc_strength->value : 1.0f;
	restirPush.params2[1] = r_nvc_disocclusionBoost ? r_nvc_disocclusionBoost->value : 1.5f;
	restirPush.params2[2] = r_nvc_reservoirM ? (float)r_nvc_reservoirM->integer : 4.0f;
	restirPush.featureHidden[0] = (float)nvc.man.featureDim;
	restirPush.featureHidden[1] = (float)nvc.man.hiddenDim;
	restirPush.useGBufferNormal = useGbuf ? 1u : 0u;
	restirPush.hasGBuffer = useGbuf ? 1u : 0u;
	restirPush.restirMode = r_nvc_restirMode ? (uint32_t)r_nvc_restirMode->integer : 1u;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.nvc.restir_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.nvc.restir_pipeline_layout, 0, 1, &vk.nvc.restir_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.nvc.restir_pipeline_layout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( restirPush ), &restirPush );
	qvkCmdDispatch( vk.cmd->command_buffer, gx, gy, 1 );

	record_image_layout_transition( vk.cmd->command_buffer, vk.nvc.direct_image,
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
		VkDescriptorImageInfo depth_img;
		VkDescriptorImageInfo direct_img;
		VkDescriptorImageInfo color_out;
		VkWriteDescriptorSet writes[3];

		if ( vk.nvc.composite_pool != VK_NULL_HANDLE ) {
			qvkDestroyDescriptorPool( vk.device, vk.nvc.composite_pool, NULL );
		}
		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[0].descriptorCount = 2;
		pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		pool_sizes[1].descriptorCount = 1;
		Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
		pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_ci.maxSets = 1;
		pool_ci.poolSizeCount = 2;
		pool_ci.pPoolSizes = pool_sizes;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.nvc.composite_pool ) );
		Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
		alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_ci.descriptorPool = vk.nvc.composite_pool;
		alloc_ci.descriptorSetCount = 1;
		alloc_ci.pSetLayouts = &vk.nvc.composite_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &vk.nvc.composite_descriptor ) );

		Com_Memset( &depth_img, 0, sizeof( depth_img ) );
		depth_img.sampler = NVC_DepthSampler();
		depth_img.imageView = depthView;
		depth_img.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		Com_Memset( &direct_img, 0, sizeof( direct_img ) );
		direct_img.sampler = NVC_LinearSampler();
		direct_img.imageView = vk.nvc.direct_view;
		direct_img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		Com_Memset( &color_out, 0, sizeof( color_out ) );
		color_out.imageView = vk.color_image_view;
		color_out.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( writes, 0, sizeof( writes ) );
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = vk.nvc.composite_descriptor;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].pImageInfo = &depth_img;
		writes[1] = writes[0];
		writes[1].dstBinding = 1;
		writes[1].pImageInfo = &direct_img;
		writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstSet = vk.nvc.composite_descriptor;
		writes[2].dstBinding = 2;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[2].pImageInfo = &color_out;
		qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );
	}

	Com_Memset( &compPush, 0, sizeof( compPush ) );
	compPush.extent[0] = (float)fullW;
	compPush.extent[1] = (float)fullH;
	compPush.params[0] = 1.0f;
	compPush.params[2] = ( r_nvc_skipSky && r_nvc_skipSky->integer ) ? 1.0f : 0.0f;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.nvc.composite_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.nvc.composite_pipeline_layout, 0, 1, &vk.nvc.composite_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.nvc.composite_pipeline_layout,
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

	if ( r_nvc_debug && r_nvc_debug->integer ) {
		ri.Printf( PRINT_DEVELOPER, "[NVC] cache/restir %ux%u composite %ux%u lights=%u\n",
			width, height, fullW, fullH, vk.forward_plus.last_packed_count );
	}
}
