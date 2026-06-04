/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Forget Superresolution, Sample Adaptively (FSA) — importance-guided sub-1-SPP
path tracing with denoise (Feb 2026). See docs/FORGET_SUPERRESOLUTION_FSA.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_fsa.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_view_state.h"
#include "vk_deferred_gbuffer.h"
#include "vk_cmd.h"

#define FSA_MANIFEST_VERSION    1

typedef struct {
	int         version;
} fsaManifest_t;

typedef struct {
	qboolean    loaded;
	char        mapName[MAX_QPATH];
	uint32_t    targetWidth;
	uint32_t    targetHeight;
} fsaState_t;

static fsaState_t fsa;

static cvar_t *r_fsa;
static cvar_t *r_fsa_budget;
static cvar_t *r_fsa_strength;
static cvar_t *r_fsa_scale;
static cvar_t *r_fsa_specularWeight;
static cvar_t *r_fsa_silhouetteWeight;
static cvar_t *r_fsa_contactWeight;
static cvar_t *r_fsa_dynamicLightWeight;
static cvar_t *r_fsa_useGBuffer;
static cvar_t *r_fsa_rtxAdaptive;
static cvar_t *r_fsa_denoise;
static cvar_t *r_fsa_debug;
static cvar_t *r_fsa_skipSky;

typedef struct {
	float invViewProj[16];
	vec4_t extent;
	vec4_t weights;
	vec4_t forwardPlus;
	uint32_t useGBuffer;
	uint32_t hasGBuffer;
	uint32_t hasMaterial;
	uint32_t pad0;
} vk_fsa_importance_push_t;

typedef struct {
	vec4_t extent;
	vec4_t params;
	uint32_t useGBuffer;
	uint32_t hasGBuffer;
	uint32_t pad0;
	uint32_t pad1;
} vk_fsa_denoise_push_t;

static VkSampler FSA_DepthSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static VkSampler FSA_LinearSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	return vk_find_sampler( &sd );
}

static void FSA_ClearGpu( void )
{
	if ( vk.fsa.importance_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.fsa.importance_pipeline, NULL );
		vk.fsa.importance_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.fsa.denoise_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.fsa.denoise_pipeline, NULL );
		vk.fsa.denoise_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.fsa.importance_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.fsa.importance_pipeline_layout, NULL );
		vk.fsa.importance_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.fsa.denoise_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.fsa.denoise_pipeline_layout, NULL );
		vk.fsa.denoise_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.fsa.importance_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.fsa.importance_layout, NULL );
		vk.fsa.importance_layout = VK_NULL_HANDLE;
	}
	if ( vk.fsa.denoise_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.fsa.denoise_layout, NULL );
		vk.fsa.denoise_layout = VK_NULL_HANDLE;
	}
	if ( vk.fsa.importance_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.fsa.importance_pool, NULL );
		vk.fsa.importance_pool = VK_NULL_HANDLE;
	}
	if ( vk.fsa.denoise_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.fsa.denoise_pool, NULL );
		vk.fsa.denoise_pool = VK_NULL_HANDLE;
	}
	if ( vk.fsa.importance_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.fsa.importance_image, NULL );
		vk.fsa.importance_image = VK_NULL_HANDLE;
	}
	if ( vk.fsa.importance_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.fsa.importance_view, NULL );
		vk.fsa.importance_view = VK_NULL_HANDLE;
	}
	if ( vk.fsa.importance_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.fsa.importance_memory, NULL );
		vk.fsa.importance_memory = VK_NULL_HANDLE;
	}
	if ( vk.fsa.fallback_importance_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.fsa.fallback_importance_view, NULL );
		vk.fsa.fallback_importance_view = VK_NULL_HANDLE;
	}
	if ( vk.fsa.fallback_importance_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.fsa.fallback_importance_image, NULL );
		vk.fsa.fallback_importance_image = VK_NULL_HANDLE;
	}
	if ( vk.fsa.fallback_importance_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.fsa.fallback_importance_memory, NULL );
		vk.fsa.fallback_importance_memory = VK_NULL_HANDLE;
	}
	if ( vk.fsa.dummy_ssbo != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.fsa.dummy_ssbo, NULL );
		vk.fsa.dummy_ssbo = VK_NULL_HANDLE;
	}
	if ( vk.fsa.dummy_ssbo_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.fsa.dummy_ssbo_memory, NULL );
		vk.fsa.dummy_ssbo_memory = VK_NULL_HANDLE;
	}
	vk.fsa.importance_descriptor = VK_NULL_HANDLE;
	vk.fsa.denoise_descriptor = VK_NULL_HANDLE;
	vk.fsa.importance_ready = qfalse;
	vk.fsa.denoise_ready = qfalse;
	vk.fsa.importance_built = qfalse;
	vk.fsaAllocated = qfalse;
}

static qboolean FSA_EnsureDummySsbo( void )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements mr;
	VkMemoryAllocateInfo mai;

	if ( vk.fsa.dummy_ssbo != VK_NULL_HANDLE ) {
		return qtrue;
	}

	Com_Memset( &bci, 0, sizeof( bci ) );
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = 256;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &bci, NULL, &vk.fsa.dummy_ssbo ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.fsa.dummy_ssbo, &mr );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mr.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &mai, NULL, &vk.fsa.dummy_ssbo_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.fsa.dummy_ssbo, vk.fsa.dummy_ssbo_memory, 0 ) );
	return qtrue;
}

static qboolean FSA_EnsureFallbackImportance( void )
{
	VkImageCreateInfo image_desc;
	VkImageViewCreateInfo view_desc;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_info;
	uint32_t white = 0xFFFFFFFFu;
	VkDeviceMemory staging_mem = VK_NULL_HANDLE;
	VkBuffer staging_buf = VK_NULL_HANDLE;
	void *mapped;

	if ( vk.fsa.fallback_importance_view != VK_NULL_HANDLE ) {
		return qtrue;
	}

	Com_Memset( &image_desc, 0, sizeof( image_desc ) );
	image_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_desc.imageType = VK_IMAGE_TYPE_2D;
	image_desc.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_desc.extent.width = 1;
	image_desc.extent.height = 1;
	image_desc.extent.depth = 1;
	image_desc.mipLevels = 1;
	image_desc.arrayLayers = 1;
	image_desc.samples = VK_SAMPLE_COUNT_1_BIT;
	image_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	image_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if ( qvkCreateImage( vk.device, &image_desc, NULL, &vk.fsa.fallback_importance_image ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetImageMemoryRequirements( vk.device, vk.fsa.fallback_importance_image, &mem_req );
	Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.fsa.fallback_importance_memory ) != VK_SUCCESS ||
		qvkBindImageMemory( vk.device, vk.fsa.fallback_importance_image, vk.fsa.fallback_importance_memory, 0 ) != VK_SUCCESS ) {
		return qfalse;
	}
	Com_Memset( &view_desc, 0, sizeof( view_desc ) );
	view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_desc.image = vk.fsa.fallback_importance_image;
	view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_desc.format = VK_FORMAT_R8G8B8A8_UNORM;
	view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_desc.subresourceRange.levelCount = 1;
	view_desc.subresourceRange.layerCount = 1;
	if ( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.fsa.fallback_importance_view ) != VK_SUCCESS ) {
		return qfalse;
	}

	{
		VkBufferCreateInfo bci;
		VkMemoryAllocateInfo mai;
		VkMemoryRequirements mr;
		VkCommandBuffer cmd;
		VkCommandBufferAllocateInfo cbai;
		VkCommandPoolCreateInfo cpci;
		VkSubmitInfo si;
		VkFenceCreateInfo fci;
		VkFence fence;
		VkImageMemoryBarrier barrier;

		Com_Memset( &bci, 0, sizeof( bci ) );
		bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bci.size = 4;
		bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		VK_CHECK( qvkCreateBuffer( vk.device, &bci, NULL, &staging_buf ) );
		qvkGetBufferMemoryRequirements( vk.device, staging_buf, &mr );
		mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mai.allocationSize = mr.size;
		mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mr.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &mai, NULL, &staging_mem ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, staging_buf, staging_mem, 0 ) );
		VK_CHECK( qvkMapMemory( vk.device, staging_mem, 0, 4, 0, &mapped ) );
		Com_Memcpy( mapped, &white, 4 );
		qvkUnmapMemory( vk.device, staging_mem );

		Com_Memset( &cpci, 0, sizeof( cpci ) );
		cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cpci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		cpci.queueFamilyIndex = vk.queue_family_index;
		VK_CHECK( qvkCreateCommandPool( vk.device, &cpci, NULL, &vk.fsa.fallback_cmd_pool ) );
		Com_Memset( &cbai, 0, sizeof( cbai ) );
		cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cbai.commandPool = vk.fsa.fallback_cmd_pool;
		cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		VK_CHECK( qvkAllocateCommandBuffers( vk.device, &cbai, &cmd ) );
		{
			VkCommandBufferBeginInfo beginInfo;
			Com_Memset( &beginInfo, 0, sizeof( beginInfo ) );
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			VK_CHECK( qvkBeginCommandBuffer( cmd, &beginInfo ) );
		}
		Com_Memset( &barrier, 0, sizeof( barrier ) );
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.image = vk.fsa.fallback_importance_image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, NULL, 0, NULL, 1, &barrier );
		{
			VkBufferImageCopy region;
			Com_Memset( &region, 0, sizeof( region ) );
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.layerCount = 1;
			region.imageExtent.width = 1;
			region.imageExtent.height = 1;
			region.imageExtent.depth = 1;
			qvkCmdCopyBufferToImage( cmd, staging_buf, vk.fsa.fallback_importance_image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
		}
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, NULL, 0, NULL, 1, &barrier );
		VK_CHECK( qvkEndCommandBuffer( cmd ) );
		fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		VK_CHECK( qvkCreateFence( vk.device, &fci, NULL, &fence ) );
		si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		si.commandBufferCount = 1;
		si.pCommandBuffers = &cmd;
		VK_CHECK( qvkQueueSubmit( vk.queue, 1, &si, fence ) );
		VK_CHECK( qvkWaitForFences( vk.device, 1, &fence, VK_TRUE, UINT64_MAX ) );
		qvkDestroyFence( vk.device, fence, NULL );
		qvkFreeCommandBuffers( vk.device, vk.fsa.fallback_cmd_pool, 1, &cmd );
		qvkDestroyBuffer( vk.device, staging_buf, NULL );
		qvkFreeMemory( vk.device, staging_mem, NULL );
	}
	return qtrue;
}

static qboolean FSA_EnsureImportanceTarget( uint32_t width, uint32_t height )
{
	VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	VkImageCreateInfo image_desc;
	VkImageViewCreateInfo view_desc;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_info;

	if ( vk.fsa.importance_image != VK_NULL_HANDLE &&
		fsa.targetWidth == width && fsa.targetHeight == height ) {
		return qtrue;
	}

	if ( vk.fsa.importance_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.fsa.importance_view, NULL );
		vk.fsa.importance_view = VK_NULL_HANDLE;
	}
	if ( vk.fsa.importance_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.fsa.importance_image, NULL );
		qvkFreeMemory( vk.device, vk.fsa.importance_memory, NULL );
		vk.fsa.importance_image = VK_NULL_HANDLE;
		vk.fsa.importance_memory = VK_NULL_HANDLE;
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

	if ( qvkCreateImage( vk.device, &image_desc, NULL, &vk.fsa.importance_image ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetImageMemoryRequirements( vk.device, vk.fsa.importance_image, &mem_req );
	Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.fsa.importance_memory ) != VK_SUCCESS ||
		qvkBindImageMemory( vk.device, vk.fsa.importance_image, vk.fsa.importance_memory, 0 ) != VK_SUCCESS ) {
		return qfalse;
	}
	Com_Memset( &view_desc, 0, sizeof( view_desc ) );
	view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_desc.image = vk.fsa.importance_image;
	view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_desc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_desc.subresourceRange.levelCount = 1;
	view_desc.subresourceRange.layerCount = 1;
	if ( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.fsa.importance_view ) != VK_SUCCESS ) {
		return qfalse;
	}

	fsa.targetWidth = width;
	fsa.targetHeight = height;
	vk.fsa.importance_built = qfalse;
	return qtrue;
}

static void FSA_CreateImportancePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[8];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.fsa.importance_ready ) {
		return;
	}
	if ( vk.modules.fsa_importance_cs == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	for ( int i = 0; i < 8; i++ ) {
		bindings[i].binding = (uint32_t)i;
		bindings[i].descriptorCount = 1;
		bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 8;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.fsa.importance_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_fsa_importance_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.fsa.importance_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.fsa.importance_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.fsa_importance_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.fsa.importance_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.fsa.importance_pipeline ) );
	vk.fsa.importance_ready = qtrue;
}

static void FSA_CreateDenoisePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[4];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.fsa.denoise_ready ) {
		return;
	}
	if ( vk.modules.fsa_denoise_cs == VK_NULL_HANDLE ) {
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
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.fsa.denoise_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_fsa_denoise_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.fsa.denoise_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.fsa.denoise_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.fsa_denoise_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.fsa.denoise_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.fsa.denoise_pipeline ) );
	vk.fsa.denoise_ready = qtrue;
}

static void FSA_FillInvViewProj( float *out16 )
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

static void FSA_Cmd_Reload( void )
{
	if ( fsa.mapName[0] ) {
		R_FSA_OnMapLoad( fsa.mapName );
	}
}

static void FSA_Cmd_Status( void )
{
	ri.Printf( PRINT_ALL,
		"[FSA] active=%d importance=%ux%u budget=%.3f rtxAdaptive=%d denoise=%d\n",
		R_FSA_Active() ? 1 : 0,
		fsa.targetWidth, fsa.targetHeight,
		r_fsa_budget ? r_fsa_budget->value : 0.25f,
		vk_fsa_rtx_adaptive_wanted() ? 1 : 0,
		( r_fsa_denoise && r_fsa_denoise->integer ) ? 1 : 0 );
}

void R_FSA_Init( void )
{
	r_fsa = ri.Cvar_Get( "r_fsa", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_fsa_budget = ri.Cvar_Get( "r_fsa_budget", "0.25", CVAR_ARCHIVE_ND );
	r_fsa_strength = ri.Cvar_Get( "r_fsa_strength", "1", CVAR_ARCHIVE_ND );
	r_fsa_scale = ri.Cvar_Get( "r_fsa_scale", "1", CVAR_ARCHIVE_ND );
	r_fsa_specularWeight = ri.Cvar_Get( "r_fsa_specularWeight", "1", CVAR_ARCHIVE_ND );
	r_fsa_silhouetteWeight = ri.Cvar_Get( "r_fsa_silhouetteWeight", "1", CVAR_ARCHIVE_ND );
	r_fsa_contactWeight = ri.Cvar_Get( "r_fsa_contactWeight", "1", CVAR_ARCHIVE_ND );
	r_fsa_dynamicLightWeight = ri.Cvar_Get( "r_fsa_dynamicLightWeight", "1", CVAR_ARCHIVE_ND );
	r_fsa_useGBuffer = ri.Cvar_Get( "r_fsa_useGBuffer", "1", CVAR_ARCHIVE_ND );
	r_fsa_rtxAdaptive = ri.Cvar_Get( "r_fsa_rtxAdaptive", "1", CVAR_ARCHIVE_ND );
	r_fsa_denoise = ri.Cvar_Get( "r_fsa_denoise", "1", CVAR_ARCHIVE_ND );
	r_fsa_debug = ri.Cvar_Get( "r_fsa_debug", "0", CVAR_ARCHIVE_ND );
	r_fsa_skipSky = ri.Cvar_Get( "r_fsa_skipSky", "1", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_fsa, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_fsa_budget, "0.05", "1", CV_FLOAT );
	ri.Cvar_CheckRange( r_fsa_scale, "0.25", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_fsa,
		"Forget Superresolution / Sample Adaptively: importance-guided sub-1-SPP path trace + denoise (0=off, 1=on)." );
	ri.Cvar_SetDescription( r_fsa_budget,
		"Target path samples per pixel (may be <1); combined with importance map for stochastic RTX." );
	ri.Cvar_SetDescription( r_fsa_rtxAdaptive,
		"When r_rtxDemo 1: stochastic RTX traces weighted by FSA importance (not uniform supersampling)." );

	ri.Cmd_AddCommand( "fsa_reload", FSA_Cmd_Reload );
	ri.Cmd_AddCommand( "fsa_status", FSA_Cmd_Status );

	if ( r_fsa->integer ) {
		ri.Printf( PRINT_ALL,
			"[FSA] Forget Superresolution / Sample Adaptively enabled (experimental). See docs/FORGET_SUPERRESOLUTION_FSA.md\n" );
	}
}

void R_FSA_Shutdown( void )
{
	ri.Cmd_RemoveCommand( "fsa_reload" );
	ri.Cmd_RemoveCommand( "fsa_status" );
	FSA_ClearGpu();
	if ( vk.fsa.fallback_cmd_pool != VK_NULL_HANDLE ) {
		qvkDestroyCommandPool( vk.device, vk.fsa.fallback_cmd_pool, NULL );
		vk.fsa.fallback_cmd_pool = VK_NULL_HANDLE;
	}
	Com_Memset( &fsa, 0, sizeof( fsa ) );
}

qboolean R_FSA_Active( void )
{
	return ( r_fsa && r_fsa->integer && fsa.loaded && vk.fboActive &&
		vk.depth_image != VK_NULL_HANDLE ) ? qtrue : qfalse;
}

qboolean vk_fsa_rtx_adaptive_wanted( void )
{
#ifdef USE_VULKAN_RTX
	if ( !R_FSA_Active() || !vk.rtxAvailable ) {
		return qfalse;
	}
	if ( !r_fsa_rtxAdaptive || !r_fsa_rtxAdaptive->integer ) {
		return qfalse;
	}
	if ( !r_rtx || r_rtx->integer <= 0 || !r_rtxDemo || !r_rtxDemo->integer ) {
		return qfalse;
	}
	return vk.fsa.importance_built ? qtrue : qfalse;
#else
	return qfalse;
#endif
}

void R_FSA_OnMapLoad( const char *mapBaseName )
{
	FSA_ClearGpu();
	Com_Memset( &fsa, 0, sizeof( fsa ) );

	if ( !r_fsa || !r_fsa->integer ) {
		return;
	}
	if ( !mapBaseName || !mapBaseName[0] ) {
		return;
	}

	Q_strncpyz( fsa.mapName, mapBaseName, sizeof( fsa.mapName ) );
	FSA_EnsureDummySsbo();
	FSA_EnsureFallbackImportance();
	fsa.loaded = qtrue;
	vk.fsaAllocated = qtrue;
	ri.Printf( PRINT_ALL, "[FSA] Ready on '%s' (importance + denoise; pair with r_rtx / r_forwardPlus)\n", mapBaseName );
}

VkImageView vk_fsa_get_importance_view( void )
{
	if ( vk.fsa.importance_built && vk.fsa.importance_view != VK_NULL_HANDLE ) {
		return vk.fsa.importance_view;
	}
	return vk.fsa.fallback_importance_view;
}

void vk_fsa_patch_rtx_trace_params( float traceParams[4], uint32_t frameSeed )
{
	float budget;

	if ( !traceParams ) {
		return;
	}
	budget = r_fsa_budget ? r_fsa_budget->value : 0.25f;
	if ( budget < 0.05f ) {
		budget = 0.05f;
	}
	if ( budget > 1.0f ) {
		budget = 1.0f;
	}
	traceParams[1] = budget;
	traceParams[2] = (float)( frameSeed & 0xFFFFu );
	traceParams[3] = vk_fsa_rtx_adaptive_wanted() ? 1.0f : 0.0f;
}

void vk_fsa_write_rtx_importance_descriptor( VkDescriptorSet rtxSet )
{
	VkDescriptorImageInfo img_info;
	VkWriteDescriptorSet write;
	VkImageView view;

	if ( rtxSet == VK_NULL_HANDLE ) {
		return;
	}
	FSA_EnsureFallbackImportance();
	view = vk_fsa_get_importance_view();
	if ( view == VK_NULL_HANDLE ) {
		return;
	}
	Com_Memset( &img_info, 0, sizeof( img_info ) );
	img_info.sampler = FSA_LinearSampler();
	img_info.imageView = view;
	img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = rtxSet;
	write.dstBinding = 5;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &img_info;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
}

void vk_fsa_build_importance_after_geometry( void )
{
	uint32_t fullW, fullH, width, height;
	VkImageView depthView, normalView, materialView, colorView;
	VkImageAspectFlags depth_aspect;
	vk_fsa_importance_push_t push;
	float scale;
	uint32_t gx, gy;
	qboolean useGbuf, hasMat, hasFp;

	if ( !R_FSA_Active() ) {
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

	scale = r_fsa_scale ? r_fsa_scale->value : 1.0f;
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

	if ( !FSA_EnsureImportanceTarget( width, height ) ) {
		return;
	}

	FSA_CreateImportancePipeline();
	if ( !vk.fsa.importance_ready ) {
		return;
	}

	depthView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	colorView = vk.color_image_view;
	useGbuf = ( r_fsa_useGBuffer && r_fsa_useGBuffer->integer &&
		vk.deferred_gbuffer_normal_view != VK_NULL_HANDLE &&
		vk_deferred_gbuffer_fill_wanted() ) ? qtrue : qfalse;
	normalView = useGbuf ? vk.deferred_gbuffer_normal_view :
		( tr.whiteImage ? tr.whiteImage->view : vk.color_image_view );
	hasMat = ( vk.deferred_gbuffer_material_view != VK_NULL_HANDLE && useGbuf ) ? qtrue : qfalse;
	materialView = hasMat ? vk.deferred_gbuffer_material_view :
		( tr.whiteImage ? tr.whiteImage->view : vk.color_image_view );
	hasFp = ( r_forwardPlus && r_forwardPlus->integer &&
		vk.forward_plus.buffer != VK_NULL_HANDLE &&
		vk.forward_plus.tile_buffer != VK_NULL_HANDLE ) ? qtrue : qfalse;

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

	record_image_layout_transition( vk.cmd->command_buffer, vk.fsa.importance_image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );

	{
		VkDescriptorPoolSize pool_sizes[4];
		VkDescriptorPoolCreateInfo pool_ci;
		VkDescriptorSetAllocateInfo alloc_ci;
		VkDescriptorImageInfo depth_img, normal_img, mat_img, color_img, out_img;
		VkDescriptorBufferInfo buf_infos[3];
		VkWriteDescriptorSet writes[8];

		if ( vk.fsa.importance_pool != VK_NULL_HANDLE ) {
			qvkDestroyDescriptorPool( vk.device, vk.fsa.importance_pool, NULL );
		}
		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[0].descriptorCount = 4;
		pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		pool_sizes[1].descriptorCount = 3;
		pool_sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		pool_sizes[2].descriptorCount = 1;
		Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
		pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_ci.maxSets = 1;
		pool_ci.poolSizeCount = 3;
		pool_ci.pPoolSizes = pool_sizes;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.fsa.importance_pool ) );
		Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
		alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_ci.descriptorPool = vk.fsa.importance_pool;
		alloc_ci.descriptorSetCount = 1;
		alloc_ci.pSetLayouts = &vk.fsa.importance_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &vk.fsa.importance_descriptor ) );

		Com_Memset( &depth_img, 0, sizeof( depth_img ) );
		depth_img.sampler = FSA_DepthSampler();
		depth_img.imageView = depthView;
		depth_img.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		normal_img.sampler = FSA_LinearSampler();
		normal_img.imageView = normalView;
		normal_img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		mat_img = normal_img;
		mat_img.imageView = materialView;
		color_img = normal_img;
		color_img.imageView = colorView;
		out_img.imageView = vk.fsa.importance_view;
		out_img.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		FSA_EnsureDummySsbo();
		buf_infos[0].buffer = hasFp ? vk.forward_plus.buffer : vk.fsa.dummy_ssbo;
		buf_infos[0].offset = 0;
		buf_infos[0].range = VK_WHOLE_SIZE;
		buf_infos[1].buffer = hasFp ? vk.forward_plus.tile_buffer : vk.fsa.dummy_ssbo;
		buf_infos[1].offset = 0;
		buf_infos[1].range = VK_WHOLE_SIZE;
		buf_infos[2].buffer = hasFp ? vk.forward_plus.param_buffer : vk.fsa.dummy_ssbo;
		buf_infos[2].offset = 0;
		buf_infos[2].range = VK_WHOLE_SIZE;

		Com_Memset( writes, 0, sizeof( writes ) );
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = vk.fsa.importance_descriptor;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].pImageInfo = &depth_img;
		writes[1] = writes[0]; writes[1].dstBinding = 1; writes[1].pImageInfo = &normal_img;
		writes[2] = writes[0]; writes[2].dstBinding = 2; writes[2].pImageInfo = &mat_img;
		writes[3] = writes[0]; writes[3].dstBinding = 3; writes[3].pImageInfo = &color_img;
		writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[4].dstSet = vk.fsa.importance_descriptor;
		writes[4].dstBinding = 4;
		writes[4].descriptorCount = 1;
		writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[4].pBufferInfo = &buf_infos[0];
		writes[5] = writes[4]; writes[5].dstBinding = 5; writes[5].pBufferInfo = &buf_infos[1];
		writes[6] = writes[4]; writes[6].dstBinding = 6; writes[6].pBufferInfo = &buf_infos[2];
		writes[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[7].dstSet = vk.fsa.importance_descriptor;
		writes[7].dstBinding = 7;
		writes[7].descriptorCount = 1;
		writes[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[7].pImageInfo = &out_img;
		qvkUpdateDescriptorSets( vk.device, 8, writes, 0, NULL );
	}

	Com_Memset( &push, 0, sizeof( push ) );
	FSA_FillInvViewProj( push.invViewProj );
	push.extent[0] = (float)width;
	push.extent[1] = (float)height;
	push.weights[0] = r_fsa_specularWeight ? r_fsa_specularWeight->value : 1.0f;
	push.weights[1] = r_fsa_silhouetteWeight ? r_fsa_silhouetteWeight->value : 1.0f;
	push.weights[2] = r_fsa_contactWeight ? r_fsa_contactWeight->value : 1.0f;
	push.weights[3] = r_fsa_strength ? r_fsa_strength->value : 1.0f;
	push.forwardPlus[0] = hasFp ? 1.0f : 0.0f;
	push.forwardPlus[1] = r_fsa_dynamicLightWeight ? r_fsa_dynamicLightWeight->value : 1.0f;
	push.useGBuffer = useGbuf ? 1u : 0u;
	push.hasGBuffer = useGbuf ? 1u : 0u;
	push.hasMaterial = hasMat ? 1u : 0u;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.fsa.importance_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.fsa.importance_pipeline_layout, 0, 1, &vk.fsa.importance_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.fsa.importance_pipeline_layout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
	gx = ( width + 7u ) / 8u;
	gy = ( height + 7u ) / 8u;
	qvkCmdDispatch( vk.cmd->command_buffer, gx, gy, 1 );

	record_image_layout_transition( vk.cmd->command_buffer, vk.fsa.importance_image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );

	vk.fsa.importance_built = qtrue;

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );

	if ( r_fsa_debug && r_fsa_debug->integer ) {
		ri.Printf( PRINT_DEVELOPER, "[FSA] importance %ux%u forwardPlus=%d\n", width, height, hasFp ? 1 : 0 );
	}
}

void vk_fsa_denoise_after_rtx( VkCommandBuffer cmd )
{
	uint32_t fullW, fullH;
	VkImageView depthView;
	VkImageAspectFlags depth_aspect;
	vk_fsa_denoise_push_t push;
	uint32_t gx, gy;
	qboolean useGbuf;
	VkImageLayout colorRestore;

	if ( !R_FSA_Active() || !cmd ) {
		return;
	}
	if ( !r_fsa_denoise || !r_fsa_denoise->integer ) {
		return;
	}
	if ( !vk.fsa.importance_built || vk.color_image == VK_NULL_HANDLE ) {
		return;
	}

	fullW = vk_get_render_target_width();
	fullH = vk_get_render_target_height();
	if ( fullW == 0 || fullH == 0 ) {
		return;
	}

	FSA_CreateDenoisePipeline();
	if ( !vk.fsa.denoise_ready ) {
		return;
	}

	depthView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	useGbuf = ( r_fsa_useGBuffer && r_fsa_useGBuffer->integer &&
		vk.deferred_gbuffer_normal_view != VK_NULL_HANDLE ) ? qtrue : qfalse;

	colorRestore = ( vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) ?
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	record_depth_image_layout_transition( cmd, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		colorRestore, VK_IMAGE_LAYOUT_GENERAL,
		0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	record_image_layout_transition( cmd, vk.fsa.importance_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	{
		VkDescriptorPoolSize pool_sizes[2];
		VkDescriptorPoolCreateInfo pool_ci;
		VkDescriptorSetAllocateInfo alloc_ci;
		VkDescriptorImageInfo depth_img, normal_img, imp_img, color_out;
		VkWriteDescriptorSet writes[4];

		if ( vk.fsa.denoise_pool != VK_NULL_HANDLE ) {
			qvkDestroyDescriptorPool( vk.device, vk.fsa.denoise_pool, NULL );
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
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.fsa.denoise_pool ) );
		Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
		alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_ci.descriptorPool = vk.fsa.denoise_pool;
		alloc_ci.descriptorSetCount = 1;
		alloc_ci.pSetLayouts = &vk.fsa.denoise_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &vk.fsa.denoise_descriptor ) );

		depth_img.sampler = FSA_DepthSampler();
		depth_img.imageView = depthView;
		depth_img.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		normal_img.sampler = FSA_LinearSampler();
		normal_img.imageView = useGbuf ? vk.deferred_gbuffer_normal_view :
			( tr.whiteImage ? tr.whiteImage->view : vk.color_image_view );
		normal_img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imp_img = normal_img;
		imp_img.imageView = vk.fsa.importance_view;
		color_out.imageView = vk.color_image_view;
		color_out.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( writes, 0, sizeof( writes ) );
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = vk.fsa.denoise_descriptor;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].pImageInfo = &depth_img;
		writes[1] = writes[0]; writes[1].dstBinding = 1; writes[1].pImageInfo = &normal_img;
		writes[2] = writes[0]; writes[2].dstBinding = 2; writes[2].pImageInfo = &imp_img;
		writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].dstSet = vk.fsa.denoise_descriptor;
		writes[3].dstBinding = 3;
		writes[3].descriptorCount = 1;
		writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[3].pImageInfo = &color_out;
		qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );
	}

	Com_Memset( &push, 0, sizeof( push ) );
	push.extent[0] = (float)fullW;
	push.extent[1] = (float)fullH;
	push.params[0] = r_fsa_strength ? r_fsa_strength->value : 1.0f;
	push.params[1] = 0.85f;
	push.params[2] = ( r_fsa_skipSky && r_fsa_skipSky->integer ) ? 1.0f : 0.0f;
	push.params[3] = 0.002f;
	push.useGBuffer = useGbuf ? 1u : 0u;
	push.hasGBuffer = useGbuf ? 1u : 0u;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.fsa.denoise_pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.fsa.denoise_pipeline_layout, 0, 1, &vk.fsa.denoise_descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, vk.fsa.denoise_pipeline_layout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
	gx = ( fullW + 7u ) / 8u;
	gy = ( fullH + 7u ) / 8u;
	qvkCmdDispatch( cmd, gx, gy, 1 );

	record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, colorRestore,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0 );

	record_depth_image_layout_transition( cmd, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );
}
