/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Neural Image Space Tessellation — G-buffer silhouette refinement post-process.
See docs/NEURAL_IMAGE_SPACE_TESSELLATION.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_nist.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_view_state.h"
#include "vk_deferred_gbuffer.h"
#include "vk_staging.h"
#include "vk_cmd.h"
#include "vk_neural_io.h"

#define NIST_MANIFEST_VERSION   1
#define NIST_MAGIC_WEIGHTS      0x314E4953 /* 'NIS1' */
#define NIST_MAX_FEATURE_DIM    4
#define NIST_MAX_HIDDEN         16

typedef struct {
	int         version;
	int         featureDim;
	int         hiddenDim;
	char        weightsPath[MAX_QPATH];
} nistManifest_t;

typedef struct {
	qboolean    loaded;
	char        mapName[MAX_QPATH];
	nistManifest_t man;
	float       W1[NIST_MAX_HIDDEN * NIST_MAX_FEATURE_DIM];
	float       b1[NIST_MAX_HIDDEN];
	float       W2[NIST_MAX_HIDDEN];
	float       b2;
	uint32_t    targetWidth;
	uint32_t    targetHeight;
} nistState_t;

static nistState_t nist;

static cvar_t *r_nist;
static cvar_t *r_nist_strength;
static cvar_t *r_nist_scale;
static cvar_t *r_nist_edgeThreshold;
static cvar_t *r_nist_radius;
static cvar_t *r_nist_depthTolerance;
static cvar_t *r_nist_featureDim;
static cvar_t *r_nist_hiddenDim;
static cvar_t *r_nist_useGBuffer;
static cvar_t *r_nist_debug;
static cvar_t *r_nist_skipSky;

typedef struct {
	vec4_t extent;
	vec4_t params;
	vec4_t featureHidden;
	uint32_t useGBufferNormal;
	uint32_t hasGBuffer;
	uint32_t pad0;
	uint32_t pad1;
} vk_nist_refine_push_t;

typedef struct {
	vec4_t extent;
	vec4_t params;
} vk_nist_composite_push_t;

static VkSampler NIST_DepthSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static VkSampler NIST_LinearSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	return vk_find_sampler( &sd );
}

static void NIST_ClearGpu( void )
{
	if ( vk.nist.refine_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.nist.refine_pipeline, NULL );
		vk.nist.refine_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.nist.composite_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.nist.composite_pipeline, NULL );
		vk.nist.composite_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.nist.refine_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.nist.refine_pipeline_layout, NULL );
		vk.nist.refine_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.nist.composite_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.nist.composite_pipeline_layout, NULL );
		vk.nist.composite_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.nist.refine_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.nist.refine_layout, NULL );
		vk.nist.refine_layout = VK_NULL_HANDLE;
	}
	if ( vk.nist.composite_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.nist.composite_layout, NULL );
		vk.nist.composite_layout = VK_NULL_HANDLE;
	}
	if ( vk.nist.refine_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.nist.refine_pool, NULL );
		vk.nist.refine_pool = VK_NULL_HANDLE;
	}
	if ( vk.nist.composite_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.nist.composite_pool, NULL );
		vk.nist.composite_pool = VK_NULL_HANDLE;
	}
	if ( vk.nist.weights_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.nist.weights_buffer, NULL );
		vk.nist.weights_buffer = VK_NULL_HANDLE;
	}
	if ( vk.nist.weights_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.nist.weights_memory, NULL );
		vk.nist.weights_memory = VK_NULL_HANDLE;
	}
	if ( vk.nist.refined_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.nist.refined_image, NULL );
		vk.nist.refined_image = VK_NULL_HANDLE;
	}
	if ( vk.nist.refined_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.nist.refined_view, NULL );
		vk.nist.refined_view = VK_NULL_HANDLE;
	}
	if ( vk.nist.refined_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.nist.refined_memory, NULL );
		vk.nist.refined_memory = VK_NULL_HANDLE;
	}
	vk.nist.refine_descriptor = VK_NULL_HANDLE;
	vk.nist.composite_descriptor = VK_NULL_HANDLE;
	vk.nist.refine_ready = qfalse;
	vk.nist.composite_ready = qfalse;
	vk.nist.weights_ready = qfalse;
	vk.nistAllocated = qfalse;
}

static void NIST_BuildDefaultWeights( void )
{
	int h = nist.man.hiddenDim;
	int f = nist.man.featureDim;
	int i;

	Com_Memset( nist.W1, 0, sizeof( nist.W1 ) );
	Com_Memset( nist.b1, 0, sizeof( nist.b1 ) );
	Com_Memset( nist.W2, 0, sizeof( nist.W2 ) );
	nist.b2 = 0.15f;

	for ( i = 0; i < h; i++ ) {
		nist.b1[i] = -0.08f;
		nist.W1[i * NIST_MAX_FEATURE_DIM + 0] = 1.1f;
		nist.W1[i * NIST_MAX_FEATURE_DIM + 1] = 0.35f;
		nist.W1[i * NIST_MAX_FEATURE_DIM + 2] = -0.2f;
		nist.W1[i * NIST_MAX_FEATURE_DIM + 3] = 0.9f;
	}
	for ( i = 0; i < h; i++ ) {
		nist.W2[i] = ( i % 2 == 0 ) ? 0.7f : 0.25f;
	}
}

static qboolean NIST_ParseManifest( const char *text, nistManifest_t *man )
{
	const char *parse;
	const char *token;
	char key[64];
	char value[MAX_TOKEN_CHARS];

	Com_Memset( man, 0, sizeof( *man ) );
	man->version = NIST_MANIFEST_VERSION;
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

	if ( man->featureDim < 1 || man->featureDim > NIST_MAX_FEATURE_DIM ) {
		return qfalse;
	}
	if ( man->hiddenDim < 1 || man->hiddenDim > NIST_MAX_HIDDEN ) {
		return qfalse;
	}
	return qtrue;
}

static qboolean NIST_UploadWeightsBuffer( void )
{
	int h = nist.man.hiddenDim;
	int f = nist.man.featureDim;
	VkDeviceSize size;
	float *cpu;
	VkBufferCreateInfo buf_ci;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_ci;
	void *mapped;

	size = (VkDeviceSize)( ( h * f ) + h + h + 1 ) * sizeof( float );
	cpu = (float *)ri.Malloc( (size_t)size );
	Com_Memcpy( cpu, nist.W1, (size_t)( h * f ) * sizeof( float ) );
	Com_Memcpy( cpu + h * f, nist.b1, (size_t)h * sizeof( float ) );
	Com_Memcpy( cpu + h * f + h, nist.W2, (size_t)h * sizeof( float ) );
	cpu[h * f + h + h] = nist.b2;

	if ( vk.nist.weights_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.nist.weights_buffer, NULL );
		qvkFreeMemory( vk.device, vk.nist.weights_memory, NULL );
	}

	Com_Memset( &buf_ci, 0, sizeof( buf_ci ) );
	buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_ci.size = size;
	buf_ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_ci, NULL, &vk.nist.weights_buffer ) );

	qvkGetBufferMemoryRequirements( vk.device, vk.nist.weights_buffer, &mem_req );
	Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
	alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_ci.allocationSize = mem_req.size;
	alloc_ci.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_ci, NULL, &vk.nist.weights_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.nist.weights_buffer, vk.nist.weights_memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.nist.weights_memory, 0, size, 0, &mapped ) );
	Com_Memcpy( mapped, cpu, (size_t)size );
	qvkUnmapMemory( vk.device, vk.nist.weights_memory );
	ri.Free( cpu );
	vk.nist.weights_size = size;
	vk.nist.weights_ready = qtrue;
	return qtrue;
}

static qboolean NIST_EnsureRefinedTarget( uint32_t width, uint32_t height )
{
	if ( vk.nist.refined_image != VK_NULL_HANDLE &&
		nist.targetWidth == width && nist.targetHeight == height ) {
		return qtrue;
	}

	if ( vk.nist.refined_image != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.nist.refined_view, NULL );
		qvkDestroyImage( vk.device, vk.nist.refined_image, NULL );
		qvkFreeMemory( vk.device, vk.nist.refined_memory, NULL );
		vk.nist.refined_image = VK_NULL_HANDLE;
		vk.nist.refined_view = VK_NULL_HANDLE;
		vk.nist.refined_memory = VK_NULL_HANDLE;
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

		if ( qvkCreateImage( vk.device, &image_desc, NULL, &vk.nist.refined_image ) != VK_SUCCESS ) {
			return qfalse;
		}
		qvkGetImageMemoryRequirements( vk.device, vk.nist.refined_image, &mem_req );
		Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		if ( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.nist.refined_memory ) != VK_SUCCESS ||
			qvkBindImageMemory( vk.device, vk.nist.refined_image, vk.nist.refined_memory, 0 ) != VK_SUCCESS ) {
			qvkDestroyImage( vk.device, vk.nist.refined_image, NULL );
			vk.nist.refined_image = VK_NULL_HANDLE;
			return qfalse;
		}
		Com_Memset( &view_desc, 0, sizeof( view_desc ) );
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.image = vk.nist.refined_image;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_desc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.layerCount = 1;
		if ( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.nist.refined_view ) != VK_SUCCESS ) {
			return qfalse;
		}
	}

	nist.targetWidth = width;
	nist.targetHeight = height;
	return qtrue;
}

static void NIST_CreateRefinePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[5];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.nist.refine_ready ) {
		return;
	}
	if ( vk.modules.nist_refine_cs == VK_NULL_HANDLE ) {
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
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.nist.refine_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_nist_refine_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.nist.refine_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.nist.refine_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.nist_refine_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.nist.refine_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.nist.refine_pipeline ) );
	vk.nist.refine_ready = qtrue;
}

static void NIST_CreateCompositePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[3];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.nist.composite_ready ) {
		return;
	}
	if ( vk.modules.nist_composite_cs == VK_NULL_HANDLE ) {
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
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.nist.composite_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_nist_composite_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.nist.composite_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.nist.composite_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.nist_composite_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.nist.composite_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.nist.composite_pipeline ) );
	vk.nist.composite_ready = qtrue;
}

static void NIST_UpdateRefineDescriptors( VkImageView depthView, VkImageView normalView, VkImageView colorView )
{
	VkDescriptorPoolSize pool_sizes[2];
	VkDescriptorPoolCreateInfo pool_ci;
	VkDescriptorSetAllocateInfo alloc_ci;
	VkDescriptorImageInfo img_infos[5];
	VkDescriptorBufferInfo buf_info;
	VkWriteDescriptorSet writes[5];

	if ( vk.nist.refine_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.nist.refine_pool, NULL );
		vk.nist.refine_pool = VK_NULL_HANDLE;
		vk.nist.refine_descriptor = VK_NULL_HANDLE;
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
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.nist.refine_pool ) );

	Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
	alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_ci.descriptorPool = vk.nist.refine_pool;
	alloc_ci.descriptorSetCount = 1;
	alloc_ci.pSetLayouts = &vk.nist.refine_layout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &vk.nist.refine_descriptor ) );

	Com_Memset( img_infos, 0, sizeof( img_infos ) );
	img_infos[0].sampler = NIST_DepthSampler();
	img_infos[0].imageView = depthView;
	img_infos[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	img_infos[1].sampler = NIST_LinearSampler();
	img_infos[1].imageView = normalView ? normalView :
		( tr.whiteImage ? tr.whiteImage->view : vk.color_image_view );
	img_infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_infos[2].sampler = NIST_LinearSampler();
	img_infos[2].imageView = colorView;
	img_infos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	buf_info.buffer = vk.nist.weights_buffer;
	buf_info.offset = 0;
	buf_info.range = vk.nist.weights_size;

	img_infos[4].imageView = vk.nist.refined_view;
	img_infos[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.nist.refine_descriptor;
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
	writes[3].dstSet = vk.nist.refine_descriptor;
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

static void NIST_Cmd_Reload( void )
{
	if ( nist.mapName[0] ) {
		R_NIST_OnMapLoad( nist.mapName );
	}
}

static void NIST_Cmd_Status( void )
{
	ri.Printf( PRINT_ALL,
		"[NIST] active=%d loaded=%d hidden=%d feature=%d weights=%zuB target=%ux%u\n",
		R_NIST_Active() ? 1 : 0,
		nist.loaded ? 1 : 0,
		nist.man.hiddenDim, nist.man.featureDim,
		(size_t)vk.nist.weights_size,
		nist.targetWidth, nist.targetHeight );
}

void R_NIST_Init( void )
{
	r_nist = ri.Cvar_Get( "r_nist", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_nist_strength = ri.Cvar_Get( "r_nist_strength", "1", CVAR_ARCHIVE_ND );
	r_nist_scale = ri.Cvar_Get( "r_nist_scale", "1", CVAR_ARCHIVE_ND );
	r_nist_edgeThreshold = ri.Cvar_Get( "r_nist_edgeThreshold", "0.002", CVAR_ARCHIVE_ND );
	r_nist_radius = ri.Cvar_Get( "r_nist_radius", "4", CVAR_ARCHIVE_ND );
	r_nist_depthTolerance = ri.Cvar_Get( "r_nist_depthTolerance", "0.001", CVAR_ARCHIVE_ND );
	r_nist_featureDim = ri.Cvar_Get( "r_nist_featureDim", "4", CVAR_ARCHIVE_ND );
	r_nist_hiddenDim = ri.Cvar_Get( "r_nist_hiddenDim", "8", CVAR_ARCHIVE_ND );
	r_nist_useGBuffer = ri.Cvar_Get( "r_nist_useGBuffer", "1", CVAR_ARCHIVE_ND );
	r_nist_debug = ri.Cvar_Get( "r_nist_debug", "0", CVAR_ARCHIVE_ND );
	r_nist_skipSky = ri.Cvar_Get( "r_nist_skipSky", "1", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_nist, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_nist_scale, "0.25", "1", CV_FLOAT );
	ri.Cvar_CheckRange( r_nist_radius, "1", "8", CV_FLOAT );
	ri.Cvar_SetDescription( r_nist,
		"Neural Image Space Tessellation: screen-space silhouette smoothing (0=off, 1=on)." );
	ri.Cvar_SetDescription( r_nist_scale,
		"NIST refine resolution scale (1=full, 0.5=half) for ~1080p target cost." );

	ri.Cmd_AddCommand( "nist_reload", NIST_Cmd_Reload );
	ri.Cmd_AddCommand( "nist_status", NIST_Cmd_Status );

	if ( r_nist->integer ) {
		ri.Printf( PRINT_ALL,
			"[NIST] Neural Image Space Tessellation enabled (experimental). See docs/NEURAL_IMAGE_SPACE_TESSELLATION.md\n" );
	}
}

void R_NIST_Shutdown( void )
{
	ri.Cmd_RemoveCommand( "nist_reload" );
	ri.Cmd_RemoveCommand( "nist_status" );
	NIST_ClearGpu();
	Com_Memset( &nist, 0, sizeof( nist ) );
}

qboolean R_NIST_Active( void )
{
	return ( r_nist && r_nist->integer && nist.loaded && vk.nist.weights_ready &&
		vk.fboActive && vk.depth_image != VK_NULL_HANDLE ) ? qtrue : qfalse;
}

void R_NIST_OnMapLoad( const char *mapBaseName )
{
	byte *buf;
	int len;
	const char *tryPaths[2];
	int i;

	NIST_ClearGpu();
	Com_Memset( &nist, 0, sizeof( nist ) );

	if ( !r_nist || !r_nist->integer ) {
		return;
	}
	if ( !mapBaseName || !mapBaseName[0] ) {
		return;
	}

	Q_strncpyz( nist.mapName, mapBaseName, sizeof( nist.mapName ) );

	tryPaths[0] = va( "maps/%s.nist", mapBaseName );
	tryPaths[1] = va( "nist/%s.nist", mapBaseName );

	for ( i = 0; i < 2; i++ ) {
		len = ri.FS_ReadFile( tryPaths[i], (void **)&buf );
		if ( len > 0 && buf ) {
			if ( NIST_ParseManifest( (const char *)buf, &nist.man ) ) {
				ri.Printf( PRINT_ALL, "[NIST] Loaded manifest %s\n", tryPaths[i] );
			}
			ri.FS_FreeFile( buf );
			break;
		}
	}

	if ( r_nist_featureDim && r_nist_featureDim->integer > 0 ) {
		nist.man.featureDim = r_nist_featureDim->integer;
	}
	if ( r_nist_hiddenDim && r_nist_hiddenDim->integer > 0 ) {
		nist.man.hiddenDim = r_nist_hiddenDim->integer;
	}

	{
		char weightsPath[MAX_QPATH];
		if ( nist.man.weightsPath[0] ) {
			Q_strncpyz( weightsPath, nist.man.weightsPath, sizeof( weightsPath ) );
		} else {
			Com_sprintf( weightsPath, sizeof( weightsPath ), "nist/%s.nistb", mapBaseName );
		}
		if ( !vk_neural_load_mlp_scalar( VK_NEURAL_MAGIC_NIS1, weightsPath,
				nist.man.featureDim, nist.man.hiddenDim,
				nist.W1, NIST_MAX_FEATURE_DIM, nist.b1, nist.W2, &nist.b2,
				NIST_MAX_FEATURE_DIM, NIST_MAX_HIDDEN ) ) {
			NIST_BuildDefaultWeights();
		}
	}
	if ( !NIST_UploadWeightsBuffer() ) {
		ri.Printf( PRINT_WARNING, "[NIST] Failed to upload weights\n" );
		return;
	}

	nist.loaded = qtrue;
	vk.nistAllocated = qtrue;
	ri.Printf( PRINT_ALL, "[NIST] Ready on '%s': featureDim=%d hiddenDim=%d\n",
		mapBaseName, nist.man.featureDim, nist.man.hiddenDim );
}

void vk_nist_apply_after_geometry( void )
{
	uint32_t fullW, fullH;
	uint32_t width, height;
	VkImageView depthView;
	VkImageView normalView;
	VkImageView colorView;
	VkImageAspectFlags depth_aspect;
	vk_nist_refine_push_t refinePush;
	vk_nist_composite_push_t compPush;
	float scale;
	uint32_t gx, gy;
	qboolean useGbuf;

	if ( !R_NIST_Active() ) {
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

	scale = r_nist_scale ? r_nist_scale->value : 1.0f;
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

	if ( !NIST_EnsureRefinedTarget( width, height ) ) {
		return;
	}

	NIST_CreateRefinePipeline();
	NIST_CreateCompositePipeline();
	if ( !vk.nist.refine_ready || !vk.nist.composite_ready ) {
		return;
	}

	depthView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	colorView = vk.color_image_view;
	useGbuf = ( r_nist_useGBuffer && r_nist_useGBuffer->integer &&
		vk.deferred_gbuffer_normal_view != VK_NULL_HANDLE &&
		vk_deferred_gbuffer_fill_wanted() ) ? qtrue : qfalse;
	normalView = useGbuf ? vk.deferred_gbuffer_normal_view :
		( tr.whiteImage ? tr.whiteImage->view : VK_NULL_HANDLE );

	NIST_UpdateRefineDescriptors( depthView, normalView, colorView );

	depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	record_image_layout_transition( vk.cmd->command_buffer, vk.nist.refined_image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );

	Com_Memset( &refinePush, 0, sizeof( refinePush ) );
	refinePush.extent[0] = (float)width;
	refinePush.extent[1] = (float)height;
	refinePush.params[0] = r_nist_strength ? r_nist_strength->value : 1.0f;
	refinePush.params[1] = r_nist_edgeThreshold ? r_nist_edgeThreshold->value : 0.002f;
	refinePush.params[2] = r_nist_radius ? r_nist_radius->value : 4.0f;
	refinePush.params[3] = r_nist_depthTolerance ? r_nist_depthTolerance->value : 0.001f;
	refinePush.featureHidden[0] = (float)nist.man.featureDim;
	refinePush.featureHidden[1] = (float)nist.man.hiddenDim;
	refinePush.useGBufferNormal = useGbuf ? 1u : 0u;
	refinePush.hasGBuffer = useGbuf ? 1u : 0u;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.nist.refine_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.nist.refine_pipeline_layout, 0, 1, &vk.nist.refine_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.nist.refine_pipeline_layout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( refinePush ), &refinePush );
	gx = ( width + 7u ) / 8u;
	gy = ( height + 7u ) / 8u;
	qvkCmdDispatch( vk.cmd->command_buffer, gx, gy, 1 );

	record_image_layout_transition( vk.cmd->command_buffer, vk.nist.refined_image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );

	{
		VkDescriptorPoolSize pool_sizes[2];
		VkDescriptorPoolCreateInfo pool_ci;
		VkDescriptorSetAllocateInfo alloc_ci;
		VkDescriptorImageInfo img_infos[3];
		VkWriteDescriptorSet writes[3];
		if ( vk.nist.composite_pool != VK_NULL_HANDLE ) {
			qvkDestroyDescriptorPool( vk.device, vk.nist.composite_pool, NULL );
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
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.nist.composite_pool ) );
		Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
		alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_ci.descriptorPool = vk.nist.composite_pool;
		alloc_ci.descriptorSetCount = 1;
		alloc_ci.pSetLayouts = &vk.nist.composite_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &vk.nist.composite_descriptor ) );

		Com_Memset( img_infos, 0, sizeof( img_infos ) );
		img_infos[0].sampler = NIST_DepthSampler();
		img_infos[0].imageView = depthView;
		img_infos[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		img_infos[1].sampler = NIST_LinearSampler();
		img_infos[1].imageView = vk.nist.refined_view;
		img_infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		img_infos[2].imageView = vk.color_image_view;
		img_infos[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( writes, 0, sizeof( writes ) );
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = vk.nist.composite_descriptor;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].pImageInfo = &img_infos[0];
		writes[1] = writes[0];
		writes[1].dstBinding = 1;
		writes[1].pImageInfo = &img_infos[1];
		writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstSet = vk.nist.composite_descriptor;
		writes[2].dstBinding = 2;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[2].pImageInfo = &img_infos[2];
		qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );
	}

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );

	Com_Memset( &compPush, 0, sizeof( compPush ) );
	compPush.extent[0] = (float)fullW;
	compPush.extent[1] = (float)fullH;
	compPush.params[0] = 1.0f;
	compPush.params[2] = ( r_nist_skipSky && r_nist_skipSky->integer ) ? 1.0f : 0.0f;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.nist.composite_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.nist.composite_pipeline_layout, 0, 1, &vk.nist.composite_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.nist.composite_pipeline_layout,
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

	if ( r_nist_debug && r_nist_debug->integer ) {
		ri.Printf( PRINT_DEVELOPER, "[NIST] refine %ux%u composite %ux%u\n", width, height, fullW, fullH );
	}
}
