/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

VkSplat — end-to-end 3DGS training scaffold in Vulkan compute.
Chen, Ibrahim & Liu, Eurographics 2026 / arXiv:2605.00219.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_vksplat.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_cmd.h"
#include "vk_view_state.h"

#define VKSPLAT_MAX_GAUSSIANS       8192u
#define VKSPLAT_MAX_KEYS_PER_G      64u
#define VKSPLAT_TILE_SIZE           16
#define VKSPLAT_GAUSSIAN_STRIDE     64u

typedef struct {
	float mean[3];
	float opacity;
	float scaleLog[3];
	float pad0;
	float rotation[4];
	float sh0[3];
	float pad1;
} vksplatGaussian_t;

typedef struct {
	uint32_t train_width;
	uint32_t train_height;
	uint32_t gaussian_count;
	uint32_t train_steps;
	qboolean ready;
} vksplatState_t;

static vksplatState_t vksplat;

static cvar_t *r_vksplat;
static cvar_t *r_vksplat_gaussians;
static cvar_t *r_vksplat_lr;
static cvar_t *r_vksplat_bwdMode;
static cvar_t *r_vksplat_debug;

static void VKSplat_DestroyGpu( void );
static void VKSplat_EnsurePipelines( void );
static qboolean VKSplat_EnsureBuffers( void );
static qboolean VKSplat_SeedGaussians( void );

typedef struct {
	VkDescriptorSetLayout layout;
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
	VkDescriptorPool pool;
	VkDescriptorSet descriptor;
	qboolean ready;
} vksplatPipeline_t;

static vksplatPipeline_t vksplat_project;
static vksplatPipeline_t vksplat_tile;
static vksplatPipeline_t vksplat_raster;
static vksplatPipeline_t vksplat_adam;

static void VKSplat_DestroyPipeline( vksplatPipeline_t *p )
{
	if ( !p ) {
		return;
	}
	if ( p->pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, p->pipeline, NULL );
	}
	if ( p->pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, p->pipeline_layout, NULL );
	}
	if ( p->layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, p->layout, NULL );
	}
	if ( p->pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, p->pool, NULL );
	}
	Com_Memset( p, 0, sizeof( *p ) );
}

static qboolean VKSplat_CreateComputePipeline( vksplatPipeline_t *p, VkShaderModule module,
	const VkDescriptorSetLayoutBinding *bindings, uint32_t bindingCount, uint32_t pushSize )
{
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;
	VkDescriptorPoolCreateInfo pci;
	VkDescriptorSetAllocateInfo ai;

	if ( !p || module == VK_NULL_HANDLE || p->ready ) {
		return qfalse;
	}

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = bindingCount;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &p->layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = pushSize;

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &p->layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &p->pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = module;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = p->pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &p->pipeline ) );

	{
		uint32_t bufCount = 0;
		uint32_t imgCount = 0;
		uint32_t poolCount = 0;
		uint32_t i;
		VkDescriptorPoolSize poolSizes[2];

		for ( i = 0; i < bindingCount; i++ ) {
			if ( bindings[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ) {
				bufCount += bindings[i].descriptorCount;
			} else if ( bindings[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ) {
				imgCount += bindings[i].descriptorCount;
			}
		}
		poolCount = 0;
		if ( bufCount > 0 ) {
			poolSizes[poolCount].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			poolSizes[poolCount].descriptorCount = bufCount;
			poolCount++;
		}
		if ( imgCount > 0 ) {
			poolSizes[poolCount].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			poolSizes[poolCount].descriptorCount = imgCount;
			poolCount++;
		}
		if ( poolCount == 0 ) {
			poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			poolSizes[0].descriptorCount = 1;
			poolCount = 1;
		}
		Com_Memset( &pci, 0, sizeof( pci ) );
		pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pci.maxSets = 1;
		pci.poolSizeCount = poolCount;
		pci.pPoolSizes = poolSizes;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pci, NULL, &p->pool ) );
	}
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.descriptorPool = p->pool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &p->layout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &ai, &p->descriptor ) );

	p->ready = qtrue;
	return qtrue;
}

static void VKSplat_EnsurePipelines( void )
{
	VkDescriptorSetLayoutBinding bindings[2];

	if ( vksplat_project.ready ) {
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
	(void)VKSplat_CreateComputePipeline( &vksplat_project, vk.modules.vksplat_project_fwd_cs,
		bindings, 2, 80 );

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	(void)VKSplat_CreateComputePipeline( &vksplat_tile, vk.modules.vksplat_tile_cull_cs,
		bindings, 2, 32 );

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	(void)VKSplat_CreateComputePipeline( &vksplat_raster, vk.modules.vksplat_raster_fwd_cs,
		bindings, 2, 32 );

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	(void)VKSplat_CreateComputePipeline( &vksplat_adam, vk.modules.vksplat_adam_cs,
		bindings, 1, 32 );
}

static qboolean VKSplat_CreateBuffer( VkDeviceSize size, VkBufferUsageFlags usage,
	VkBuffer *outBuf, VkDeviceMemory *outMem )
{
	VkBufferCreateInfo buf_ci;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo alloc_ci;

	Com_Memset( &buf_ci, 0, sizeof( buf_ci ) );
	buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_ci.size = size;
	buf_ci.usage = usage;
	buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_ci, NULL, outBuf ) );
	qvkGetBufferMemoryRequirements( vk.device, *outBuf, &req );
	Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
	alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_ci.allocationSize = req.size;
	alloc_ci.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_ci, NULL, outMem ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, *outBuf, *outMem, 0 ) );
	return qtrue;
}

static qboolean VKSplat_EnsureRenderTarget( uint32_t w, uint32_t h )
{
	if ( vk.vksplat.render_image != VK_NULL_HANDLE &&
		vksplat.train_width == w && vksplat.train_height == h ) {
		return qtrue;
	}

	if ( vk.vksplat.render_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.vksplat.render_view, NULL );
		qvkDestroyImage( vk.device, vk.vksplat.render_image, NULL );
		qvkFreeMemory( vk.device, vk.vksplat.render_memory, NULL );
		vk.vksplat.render_view = VK_NULL_HANDLE;
		vk.vksplat.render_image = VK_NULL_HANDLE;
		vk.vksplat.render_memory = VK_NULL_HANDLE;
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
		image_desc.extent.width = w;
		image_desc.extent.height = h;
		image_desc.extent.depth = 1;
		image_desc.mipLevels = 1;
		image_desc.arrayLayers = 1;
		image_desc.samples = VK_SAMPLE_COUNT_1_BIT;
		image_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_desc.usage = VK_IMAGE_USAGE_STORAGE_BIT;
		image_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VK_CHECK( qvkCreateImage( vk.device, &image_desc, NULL, &vk.vksplat.render_image ) );
		qvkGetImageMemoryRequirements( vk.device, vk.vksplat.render_image, &mem_req );
		Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.vksplat.render_memory ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.vksplat.render_image, vk.vksplat.render_memory, 0 ) );
		Com_Memset( &view_desc, 0, sizeof( view_desc ) );
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.image = vk.vksplat.render_image;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_desc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.layerCount = 1;
		VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.vksplat.render_view ) );
	}

	vksplat.train_width = w;
	vksplat.train_height = h;
	return qtrue;
}

static qboolean VKSplat_EnsureBuffers( void )
{
	VkDeviceSize gaussBytes;
	VkDeviceSize projBytes;
	VkDeviceSize keyBytes;
	uint32_t count;

	count = r_vksplat_gaussians ? (uint32_t)r_vksplat_gaussians->integer : 4096u;
	if ( count < 64u ) {
		count = 64u;
	}
	if ( count > VKSPLAT_MAX_GAUSSIANS ) {
		count = VKSPLAT_MAX_GAUSSIANS;
	}

	if ( vk.vksplat.gaussian_buffer != VK_NULL_HANDLE && vksplat.gaussian_count == count ) {
		return qtrue;
	}

	VKSplat_DestroyGpu();

	gaussBytes = (VkDeviceSize)count * VKSPLAT_GAUSSIAN_STRIDE;
	projBytes = (VkDeviceSize)count * 48u;
	keyBytes = (VkDeviceSize)count * VKSPLAT_MAX_KEYS_PER_G * sizeof( uint32_t );

	{
		VkBufferCreateInfo bi;
		VkMemoryRequirements req;
		VkMemoryAllocateInfo ai;

		Com_Memset( &bi, 0, sizeof( bi ) );
		bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bi.size = gaussBytes;
		bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, &vk.vksplat.gaussian_buffer ) );
		qvkGetBufferMemoryRequirements( vk.device, vk.vksplat.gaussian_buffer, &req );
		Com_Memset( &ai, 0, sizeof( ai ) );
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &vk.vksplat.gaussian_memory ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, vk.vksplat.gaussian_buffer, vk.vksplat.gaussian_memory, 0 ) );
	}
	if ( !VKSplat_CreateBuffer( projBytes,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			&vk.vksplat.projected_buffer, &vk.vksplat.projected_memory ) ) {
		return qfalse;
	}
	if ( !VKSplat_CreateBuffer( keyBytes,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			&vk.vksplat.sortkey_buffer, &vk.vksplat.sortkey_memory ) ) {
		return qfalse;
	}

	vksplat.gaussian_count = count;
	vksplat.ready = qtrue;
	return VKSplat_SeedGaussians();
}

static qboolean VKSplat_SeedGaussians( void )
{
	vksplatGaussian_t *host;
	uint32_t i;
	void *mapped;
	VkDeviceSize size;

	size = (VkDeviceSize)vksplat.gaussian_count * sizeof( vksplatGaussian_t );
	host = (vksplatGaussian_t *)ri.Hunk_AllocateTempMemory( (int)size );
	if ( !host ) {
		return qfalse;
	}

	for ( i = 0; i < vksplat.gaussian_count; i++ ) {
		float t = (float)i / (float)vksplat.gaussian_count;
		host[i].mean[0] = sinf( t * 6.283f ) * 32.0f;
		host[i].mean[1] = cosf( t * 3.141f ) * 16.0f;
		host[i].mean[2] = 64.0f + sinf( t * 12.0f ) * 8.0f;
		host[i].opacity = 0.0f;
		host[i].scaleLog[0] = -2.0f;
		host[i].scaleLog[1] = -2.0f;
		host[i].scaleLog[2] = -2.0f;
		host[i].rotation[0] = 1.0f;
		host[i].sh0[0] = 0.4f + 0.2f * sinf( t * 9.0f );
		host[i].sh0[1] = 0.4f + 0.2f * cosf( t * 7.0f );
		host[i].sh0[2] = 0.45f;
	}

	if ( qvkMapMemory( vk.device, vk.vksplat.gaussian_memory, 0, size, 0, &mapped ) != VK_SUCCESS ) {
		ri.Hunk_FreeTempMemory( host );
		return qfalse;
	}
	Com_Memcpy( mapped, host, (size_t)size );
	qvkUnmapMemory( vk.device, vk.vksplat.gaussian_memory );
	ri.Hunk_FreeTempMemory( host );
	return qtrue;
}

static void VKSplat_DestroyGpu( void )
{
	if ( vk.vksplat.gaussian_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.vksplat.gaussian_buffer, NULL );
		qvkFreeMemory( vk.device, vk.vksplat.gaussian_memory, NULL );
	}
	if ( vk.vksplat.projected_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.vksplat.projected_buffer, NULL );
		qvkFreeMemory( vk.device, vk.vksplat.projected_memory, NULL );
	}
	if ( vk.vksplat.sortkey_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.vksplat.sortkey_buffer, NULL );
		qvkFreeMemory( vk.device, vk.vksplat.sortkey_memory, NULL );
	}
	if ( vk.vksplat.render_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.vksplat.render_view, NULL );
		qvkDestroyImage( vk.device, vk.vksplat.render_image, NULL );
		qvkFreeMemory( vk.device, vk.vksplat.render_memory, NULL );
	}
	Com_Memset( &vk.vksplat, 0, sizeof( vk.vksplat ) );
	vksplat.ready = qfalse;
	vksplat.gaussian_count = 0;
}

static qboolean VKSplat_RunOneStep( void )
{
	VkCommandBuffer cmd;
	float viewProj[16];
	const float *view;
	const float *projection;
	float proj_vk[16];
	struct {
		float viewProj[16];
		float viewport[4];
		int gaussianCount;
		int tileSize;
		float opacityThreshold;
		float pad0;
	} projectPush;
	struct {
		float viewport[4];
		int gaussianCount;
		int tileSize;
		int tileCols;
		int tileRows;
		int maxKeysPerGaussian;
	} tilePush;
	struct {
		float viewport[4];
		int gaussianCount;
		int tileSize;
		int tileCols;
	} rasterPush;
	struct {
		int gaussianCount;
		float lr;
		float l1Weight;
		float beta1;
		float beta2;
	} adamPush;
	VkDescriptorBufferInfo gaussInfo, projInfo, keyInfo;
	VkDescriptorImageInfo renderInfo;
	VkWriteDescriptorSet writes[3];
	VkImageMemoryBarrier barrier;
	uint32_t groups;
	uint32_t tileCols;
	uint32_t tileRows;
	int w;
	int h;

	if ( !vk.device || vk.device_lost ) {
		return qfalse;
	}

	w = 640;
	h = 480;
	if ( !VKSplat_EnsureRenderTarget( (uint32_t)w, (uint32_t)h ) ||
		!VKSplat_EnsureBuffers() ) {
		return qfalse;
	}
	VKSplat_EnsurePipelines();
	if ( !vksplat_project.ready || !vksplat_tile.ready || !vksplat_raster.ready || !vksplat_adam.ready ) {
		ri.Printf( PRINT_WARNING, "[VkSplat] Pipelines not ready (compile shaders)\n" );
		return qfalse;
	}

	tileCols = ( (uint32_t)w + VKSPLAT_TILE_SIZE - 1u ) / VKSPLAT_TILE_SIZE;
	tileRows = ( (uint32_t)h + VKSPLAT_TILE_SIZE - 1u ) / VKSPLAT_TILE_SIZE;

	cmd = vk_begin_command_buffer();
	if ( cmd == VK_NULL_HANDLE ) {
		return qfalse;
	}

	view = backEnd.viewParms.world.modelViewMatrix;
	projection = backEnd.useFirstPersonProjection ?
		backEnd.firstPersonProjectionMatrix : backEnd.viewParms.projectionMatrix;
	vk_get_projection_matrix_vk( projection, proj_vk );
	myGlMultMatrix( view, proj_vk, viewProj );

	Com_Memset( &projectPush, 0, sizeof( projectPush ) );
	Com_Memcpy( projectPush.viewProj, viewProj, sizeof( projectPush.viewProj ) );
	projectPush.viewport[0] = (float)w;
	projectPush.viewport[1] = (float)h;
	projectPush.viewport[2] = (float)tileCols;
	projectPush.viewport[3] = (float)tileRows;
	projectPush.gaussianCount = (int)vksplat.gaussian_count;
	projectPush.tileSize = VKSPLAT_TILE_SIZE;
	projectPush.opacityThreshold = 0.01f;

	gaussInfo.buffer = vk.vksplat.gaussian_buffer;
	gaussInfo.offset = 0;
	gaussInfo.range = VK_WHOLE_SIZE;
	projInfo.buffer = vk.vksplat.projected_buffer;
	projInfo.offset = 0;
	projInfo.range = VK_WHOLE_SIZE;
	keyInfo.buffer = vk.vksplat.sortkey_buffer;
	keyInfo.offset = 0;
	keyInfo.range = VK_WHOLE_SIZE;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vksplat_project.descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &gaussInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vksplat_project.descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].pBufferInfo = &projInfo;
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vksplat_project.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vksplat_project.pipeline_layout,
		0, 1, &vksplat_project.descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, vksplat_project.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( projectPush ), &projectPush );
	groups = ( vksplat.gaussian_count + 63u ) / 64u;
	qvkCmdDispatch( cmd, groups, 1, 1 );

	writes[0].dstSet = vksplat_tile.descriptor;
	writes[1].dstSet = vksplat_tile.descriptor;
	writes[1].pBufferInfo = &keyInfo;
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	Com_Memset( &tilePush, 0, sizeof( tilePush ) );
	tilePush.viewport[0] = (float)w;
	tilePush.viewport[1] = (float)h;
	tilePush.gaussianCount = (int)vksplat.gaussian_count;
	tilePush.tileSize = VKSPLAT_TILE_SIZE;
	tilePush.tileCols = (int)tileCols;
	tilePush.tileRows = (int)tileRows;
	tilePush.maxKeysPerGaussian = (int)VKSPLAT_MAX_KEYS_PER_G;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vksplat_tile.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vksplat_tile.pipeline_layout,
		0, 1, &vksplat_tile.descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, vksplat_tile.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( tilePush ), &tilePush );
	qvkCmdDispatch( cmd, groups, 1, 1 );

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.image = vk.vksplat.render_image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );

	renderInfo.imageView = vk.vksplat.render_view;
	renderInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	writes[0].dstSet = vksplat_raster.descriptor;
	writes[0].pBufferInfo = &projInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vksplat_raster.descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[1].pImageInfo = &renderInfo;
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	Com_Memset( &rasterPush, 0, sizeof( rasterPush ) );
	rasterPush.viewport[0] = (float)w;
	rasterPush.viewport[1] = (float)h;
	rasterPush.gaussianCount = (int)vksplat.gaussian_count;
	rasterPush.tileSize = VKSPLAT_TILE_SIZE;
	rasterPush.tileCols = (int)tileCols;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vksplat_raster.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vksplat_raster.pipeline_layout,
		0, 1, &vksplat_raster.descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, vksplat_raster.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( rasterPush ), &rasterPush );
	qvkCmdDispatch( cmd, tileCols, tileRows, 1 );

	writes[0].dstSet = vksplat_adam.descriptor;
	writes[0].pBufferInfo = &gaussInfo;
	qvkUpdateDescriptorSets( vk.device, 1, writes, 0, NULL );

	Com_Memset( &adamPush, 0, sizeof( adamPush ) );
	adamPush.gaussianCount = (int)vksplat.gaussian_count;
	adamPush.lr = r_vksplat_lr ? r_vksplat_lr->value : 0.01f;
	adamPush.l1Weight = 0.8f;
	adamPush.beta1 = 0.9f;
	adamPush.beta2 = 0.999f;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vksplat_adam.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vksplat_adam.pipeline_layout,
		0, 1, &vksplat_adam.descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, vksplat_adam.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( adamPush ), &adamPush );
	qvkCmdDispatch( cmd, groups, 1, 1 );

	vk_end_command_buffer( cmd, "VKSplat_RunOneStep" );
	vksplat.train_steps++;
	return qtrue;
}

static void VKSplat_Cmd_Status( void )
{
	ri.Printf( PRINT_ALL,
		"[VkSplat] r_vksplat=%d gaussians=%d train_steps=%d bwdMode=%d ready=%d\n",
		r_vksplat ? r_vksplat->integer : 0,
		vksplat.gaussian_count,
		vksplat.train_steps,
		r_vksplat_bwdMode ? r_vksplat_bwdMode->integer : 0,
		vksplat.ready ? 1 : 0 );
	ri.Printf( PRINT_ALL,
		"[VkSplat] train %ux%u tiles %dx%d (paper 16x16)\n",
		vksplat.train_width, vksplat.train_height,
		VKSPLAT_TILE_SIZE, VKSPLAT_TILE_SIZE );
}

static void VKSplat_Cmd_TrainStep( void )
{
	int steps;
	int i;

	if ( !R_VKSplat_Active() ) {
		ri.Printf( PRINT_WARNING, "[VkSplat] Enable r_vksplat 1 + vid_restart\n" );
		return;
	}

	steps = ( ri.Cmd_Argc() >= 2 ) ? atoi( ri.Cmd_Argv( 1 ) ) : 1;
	if ( steps < 1 ) {
		steps = 1;
	}
	if ( steps > 64 ) {
		steps = 64;
	}

	for ( i = 0; i < steps; i++ ) {
		if ( !VKSplat_RunOneStep() ) {
			ri.Printf( PRINT_WARNING, "[VkSplat] train step failed at %d/%d\n", i + 1, steps );
			return;
		}
	}
	ri.Printf( PRINT_ALL, "[VkSplat] completed %d training step(s), total=%u\n",
		steps, vksplat.train_steps );
}

static void VKSplat_Cmd_Reset( void )
{
	vksplat.train_steps = 0;
	(void)VKSplat_SeedGaussians();
	ri.Printf( PRINT_ALL, "[VkSplat] reset Gaussians and step counter\n" );
}

void R_VKSplat_Init( void )
{
	r_vksplat = ri.Cvar_Get( "r_vksplat", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_vksplat_gaussians = ri.Cvar_Get( "r_vksplat_gaussians", "4096", CVAR_ARCHIVE_ND );
	r_vksplat_lr = ri.Cvar_Get( "r_vksplat_lr", "0.01", CVAR_ARCHIVE_ND );
	r_vksplat_bwdMode = ri.Cvar_Get( "r_vksplat_bwdMode", "0", CVAR_ARCHIVE_ND );
	r_vksplat_debug = ri.Cvar_Get( "r_vksplat_debug", "0", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_vksplat, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_vksplat_gaussians, "64", "8192", CV_INTEGER );
	ri.Cvar_CheckRange( r_vksplat_bwdMode, "0", "2", CV_INTEGER );

	ri.Cvar_SetDescription( r_vksplat,
		"VkSplat 3DGS Vulkan compute training scaffold (Chen et al., Eurographics 2026)." );
	ri.Cvar_SetDescription( r_vksplat_bwdMode,
		"Raster backward mode: 0=Thompson auto, 1=per-Gaussian, 2=shared-memory (paper §4.2)." );

	ri.Cmd_AddCommand( "vksplat_status", VKSplat_Cmd_Status );
	ri.Cmd_AddCommand( "vksplat_train_step", VKSplat_Cmd_TrainStep );
	ri.Cmd_AddCommand( "vksplat_reset", VKSplat_Cmd_Reset );

	if ( r_vksplat->integer ) {
		ri.Printf( PRINT_ALL, "[VkSplat] Enabled — vksplat_train_step, see docs/VKSPLAT.md\n" );
	}
}

void R_VKSplat_Shutdown( void )
{
	ri.Cmd_RemoveCommand( "vksplat_status" );
	ri.Cmd_RemoveCommand( "vksplat_train_step" );
	ri.Cmd_RemoveCommand( "vksplat_reset" );
	VKSplat_DestroyPipeline( &vksplat_project );
	VKSplat_DestroyPipeline( &vksplat_tile );
	VKSplat_DestroyPipeline( &vksplat_raster );
	VKSplat_DestroyPipeline( &vksplat_adam );
	VKSplat_DestroyGpu();
}

qboolean R_VKSplat_Active( void )
{
	return ( r_vksplat && r_vksplat->integer && vk.device != VK_NULL_HANDLE ) ? qtrue : qfalse;
}

qboolean R_VKSplat_RunTrainSteps( int steps )
{
	int i;

	if ( steps < 1 || !R_VKSplat_Active() ) {
		return qfalse;
	}
	for ( i = 0; i < steps; i++ ) {
		if ( !VKSplat_RunOneStep() ) {
			return qfalse;
		}
	}
	return qtrue;
}
