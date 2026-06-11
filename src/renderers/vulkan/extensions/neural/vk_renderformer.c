/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

RenderFormer scaffold — triangle-token transport + view decode (experimental).
See docs/RENDERFORMER.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_renderformer.h"
#include "vk_renderformer_scene.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_view_state.h"
#include "vk_deferred_gbuffer.h"
#include "vk_cmd.h"

#define RF_MANIFEST_VERSION   1
#define RF_MAX_HIDDEN         16

typedef struct {
	int         version;
	int         hiddenDim;
	int         gridX;
	int         gridY;
	int         gridZ;
	float       quantUnits;
} rfManifest_t;

typedef struct {
	qboolean            loaded;
	char                mapName[MAX_QPATH];
	rfManifest_t        man;
	rfSceneData_t       scene;
	uint32_t            targetWidth;
	uint32_t            targetHeight;
	qboolean            transportValid;
} rfState_t;

static rfState_t rf;

static cvar_t *r_renderformer;
static cvar_t *r_renderformer_strength;
static cvar_t *r_renderformer_scale;
static cvar_t *r_renderformer_triCap;
static cvar_t *r_renderformer_quant;
static cvar_t *r_renderformer_gridX;
static cvar_t *r_renderformer_gridY;
static cvar_t *r_renderformer_gridZ;
static cvar_t *r_renderformer_hiddenDim;
static cvar_t *r_renderformer_useGBuffer;
static cvar_t *r_renderformer_skipSky;
static cvar_t *r_renderformer_debug;

typedef struct {
	float       worldMin[4];
	float       worldMax[4];
	uint32_t    gridDim[3];
	uint32_t    triCount;
	uint32_t    hiddenDim;
	float       strength;
} vk_rf_transport_push_t;

typedef struct {
	float       invViewProj[16];
	float       worldMin[4];
	float       worldMax[4];
	uint32_t    gridDim[3];
	uint32_t    useGBufferNormal;
	uint32_t    hasGBuffer;
	uint32_t    extent[2];
	float       cameraPos[4];
	float       strength;
} vk_rf_decode_push_t;

typedef struct {
	uint32_t    extent[2];
	float       strength;
	uint32_t    skipSky;
} vk_rf_composite_push_t;

static VkSampler RF_DepthSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static VkSampler RF_LinearSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	return vk_find_sampler( &sd );
}

static void RF_ClearGpu( void )
{
	if ( vk.renderformer.transport_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.renderformer.transport_pipeline, NULL );
		vk.renderformer.transport_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.renderformer.decode_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.renderformer.decode_pipeline, NULL );
		vk.renderformer.decode_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.renderformer.composite_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.renderformer.composite_pipeline, NULL );
		vk.renderformer.composite_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.renderformer.transport_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.renderformer.transport_pipeline_layout, NULL );
		vk.renderformer.transport_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.renderformer.decode_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.renderformer.decode_pipeline_layout, NULL );
		vk.renderformer.decode_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.renderformer.composite_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.renderformer.composite_pipeline_layout, NULL );
		vk.renderformer.composite_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.renderformer.transport_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.renderformer.transport_layout, NULL );
		vk.renderformer.transport_layout = VK_NULL_HANDLE;
	}
	if ( vk.renderformer.decode_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.renderformer.decode_layout, NULL );
		vk.renderformer.decode_layout = VK_NULL_HANDLE;
	}
	if ( vk.renderformer.composite_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.renderformer.composite_layout, NULL );
		vk.renderformer.composite_layout = VK_NULL_HANDLE;
	}
	if ( vk.renderformer.token_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.renderformer.token_buffer, NULL );
		vk.renderformer.token_buffer = VK_NULL_HANDLE;
	}
	if ( vk.renderformer.token_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.renderformer.token_memory, NULL );
		vk.renderformer.token_memory = VK_NULL_HANDLE;
	}
	if ( vk.renderformer.grid_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.renderformer.grid_buffer, NULL );
		vk.renderformer.grid_buffer = VK_NULL_HANDLE;
	}
	if ( vk.renderformer.grid_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.renderformer.grid_memory, NULL );
		vk.renderformer.grid_memory = VK_NULL_HANDLE;
	}
	if ( vk.renderformer.latent_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.renderformer.latent_buffer, NULL );
		vk.renderformer.latent_buffer = VK_NULL_HANDLE;
	}
	if ( vk.renderformer.latent_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.renderformer.latent_memory, NULL );
		vk.renderformer.latent_memory = VK_NULL_HANDLE;
	}
	if ( vk.renderformer.preview_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.renderformer.preview_image, NULL );
		vk.renderformer.preview_image = VK_NULL_HANDLE;
	}
	if ( vk.renderformer.preview_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.renderformer.preview_view, NULL );
		vk.renderformer.preview_view = VK_NULL_HANDLE;
	}
	if ( vk.renderformer.preview_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.renderformer.preview_memory, NULL );
		vk.renderformer.preview_memory = VK_NULL_HANDLE;
	}
	vk.renderformer.transport_ready = qfalse;
	vk.renderformer.decode_ready = qfalse;
	vk.renderformer.composite_ready = qfalse;
	vk.renderformer.buffers_ready = qfalse;
	vk.renderformerAllocated = qfalse;
	rf.transportValid = qfalse;
}

static qboolean RF_ParseManifest( const char *text, rfManifest_t *man )
{
	const char *parse;
	const char *token;
	char key[64];
	char value[MAX_TOKEN_CHARS];

	Com_Memset( man, 0, sizeof( *man ) );
	man->version = RF_MANIFEST_VERSION;
	man->hiddenDim = 4;
	man->gridX = 32;
	man->gridY = 32;
	man->gridZ = 32;
	man->quantUnits = 16.0f;

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
		if ( !Q_stricmp( key, "version" ) ) {
			man->version = atoi( value );
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
		}
	}
	return qtrue;
}

static qboolean RF_UploadSceneBuffers( void )
{
	VkDeviceSize tokBytes;
	VkDeviceSize gridBytes;
	VkDeviceSize latentBytes;
	VkBufferCreateInfo buf_ci;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_ci;
	void *mapped;

	if ( !rf.scene.valid || rf.scene.triangleCount == 0 ) {
		return qfalse;
	}

	tokBytes = (VkDeviceSize)rf.scene.triangleCount * sizeof( rfTriangleToken_t );
	gridBytes = (VkDeviceSize)( rf.scene.gridX * rf.scene.gridY * rf.scene.gridZ ) * sizeof( rfGridCell_t );
	latentBytes = (VkDeviceSize)( rf.scene.gridX * rf.scene.gridY * rf.scene.gridZ ) * sizeof( float ) * 4u;

	if ( vk.renderformer.token_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.renderformer.token_buffer, NULL );
		qvkFreeMemory( vk.device, vk.renderformer.token_memory, NULL );
	}
	if ( vk.renderformer.grid_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.renderformer.grid_buffer, NULL );
		qvkFreeMemory( vk.device, vk.renderformer.grid_memory, NULL );
	}
	if ( vk.renderformer.latent_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.renderformer.latent_buffer, NULL );
		qvkFreeMemory( vk.device, vk.renderformer.latent_memory, NULL );
	}

	Com_Memset( &buf_ci, 0, sizeof( buf_ci ) );
	buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_ci.size = tokBytes;
	buf_ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_ci, NULL, &vk.renderformer.token_buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.renderformer.token_buffer, &mem_req );
	Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
	alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_ci.allocationSize = mem_req.size;
	alloc_ci.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_ci, NULL, &vk.renderformer.token_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.renderformer.token_buffer, vk.renderformer.token_memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.renderformer.token_memory, 0, tokBytes, 0, &mapped ) );
	Com_Memcpy( mapped, rf.scene.tokens, (size_t)tokBytes );
	qvkUnmapMemory( vk.device, vk.renderformer.token_memory );
	vk.renderformer.triangle_count = rf.scene.triangleCount;

	buf_ci.size = gridBytes;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_ci, NULL, &vk.renderformer.grid_buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.renderformer.grid_buffer, &mem_req );
	alloc_ci.allocationSize = mem_req.size;
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_ci, NULL, &vk.renderformer.grid_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.renderformer.grid_buffer, vk.renderformer.grid_memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.renderformer.grid_memory, 0, gridBytes, 0, &mapped ) );
	Com_Memcpy( mapped, rf.scene.cells, (size_t)gridBytes );
	qvkUnmapMemory( vk.device, vk.renderformer.grid_memory );

	buf_ci.size = latentBytes;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_ci, NULL, &vk.renderformer.latent_buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.renderformer.latent_buffer, &mem_req );
	alloc_ci.allocationSize = mem_req.size;
	alloc_ci.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_ci, NULL, &vk.renderformer.latent_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.renderformer.latent_buffer, vk.renderformer.latent_memory, 0 ) );

	vk.renderformer.buffers_ready = qtrue;
	return qtrue;
}

static qboolean RF_EnsurePreviewTarget( uint32_t width, uint32_t height )
{
	if ( vk.renderformer.preview_image != VK_NULL_HANDLE &&
		rf.targetWidth == width && rf.targetHeight == height ) {
		return qtrue;
	}

	if ( vk.renderformer.preview_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.renderformer.preview_view, NULL );
		vk.renderformer.preview_view = VK_NULL_HANDLE;
	}
	if ( vk.renderformer.preview_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.renderformer.preview_image, NULL );
		qvkFreeMemory( vk.device, vk.renderformer.preview_memory, NULL );
		vk.renderformer.preview_image = VK_NULL_HANDLE;
		vk.renderformer.preview_memory = VK_NULL_HANDLE;
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

		if ( qvkCreateImage( vk.device, &image_desc, NULL, &vk.renderformer.preview_image ) != VK_SUCCESS ) {
			return qfalse;
		}
		qvkGetImageMemoryRequirements( vk.device, vk.renderformer.preview_image, &mem_req );
		Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		if ( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.renderformer.preview_memory ) != VK_SUCCESS ||
			qvkBindImageMemory( vk.device, vk.renderformer.preview_image, vk.renderformer.preview_memory, 0 ) != VK_SUCCESS ) {
			return qfalse;
		}
		Com_Memset( &view_desc, 0, sizeof( view_desc ) );
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.image = vk.renderformer.preview_image;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_desc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.layerCount = 1;
		if ( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.renderformer.preview_view ) != VK_SUCCESS ) {
			return qfalse;
		}
	}

	rf.targetWidth = width;
	rf.targetHeight = height;
	return qtrue;
}

static void RF_CreateTransportPipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[3];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.renderformer.transport_ready ) {
		return;
	}
	if ( vk.modules.rf_transport_cs == VK_NULL_HANDLE ) {
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

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 3;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.renderformer.transport_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_rf_transport_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.renderformer.transport_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.renderformer.transport_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.rf_transport_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.renderformer.transport_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.renderformer.transport_pipeline ) );
	vk.renderformer.transport_ready = qtrue;
}

static void RF_CreateDecodePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[4];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.renderformer.decode_ready ) {
		return;
	}
	if ( vk.modules.rf_decode_cs == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
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
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.renderformer.decode_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_rf_decode_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.renderformer.decode_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.renderformer.decode_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.rf_decode_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.renderformer.decode_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.renderformer.decode_pipeline ) );
	vk.renderformer.decode_ready = qtrue;
}

static void RF_CreateCompositePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[4];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.renderformer.composite_ready ) {
		return;
	}
	if ( vk.modules.rf_composite_cs == VK_NULL_HANDLE ) {
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
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.renderformer.composite_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_rf_composite_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.renderformer.composite_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.renderformer.composite_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.rf_composite_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.renderformer.composite_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.renderformer.composite_pipeline ) );
	vk.renderformer.composite_ready = qtrue;
}

static void RF_FillInvViewProj( float *out16 )
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

static void RF_Cmd_Reload( void )
{
	if ( rf.mapName[0] ) {
		R_RenderFormer_OnMapLoad( rf.mapName );
	}
}

static void RF_Cmd_Status( void )
{
	size_t tokKb = rf.scene.valid ?
		(size_t)( rf.scene.triangleCount * sizeof( rfTriangleToken_t ) ) / 1024 : 0;
	size_t gridKb = rf.scene.valid ?
		(size_t)( rf.scene.gridX * rf.scene.gridY * rf.scene.gridZ * sizeof( rfGridCell_t ) ) / 1024 : 0;

	ri.Printf( PRINT_ALL,
		"[RenderFormer] active=%d loaded=%d tris=%u grid=%ux%ux%u transport=%d mem~%zuKB+%zuKB\n",
		R_RenderFormer_Active() ? 1 : 0,
		rf.loaded ? 1 : 0,
		rf.scene.triangleCount,
		rf.scene.gridX, rf.scene.gridY, rf.scene.gridZ,
		rf.transportValid ? 1 : 0,
		tokKb, gridKb );
}

void R_RenderFormer_Init( void )
{
	r_renderformer = ri.Cvar_Get( "r_renderformer", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_renderformer_strength = ri.Cvar_Get( "r_renderformer_strength", "1", CVAR_ARCHIVE_ND );
	r_renderformer_scale = ri.Cvar_Get( "r_renderformer_scale", "1", CVAR_ARCHIVE_ND );
	r_renderformer_triCap = ri.Cvar_Get( "r_renderformer_triCap", "32768", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_renderformer_quant = ri.Cvar_Get( "r_renderformer_quant", "16", CVAR_ARCHIVE_ND );
	r_renderformer_gridX = ri.Cvar_Get( "r_renderformer_gridX", "32", CVAR_ARCHIVE_ND );
	r_renderformer_gridY = ri.Cvar_Get( "r_renderformer_gridY", "32", CVAR_ARCHIVE_ND );
	r_renderformer_gridZ = ri.Cvar_Get( "r_renderformer_gridZ", "32", CVAR_ARCHIVE_ND );
	r_renderformer_hiddenDim = ri.Cvar_Get( "r_renderformer_hiddenDim", "4", CVAR_ARCHIVE_ND );
	r_renderformer_useGBuffer = ri.Cvar_Get( "r_renderformer_useGBuffer", "1", CVAR_ARCHIVE_ND );
	r_renderformer_skipSky = ri.Cvar_Get( "r_renderformer_skipSky", "1", CVAR_ARCHIVE_ND );
	r_renderformer_debug = ri.Cvar_Get( "r_renderformer_debug", "0", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_renderformer, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_renderformer_scale, "0.25", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_renderformer,
		"RenderFormer neural mesh preview: triangle-token transport + view decode (0=off, 1=on)." );
	ri.Cvar_SetDescription( r_renderformer_triCap,
		"Max world triangles for RenderFormer token buffer (latched)." );

	ri.Cmd_AddCommand( "renderformer_reload", RF_Cmd_Reload );
	ri.Cmd_AddCommand( "renderformer_status", RF_Cmd_Status );

	if ( r_renderformer->integer ) {
		ri.Printf( PRINT_ALL,
			"[RenderFormer] Triangle-token neural preview enabled (experimental). See docs/RENDERFORMER.md\n" );
	}
}

void R_RenderFormer_Shutdown( void )
{
	ri.Cmd_RemoveCommand( "renderformer_reload" );
	ri.Cmd_RemoveCommand( "renderformer_status" );
	RF_ClearGpu();
	RF_Scene_Free( &rf.scene );
	Com_Memset( &rf, 0, sizeof( rf ) );
}

qboolean R_RenderFormer_Active( void )
{
	return ( r_renderformer && r_renderformer->integer && rf.loaded && vk.renderformer.buffers_ready &&
		vk.fboActive && vk.depth_image != VK_NULL_HANDLE ) ? qtrue : qfalse;
}

void R_RenderFormer_OnMapLoad( const char *mapBaseName )
{
	uint32_t maxTris;
	byte *buf;
	int len;
	const char *tryPaths[2];
	int i;

	RF_ClearGpu();
	RF_Scene_Free( &rf.scene );
	Com_Memset( &rf, 0, sizeof( rf ) );

	if ( !r_renderformer || !r_renderformer->integer ) {
		return;
	}
	if ( !mapBaseName || !mapBaseName[0] || !tr.world ) {
		return;
	}

	Q_strncpyz( rf.mapName, mapBaseName, sizeof( rf.mapName ) );
	rf.man.version = RF_MANIFEST_VERSION;
	rf.man.hiddenDim = 4;
	rf.man.gridX = 32;
	rf.man.gridY = 32;
	rf.man.gridZ = 32;
	rf.man.quantUnits = 16.0f;

	tryPaths[0] = va( "maps/%s.rfm", mapBaseName );
	tryPaths[1] = va( "renderformer/%s.rfm", mapBaseName );

	for ( i = 0; i < 2; i++ ) {
		len = ri.FS_ReadFile( tryPaths[i], (void **)&buf );
		if ( len > 0 && buf ) {
			if ( RF_ParseManifest( (const char *)buf, &rf.man ) ) {
				ri.Printf( PRINT_ALL, "[RenderFormer] Loaded manifest %s\n", tryPaths[i] );
			}
			ri.FS_FreeFile( buf );
			break;
		}
	}

	if ( r_renderformer_hiddenDim && r_renderformer_hiddenDim->integer > 0 ) {
		rf.man.hiddenDim = r_renderformer_hiddenDim->integer;
	}
	if ( r_renderformer_gridX && r_renderformer_gridX->integer > 0 ) {
		rf.man.gridX = r_renderformer_gridX->integer;
	}
	if ( r_renderformer_gridY && r_renderformer_gridY->integer > 0 ) {
		rf.man.gridY = r_renderformer_gridY->integer;
	}
	if ( r_renderformer_gridZ && r_renderformer_gridZ->integer > 0 ) {
		rf.man.gridZ = r_renderformer_gridZ->integer;
	}

	maxTris = r_renderformer_triCap ? (uint32_t)r_renderformer_triCap->integer : 32768u;
	if ( maxTris > RF_MAX_TRIANGLES ) {
		maxTris = RF_MAX_TRIANGLES;
	}
	if ( maxTris < 256 ) {
		maxTris = 256;
	}

	if ( !RF_Scene_BuildFromWorld( &rf.scene, tr.world, maxTris,
			(uint32_t)rf.man.gridX, (uint32_t)rf.man.gridY, (uint32_t)rf.man.gridZ ) ) {
		ri.Printf( PRINT_WARNING, "[RenderFormer] Failed to build triangle tokens for '%s'\n", mapBaseName );
		return;
	}

	if ( !RF_UploadSceneBuffers() ) {
		ri.Printf( PRINT_WARNING, "[RenderFormer] GPU upload failed\n" );
		RF_Scene_Free( &rf.scene );
		return;
	}

	rf.loaded = qtrue;
	vk.renderformerAllocated = qtrue;
	ri.Printf( PRINT_ALL,
		"[RenderFormer] Ready on '%s': %u triangles, grid %ux%ux%u (~%zuKB tokens + ~%zuKB index)\n",
		mapBaseName, rf.scene.triangleCount,
		rf.scene.gridX, rf.scene.gridY, rf.scene.gridZ,
		(size_t)( rf.scene.triangleCount * sizeof( rfTriangleToken_t ) ) / 1024,
		(size_t)( rf.scene.gridX * rf.scene.gridY * rf.scene.gridZ * sizeof( rfGridCell_t ) ) / 1024 );
}

void vk_renderformer_apply_after_geometry( void )
{
	uint32_t fullW, fullH, width, height;
	VkImageView depthView, normalView, albedoView;
	VkImageAspectFlags depth_aspect;
	vk_rf_transport_push_t transportPush;
	vk_rf_decode_push_t decodePush;
	vk_rf_composite_push_t compPush;
	float scale;
	qboolean useGbuf;

	if ( !R_RenderFormer_Active() ) {
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

	scale = r_renderformer_scale ? r_renderformer_scale->value : 1.0f;
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

	if ( !RF_EnsurePreviewTarget( width, height ) ) {
		return;
	}

	RF_CreateTransportPipeline();
	RF_CreateDecodePipeline();
	RF_CreateCompositePipeline();
	if ( !vk.renderformer.transport_ready || !vk.renderformer.decode_ready ||
		!vk.renderformer.composite_ready ) {
		return;
	}

	depthView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	useGbuf = ( r_renderformer_useGBuffer && r_renderformer_useGBuffer->integer &&
		vk.deferred_gbuffer_normal_view != VK_NULL_HANDLE &&
		vk_deferred_gbuffer_fill_wanted() ) ? qtrue : qfalse;
	normalView = useGbuf ? vk.deferred_gbuffer_normal_view :
		( tr.whiteImage ? tr.whiteImage->view : VK_NULL_HANDLE );
	albedoView = ( vk.deferred_gbuffer_albedo_view != VK_NULL_HANDLE && useGbuf ) ?
		vk.deferred_gbuffer_albedo_view : vk.color_image_view;

	depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	record_image_layout_transition( vk.cmd->command_buffer, vk.renderformer.preview_image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );

	/* Transport pass */
	{
		VkDescriptorPoolSize pool_sizes[1];
		VkDescriptorPoolCreateInfo pool_ci;
		VkDescriptorSetAllocateInfo alloc_ci;
		VkDescriptorBufferInfo buf_infos[3];
		VkWriteDescriptorSet writes[3];
		VkDescriptorPool transport_pool;
		VkDescriptorSet transport_desc;
		uint32_t cellCount;
		uint32_t groups;

		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		pool_sizes[0].descriptorCount = 3;
		Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
		pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_ci.maxSets = 1;
		pool_ci.poolSizeCount = 1;
		pool_ci.pPoolSizes = pool_sizes;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &transport_pool ) );
		Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
		alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_ci.descriptorPool = transport_pool;
		alloc_ci.descriptorSetCount = 1;
		alloc_ci.pSetLayouts = &vk.renderformer.transport_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &transport_desc ) );

		buf_infos[0].buffer = vk.renderformer.token_buffer;
		buf_infos[0].offset = 0;
		buf_infos[0].range = VK_WHOLE_SIZE;
		buf_infos[1].buffer = vk.renderformer.grid_buffer;
		buf_infos[1].offset = 0;
		buf_infos[1].range = VK_WHOLE_SIZE;
		buf_infos[2].buffer = vk.renderformer.latent_buffer;
		buf_infos[2].offset = 0;
		buf_infos[2].range = VK_WHOLE_SIZE;

		Com_Memset( writes, 0, sizeof( writes ) );
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = transport_desc;
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
		qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );

		Com_Memset( &transportPush, 0, sizeof( transportPush ) );
		transportPush.worldMin[0] = rf.scene.worldMin[0];
		transportPush.worldMin[1] = rf.scene.worldMin[1];
		transportPush.worldMin[2] = rf.scene.worldMin[2];
		transportPush.worldMax[0] = rf.scene.worldMax[0];
		transportPush.worldMax[1] = rf.scene.worldMax[1];
		transportPush.worldMax[2] = rf.scene.worldMax[2];
		transportPush.gridDim[0] = rf.scene.gridX;
		transportPush.gridDim[1] = rf.scene.gridY;
		transportPush.gridDim[2] = rf.scene.gridZ;
		transportPush.triCount = vk.renderformer.triangle_count;
		transportPush.hiddenDim = (uint32_t)rf.man.hiddenDim;
		transportPush.strength = r_renderformer_strength ? r_renderformer_strength->value : 1.0f;

		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
			vk.renderformer.transport_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
			vk.renderformer.transport_pipeline_layout, 0, 1, &transport_desc, 0, NULL );
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.renderformer.transport_pipeline_layout,
			VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( transportPush ), &transportPush );

		cellCount = rf.scene.gridX * rf.scene.gridY * rf.scene.gridZ;
		groups = ( cellCount + 63u ) / 64u;
		qvkCmdDispatch( vk.cmd->command_buffer, groups, 1, 1 );

		{
			VkBufferMemoryBarrier bufBarrier;
			Com_Memset( &bufBarrier, 0, sizeof( bufBarrier ) );
			bufBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			bufBarrier.buffer = vk.renderformer.latent_buffer;
			bufBarrier.size = VK_WHOLE_SIZE;
			qvkCmdPipelineBarrier( vk.cmd->command_buffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0, 0, NULL, 1, &bufBarrier, 0, NULL );
		}

		qvkDestroyDescriptorPool( vk.device, transport_pool, NULL );
		rf.transportValid = qtrue;
	}

	/* Decode pass */
	{
		VkDescriptorPoolSize ps[3];
		VkDescriptorPoolCreateInfo pool_ci;
		VkDescriptorSetAllocateInfo alloc_ci;
		VkDescriptorBufferInfo latent_info;
		VkDescriptorImageInfo depth_img, normal_img, out_img;
		VkWriteDescriptorSet writes[4];
		VkDescriptorPool decode_pool;
		VkDescriptorSet decode_desc;

		ps[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		ps[0].descriptorCount = 1;
		ps[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		ps[1].descriptorCount = 2;
		ps[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		ps[2].descriptorCount = 1;
		Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
		pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_ci.maxSets = 1;
		pool_ci.poolSizeCount = 3;
		pool_ci.pPoolSizes = ps;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &decode_pool ) );
		Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
		alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_ci.descriptorPool = decode_pool;
		alloc_ci.descriptorSetCount = 1;
		alloc_ci.pSetLayouts = &vk.renderformer.decode_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &decode_desc ) );

		latent_info.buffer = vk.renderformer.latent_buffer;
		latent_info.offset = 0;
		latent_info.range = VK_WHOLE_SIZE;
		depth_img.sampler = RF_DepthSampler();
		depth_img.imageView = depthView;
		depth_img.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		normal_img.sampler = RF_LinearSampler();
		normal_img.imageView = normalView;
		normal_img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		out_img.imageView = vk.renderformer.preview_view;
		out_img.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( writes, 0, sizeof( writes ) );
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = decode_desc;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[0].pBufferInfo = &latent_info;
		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = decode_desc;
		writes[1].dstBinding = 1;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].pImageInfo = &depth_img;
		writes[2] = writes[1];
		writes[2].dstBinding = 2;
		writes[2].pImageInfo = &normal_img;
		writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].dstSet = decode_desc;
		writes[3].dstBinding = 3;
		writes[3].descriptorCount = 1;
		writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[3].pImageInfo = &out_img;
		qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );

		Com_Memset( &decodePush, 0, sizeof( decodePush ) );
		RF_FillInvViewProj( decodePush.invViewProj );
		decodePush.worldMin[0] = rf.scene.worldMin[0];
		decodePush.worldMin[1] = rf.scene.worldMin[1];
		decodePush.worldMin[2] = rf.scene.worldMin[2];
		decodePush.worldMax[0] = rf.scene.worldMax[0];
		decodePush.worldMax[1] = rf.scene.worldMax[1];
		decodePush.worldMax[2] = rf.scene.worldMax[2];
		decodePush.gridDim[0] = rf.scene.gridX;
		decodePush.gridDim[1] = rf.scene.gridY;
		decodePush.gridDim[2] = rf.scene.gridZ;
		decodePush.useGBufferNormal = useGbuf ? 1u : 0u;
		decodePush.hasGBuffer = useGbuf ? 1u : 0u;
		decodePush.extent[0] = width;
		decodePush.extent[1] = height;
		VectorCopy( backEnd.viewParms.or.origin, decodePush.cameraPos );
		decodePush.cameraPos[3] = 1.0f;
		decodePush.strength = r_renderformer_strength ? r_renderformer_strength->value : 1.0f;

		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
			vk.renderformer.decode_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
			vk.renderformer.decode_pipeline_layout, 0, 1, &decode_desc, 0, NULL );
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.renderformer.decode_pipeline_layout,
			VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( decodePush ), &decodePush );
		qvkCmdDispatch( vk.cmd->command_buffer, ( width + 7 ) / 8, ( height + 7 ) / 8, 1 );
		qvkDestroyDescriptorPool( vk.device, decode_pool, NULL );
	}

	record_image_layout_transition( vk.cmd->command_buffer, vk.renderformer.preview_image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	/* Composite pass */
	{
		VkDescriptorPoolSize ps[3];
		VkDescriptorPoolCreateInfo pool_ci;
		VkDescriptorSetAllocateInfo alloc_ci;
		VkDescriptorImageInfo depth_img, albedo_img, prev_img, out_img;
		VkWriteDescriptorSet writes[4];
		VkDescriptorPool composite_pool;
		VkDescriptorSet composite_desc;

		ps[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		ps[0].descriptorCount = 3;
		ps[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		ps[1].descriptorCount = 1;
		Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
		pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_ci.maxSets = 1;
		pool_ci.poolSizeCount = 2;
		pool_ci.pPoolSizes = ps;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &composite_pool ) );
		Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
		alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_ci.descriptorPool = composite_pool;
		alloc_ci.descriptorSetCount = 1;
		alloc_ci.pSetLayouts = &vk.renderformer.composite_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &composite_desc ) );

		depth_img.sampler = RF_DepthSampler();
		depth_img.imageView = depthView;
		depth_img.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		albedo_img.sampler = RF_LinearSampler();
		albedo_img.imageView = albedoView;
		albedo_img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		prev_img.sampler = RF_LinearSampler();
		prev_img.imageView = vk.renderformer.preview_view;
		prev_img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		out_img.imageView = vk.color_image_view;
		out_img.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( writes, 0, sizeof( writes ) );
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = composite_desc;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].pImageInfo = &depth_img;
		writes[1] = writes[0];
		writes[1].dstBinding = 1;
		writes[1].pImageInfo = &albedo_img;
		writes[2] = writes[0];
		writes[2].dstBinding = 2;
		writes[2].pImageInfo = &prev_img;
		writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].dstSet = composite_desc;
		writes[3].dstBinding = 3;
		writes[3].descriptorCount = 1;
		writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[3].pImageInfo = &out_img;
		qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );

		Com_Memset( &compPush, 0, sizeof( compPush ) );
		compPush.extent[0] = fullW;
		compPush.extent[1] = fullH;
		compPush.strength = r_renderformer_strength ? r_renderformer_strength->value : 1.0f;
		compPush.skipSky = ( r_renderformer_skipSky && r_renderformer_skipSky->integer ) ? 1u : 0u;

		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
			vk.renderformer.composite_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
			vk.renderformer.composite_pipeline_layout, 0, 1, &composite_desc, 0, NULL );
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.renderformer.composite_pipeline_layout,
			VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( compPush ), &compPush );
		qvkCmdDispatch( vk.cmd->command_buffer, ( fullW + 7 ) / 8, ( fullH + 7 ) / 8, 1 );
		qvkDestroyDescriptorPool( vk.device, composite_pool, NULL );
	}

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );

	if ( r_renderformer_debug && r_renderformer_debug->integer ) {
		ri.Printf( PRINT_DEVELOPER, "[RenderFormer] frame transport+decode+composite %ux%u (decode %ux%u)\n",
			fullW, fullH, width, height );
	}
}
