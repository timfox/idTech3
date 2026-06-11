/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Mímir — CUDA/Vulkan interop point-cloud visualization scaffold.
Carter, Hitschfeld & Navarro, arXiv:2504.20937.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_mimir.h"
#include "vk_util.h"
#include "vk_cmd.h"

#ifdef USE_MIMIR_CUDA
#include "mimir/mimir_cuda.h"
#include <unistd.h>
#endif

#define MIMIR_MAX_POINTS        1048576u
#define MIMIR_DEFAULT_POINTS    4096u
#define MIMIR_DEFAULT_W         640u
#define MIMIR_DEFAULT_H         480u
#define MIMIR_POS_STRIDE        12u

typedef struct {
	uint32_t width;
	uint32_t height;
	uint32_t point_count;
	uint32_t frames;
	qboolean ready;
	qboolean compute_section;
} mimirState_t;

static mimirState_t mimir;

static cvar_t *r_mimir;
static cvar_t *r_mimir_points;
static cvar_t *r_mimir_width;
static cvar_t *r_mimir_height;
static cvar_t *r_mimir_sync;
static cvar_t *r_mimir_sigma;
static cvar_t *r_mimir_cuda;
static cvar_t *r_mimir_debug;

typedef struct {
	VkDescriptorSetLayout layout;
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
	VkDescriptorPool pool;
	VkDescriptorSet descriptor;
	qboolean ready;
} mimirPipeline_t;

static mimirPipeline_t mimir_clear;
static mimirPipeline_t mimir_brownian;
static mimirPipeline_t mimir_splat;

#ifdef USE_MIMIR_CUDA
static int mimir_pos_fd = -1;
#endif

static void Mimir_DestroyGpu( void );
static void Mimir_EnsurePipelines( void );
static qboolean Mimir_EnsureBuffers( void );
static qboolean Mimir_SeedPoints( void );

static void Mimir_DestroyPipeline( mimirPipeline_t *p )
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

static qboolean Mimir_CreateComputePipeline( mimirPipeline_t *p, VkShaderModule module,
	const VkDescriptorSetLayoutBinding *bindings, uint32_t bindingCount, uint32_t pushSize )
{
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;
	VkDescriptorPoolCreateInfo pci;
	VkDescriptorSetAllocateInfo ai;
	uint32_t bufCount = 0;
	uint32_t imgCount = 0;
	uint32_t poolCount = 0;
	uint32_t i;
	VkDescriptorPoolSize poolSizes[2];

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

	for ( i = 0; i < bindingCount; i++ ) {
		if ( bindings[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ) {
			bufCount += bindings[i].descriptorCount;
		} else if ( bindings[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ) {
			imgCount += bindings[i].descriptorCount;
		}
	}
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

	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pci.maxSets = 1;
	pci.poolSizeCount = poolCount;
	pci.pPoolSizes = poolSizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &pci, NULL, &p->pool ) );

	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.descriptorPool = p->pool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &p->layout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &ai, &p->descriptor ) );

	p->ready = qtrue;
	return qtrue;
}

static void Mimir_EnsurePipelines( void )
{
	VkDescriptorSetLayoutBinding clearBind;
	VkDescriptorSetLayoutBinding brownianBinds[2];
	VkDescriptorSetLayoutBinding splatBinds[2];

	if ( mimir_clear.ready ) {
		return;
	}

	Com_Memset( &clearBind, 0, sizeof( clearBind ) );
	clearBind.binding = 0;
	clearBind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	clearBind.descriptorCount = 1;
	clearBind.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	(void)Mimir_CreateComputePipeline( &mimir_clear, vk.modules.mimir_clear_cs, &clearBind, 1, 16 );

	Com_Memset( brownianBinds, 0, sizeof( brownianBinds ) );
	brownianBinds[0].binding = 0;
	brownianBinds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	brownianBinds[0].descriptorCount = 1;
	brownianBinds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	brownianBinds[1].binding = 1;
	brownianBinds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	brownianBinds[1].descriptorCount = 1;
	brownianBinds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	(void)Mimir_CreateComputePipeline( &mimir_brownian, vk.modules.mimir_brownian_cs,
		brownianBinds, 2, 16 );

	Com_Memset( splatBinds, 0, sizeof( splatBinds ) );
	splatBinds[0].binding = 0;
	splatBinds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	splatBinds[0].descriptorCount = 1;
	splatBinds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	splatBinds[1].binding = 1;
	splatBinds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	splatBinds[1].descriptorCount = 1;
	splatBinds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	(void)Mimir_CreateComputePipeline( &mimir_splat, vk.modules.mimir_splat_cs, splatBinds, 2, 32 );
}

static qboolean Mimir_FindMemoryType( uint32_t typeBits, VkMemoryPropertyFlags props, uint32_t *outIndex )
{
	VkPhysicalDeviceMemoryProperties memProps;
	uint32_t i;

	qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &memProps );
	for ( i = 0; i < memProps.memoryTypeCount; i++ ) {
		if ( ( typeBits & ( 1u << i ) ) &&
			( memProps.memoryTypes[i].propertyFlags & props ) == props ) {
			*outIndex = i;
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean Mimir_CreateBuffer( VkDeviceSize size, VkBufferUsageFlags usage,
	VkBuffer *outBuf, VkDeviceMemory *outMem, qboolean exportable )
{
	VkBufferCreateInfo bi;
	VkExternalMemoryBufferCreateInfo ext_buf;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;
	VkExportMemoryAllocateInfo export_alloc;
	uint32_t memType;

	if ( !outBuf || !outMem ) {
		return qfalse;
	}

	Com_Memset( &bi, 0, sizeof( bi ) );
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = size;
	bi.usage = usage;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if ( exportable ) {
		Com_Memset( &ext_buf, 0, sizeof( ext_buf ) );
		ext_buf.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
		ext_buf.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
		bi.pNext = &ext_buf;
	}

	VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, outBuf ) );
	qvkGetBufferMemoryRequirements( vk.device, *outBuf, &req );

	if ( !Mimir_FindMemoryType( req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memType ) ) {
		qvkDestroyBuffer( vk.device, *outBuf, NULL );
		*outBuf = VK_NULL_HANDLE;
		return qfalse;
	}

	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = memType;

	if ( exportable ) {
		Com_Memset( &export_alloc, 0, sizeof( export_alloc ) );
		export_alloc.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
		export_alloc.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
		ai.pNext = &export_alloc;
	}

	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, outMem ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, *outBuf, *outMem, 0 ) );
	return qtrue;
}

static qboolean Mimir_EnsureRenderTarget( uint32_t w, uint32_t h )
{
	VkImageCreateInfo image_desc;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_info;
	VkImageViewCreateInfo view_desc;
	uint32_t memType;

	if ( vk.mimir.color_image != VK_NULL_HANDLE &&
		mimir.width == w && mimir.height == h ) {
		return qtrue;
	}

	if ( vk.mimir.color_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.mimir.color_view, NULL );
		qvkDestroyImage( vk.device, vk.mimir.color_image, NULL );
		qvkFreeMemory( vk.device, vk.mimir.color_memory, NULL );
		vk.mimir.color_view = VK_NULL_HANDLE;
		vk.mimir.color_image = VK_NULL_HANDLE;
		vk.mimir.color_memory = VK_NULL_HANDLE;
	}

	Com_Memset( &image_desc, 0, sizeof( image_desc ) );
	image_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_desc.imageType = VK_IMAGE_TYPE_2D;
	image_desc.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_desc.extent.width = w;
	image_desc.extent.height = h;
	image_desc.extent.depth = 1;
	image_desc.mipLevels = 1;
	image_desc.arrayLayers = 1;
	image_desc.samples = VK_SAMPLE_COUNT_1_BIT;
	image_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_desc.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	image_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &image_desc, NULL, &vk.mimir.color_image ) );
	qvkGetImageMemoryRequirements( vk.device, vk.mimir.color_image, &mem_req );

	if ( !Mimir_FindMemoryType( mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memType ) ) {
		return qfalse;
	}

	Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = memType;
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.mimir.color_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.mimir.color_image, vk.mimir.color_memory, 0 ) );

	Com_Memset( &view_desc, 0, sizeof( view_desc ) );
	view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_desc.image = vk.mimir.color_image;
	view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_desc.format = VK_FORMAT_R8G8B8A8_UNORM;
	view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_desc.subresourceRange.levelCount = 1;
	view_desc.subresourceRange.layerCount = 1;
	VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.mimir.color_view ) );

	mimir.width = w;
	mimir.height = h;
	return qtrue;
}

static qboolean Mimir_EnsureBuffers( void )
{
	uint32_t count;
	VkDeviceSize posBytes;
	VkDeviceSize rngBytes;
	qboolean exportPos;

	count = r_mimir_points ? (uint32_t)r_mimir_points->integer : MIMIR_DEFAULT_POINTS;
	if ( count < 64 ) {
		count = 64;
	}
	if ( count > MIMIR_MAX_POINTS ) {
		count = MIMIR_MAX_POINTS;
	}

	if ( vk.mimir.pos_buffer != VK_NULL_HANDLE && mimir.point_count == count ) {
		return qtrue;
	}

	Mimir_DestroyGpu();

	posBytes = (VkDeviceSize)count * MIMIR_POS_STRIDE;
	rngBytes = (VkDeviceSize)count * sizeof( uint32_t );
	exportPos = ( r_mimir_cuda && r_mimir_cuda->integer ) ? qtrue : qfalse;

	if ( !Mimir_CreateBuffer( posBytes,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			&vk.mimir.pos_buffer, &vk.mimir.pos_memory, exportPos ) ) {
		return qfalse;
	}
	if ( !Mimir_CreateBuffer( rngBytes,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			&vk.mimir.rng_buffer, &vk.mimir.rng_memory, qfalse ) ) {
		return qfalse;
	}

#ifdef USE_MIMIR_CUDA
	if ( exportPos && qvkGetMemoryFdKHR && mimir_pos_fd < 0 ) {
		VkMemoryGetFdInfoKHR get_fd;
		Com_Memset( &get_fd, 0, sizeof( get_fd ) );
		get_fd.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
		get_fd.memory = vk.mimir.pos_memory;
		get_fd.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
		if ( qvkGetMemoryFdKHR( vk.device, &get_fd, &mimir_pos_fd ) == VK_SUCCESS && mimir_pos_fd >= 0 ) {
			mimirCudaExport_t exp;
			exp.fd = mimir_pos_fd;
			exp.size = (uint64_t)posBytes;
			exp.valid = qtrue;
			if ( MimirCuda_ImportBuffer( &exp ) ) {
				ri.Printf( PRINT_ALL, "[Mímir] CUDA imported interop position buffer (%u points)\n", count );
			}
		}
	}
#endif

	mimir.point_count = count;
	mimir.ready = qtrue;
	return Mimir_SeedPoints();
}

static qboolean Mimir_SeedPoints( void )
{
	float *hostPos;
	uint32_t *hostRng;
	void *mapped;
	VkDeviceSize posBytes;
	VkDeviceSize rngBytes;
	uint32_t i;

	if ( !mimir.ready ) {
		return qfalse;
	}

	posBytes = (VkDeviceSize)mimir.point_count * MIMIR_POS_STRIDE;
	rngBytes = (VkDeviceSize)mimir.point_count * sizeof( uint32_t );
	hostPos = (float *)ri.Hunk_AllocateTempMemory( (int)posBytes );
	hostRng = (uint32_t *)ri.Hunk_AllocateTempMemory( (int)rngBytes );
	if ( !hostPos || !hostRng ) {
		return qfalse;
	}

	for ( i = 0; i < mimir.point_count; i++ ) {
		float t = (float)i / (float)mimir.point_count;
		hostPos[i * 3 + 0] = sinf( t * 6.283f ) * 0.6f;
		hostPos[i * 3 + 1] = cosf( t * 3.141f ) * 0.6f;
		hostPos[i * 3 + 2] = ( t - 0.5f ) * 0.4f;
		hostRng[i] = 0x12345678u ^ ( i * 747796405u );
	}

	if ( qvkMapMemory( vk.device, vk.mimir.pos_memory, 0, posBytes, 0, &mapped ) != VK_SUCCESS ) {
		return qfalse;
	}
	Com_Memcpy( mapped, hostPos, (size_t)posBytes );
	qvkUnmapMemory( vk.device, vk.mimir.pos_memory );

	if ( qvkMapMemory( vk.device, vk.mimir.rng_memory, 0, rngBytes, 0, &mapped ) != VK_SUCCESS ) {
		return qfalse;
	}
	Com_Memcpy( mapped, hostRng, (size_t)rngBytes );
	qvkUnmapMemory( vk.device, vk.mimir.rng_memory );

	return qtrue;
}

static void Mimir_DestroyGpu( void )
{
#ifdef USE_MIMIR_CUDA
	MimirCuda_ReleaseImport();
	if ( mimir_pos_fd >= 0 ) {
		close( mimir_pos_fd );
		mimir_pos_fd = -1;
	}
#endif

	if ( vk.mimir.pos_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.mimir.pos_buffer, NULL );
		qvkFreeMemory( vk.device, vk.mimir.pos_memory, NULL );
	}
	if ( vk.mimir.rng_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.mimir.rng_buffer, NULL );
		qvkFreeMemory( vk.device, vk.mimir.rng_memory, NULL );
	}
	if ( vk.mimir.color_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.mimir.color_view, NULL );
		qvkDestroyImage( vk.device, vk.mimir.color_image, NULL );
		qvkFreeMemory( vk.device, vk.mimir.color_memory, NULL );
	}

	Com_Memset( &vk.mimir, 0, sizeof( vk.mimir ) );
	mimir.ready = qfalse;
	mimir.point_count = 0;
}

static qboolean Mimir_RunOneStep( void )
{
	VkCommandBuffer cmd;
	VkBufferMemoryBarrier barriers[2];
	VkImageMemoryBarrier imgBarrier;
	VkDescriptorBufferInfo posInfo;
	VkDescriptorBufferInfo rngInfo;
	VkDescriptorImageInfo colorInfo;
	VkWriteDescriptorSet writes[3];
	struct { int viewport[4]; } clearPush;
	struct { uint32_t pointCount; float dt; float sigma; uint32_t frameSeed; } brownPush;
	struct { int viewport[4]; uint32_t pointCount; float pointRadius; float markerColor[4]; } splatPush;
	uint32_t groups;
	qboolean useCuda;

	if ( !mimir.ready || !mimir_brownian.ready || !mimir_splat.ready || !mimir_clear.ready ) {
		return qfalse;
	}

	cmd = vk_begin_command_buffer();
	if ( cmd == VK_NULL_HANDLE ) {
		return qfalse;
	}

	/* prepareViews — optional sync gate (paper §3.4) */
	if ( r_mimir_sync && r_mimir_sync->integer && !mimir.compute_section ) {
		ri.Printf( PRINT_DEVELOPER, "[Mímir] prepareViews blocked — call mimir_prepare first\n" );
		vk_end_command_buffer( cmd, "Mimir_RunOneStep_blocked" );
		return qfalse;
	}

	colorInfo.imageView = vk.mimir.color_view;
	colorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	colorInfo.sampler = VK_NULL_HANDLE;

	Com_Memset( &imgBarrier, 0, sizeof( imgBarrier ) );
	imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imgBarrier.srcAccessMask = 0;
	imgBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imgBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imgBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgBarrier.image = vk.mimir.color_image;
	imgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imgBarrier.subresourceRange.levelCount = 1;
	imgBarrier.subresourceRange.layerCount = 1;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, &imgBarrier );

	clearPush.viewport[0] = (int)mimir.width;
	clearPush.viewport[1] = (int)mimir.height;
	clearPush.viewport[2] = 0;
	clearPush.viewport[3] = 0;

	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = mimir_clear.descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[0].pImageInfo = &colorInfo;
	qvkUpdateDescriptorSets( vk.device, 1, writes, 0, NULL );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mimir_clear.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mimir_clear.pipeline_layout,
		0, 1, &mimir_clear.descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, mimir_clear.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( clearPush ), &clearPush );
	qvkCmdDispatch( cmd, ( mimir.width + 15u ) / 16u, ( mimir.height + 15u ) / 16u, 1 );

	useCuda = qfalse;
#ifdef USE_MIMIR_CUDA
	if ( r_mimir_cuda && r_mimir_cuda->integer && MimirCuda_Available() ) {
		useCuda = MimirCuda_RunBrownian( mimir.point_count, 1.f / 60.f,
			r_mimir_sigma ? r_mimir_sigma->value : 0.35f, mimir.frames );
	}
#endif

	if ( !useCuda ) {
		posInfo.buffer = vk.mimir.pos_buffer;
		posInfo.offset = 0;
		posInfo.range = VK_WHOLE_SIZE;
		rngInfo.buffer = vk.mimir.rng_buffer;
		rngInfo.offset = 0;
		rngInfo.range = VK_WHOLE_SIZE;

		writes[0].dstSet = mimir_brownian.descriptor;
		writes[0].dstBinding = 0;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[0].pBufferInfo = &posInfo;
		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = mimir_brownian.descriptor;
		writes[1].dstBinding = 1;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[1].pBufferInfo = &rngInfo;
		qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

		brownPush.pointCount = mimir.point_count;
		brownPush.dt = 1.f / 60.f;
		brownPush.sigma = r_mimir_sigma ? r_mimir_sigma->value : 0.35f;
		brownPush.frameSeed = mimir.frames ^ 0xA5A5A5A5u;

		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mimir_brownian.pipeline );
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mimir_brownian.pipeline_layout,
			0, 1, &mimir_brownian.descriptor, 0, NULL );
		qvkCmdPushConstants( cmd, mimir_brownian.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
			0, sizeof( brownPush ), &brownPush );
		groups = ( mimir.point_count + 255u ) / 256u;
		qvkCmdDispatch( cmd, groups, 1, 1 );
	}

	posInfo.buffer = vk.mimir.pos_buffer;
	posInfo.offset = 0;
	posInfo.range = VK_WHOLE_SIZE;

	writes[0].dstSet = mimir_splat.descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &posInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = mimir_splat.descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[1].pImageInfo = &colorInfo;
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	splatPush.viewport[0] = (int)mimir.width;
	splatPush.viewport[1] = (int)mimir.height;
	splatPush.viewport[2] = 0;
	splatPush.viewport[3] = 0;
	splatPush.pointCount = mimir.point_count;
	splatPush.pointRadius = 2.5f;
	splatPush.markerColor[0] = 0.35f;
	splatPush.markerColor[1] = 0.75f;
	splatPush.markerColor[2] = 1.0f;
	splatPush.markerColor[3] = 1.0f;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mimir_splat.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mimir_splat.pipeline_layout,
		0, 1, &mimir_splat.descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, mimir_splat.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( splatPush ), &splatPush );
	groups = ( mimir.point_count + 255u ) / 256u;
	qvkCmdDispatch( cmd, groups, 1, 1 );

	Com_Memset( barriers, 0, sizeof( barriers ) );
	barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[0].buffer = vk.mimir.pos_buffer;
	barriers[0].size = VK_WHOLE_SIZE;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 1, barriers, 0, NULL );

	vk_end_command_buffer( cmd, "Mimir_RunOneStep" );
	mimir.frames++;
	mimir.compute_section = qfalse;

	if ( r_mimir_debug && r_mimir_debug->integer ) {
		ri.Printf( PRINT_ALL, "[Mímir] step frame=%u points=%u cuda=%d sync=%d\n",
			mimir.frames, mimir.point_count, useCuda ? 1 : 0,
			r_mimir_sync ? r_mimir_sync->integer : 0 );
	}

	return qtrue;
}

static void Mimir_Cmd_Status( void )
{
	ri.Printf( PRINT_ALL,
		"[Mímir] r_mimir=%d points=%u %ux%u frames=%u ready=%d interop=%d\n",
		r_mimir ? r_mimir->integer : 0,
		mimir.point_count, mimir.width, mimir.height, mimir.frames,
		mimir.ready ? 1 : 0,
#ifdef USE_MIMIR_CUDA
		MimirCuda_Available() ? 1 : 0
#else
		0
#endif
	);
#ifdef USE_MIMIR_CUDA
	ri.Printf( PRINT_ALL, "[Mímir] CUDA backend: %s\n", MimirCuda_BackendName() );
#endif
	ri.Printf( PRINT_ALL, "[Mímir] ViewType::Markers disc splat; Lines/Voxels not wired in v1\n" );
}

static void Mimir_Cmd_Step( void )
{
	int n;
	int i;

	if ( !R_Mimir_Active() ) {
		ri.Printf( PRINT_WARNING, "[Mímir] Enable r_mimir 1 + vid_restart\n" );
		return;
	}
	n = ( ri.Cmd_Argc() >= 2 ) ? atoi( ri.Cmd_Argv( 1 ) ) : 1;
	for ( i = 0; i < n; i++ ) {
		if ( r_mimir_sync && r_mimir_sync->integer ) {
			mimir.compute_section = qtrue;
		}
		if ( !Mimir_RunOneStep() ) {
			ri.Printf( PRINT_WARNING, "[Mímir] step failed at iteration %d\n", i );
			break;
		}
	}
}

static void Mimir_Cmd_Reset( void )
{
	mimir.frames = 0;
	(void)Mimir_SeedPoints();
	ri.Printf( PRINT_ALL, "[Mímir] reset positions\n" );
}

static void Mimir_Cmd_Prepare( void )
{
	mimir.compute_section = qtrue;
	ri.Printf( PRINT_ALL, "[Mímir] prepareViews — compute section open\n" );
}

static void Mimir_Cmd_Update( void )
{
	mimir.compute_section = qfalse;
	ri.Printf( PRINT_ALL, "[Mímir] updateViews — compute section closed\n" );
}

void R_Mimir_Init( void )
{
	r_mimir = ri.Cvar_Get( "r_mimir", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_mimir_points = ri.Cvar_Get( "r_mimir_points", "4096", CVAR_ARCHIVE_ND );
	r_mimir_width = ri.Cvar_Get( "r_mimir_width", "640", CVAR_ARCHIVE_ND );
	r_mimir_height = ri.Cvar_Get( "r_mimir_height", "480", CVAR_ARCHIVE_ND );
	r_mimir_sync = ri.Cvar_Get( "r_mimir_sync", "1", CVAR_ARCHIVE_ND );
	r_mimir_sigma = ri.Cvar_Get( "r_mimir_sigma", "0.35", CVAR_ARCHIVE_ND );
	r_mimir_cuda = ri.Cvar_Get( "r_mimir_cuda", "0", CVAR_ARCHIVE_ND );
	r_mimir_debug = ri.Cvar_Get( "r_mimir_debug", "0", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_mimir, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_mimir_points, "64", "1048576", CV_INTEGER );
	ri.Cvar_CheckRange( r_mimir_width, "64", "4096", CV_INTEGER );
	ri.Cvar_CheckRange( r_mimir_height, "64", "4096", CV_INTEGER );
	ri.Cvar_CheckRange( r_mimir_sync, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_mimir_cuda, "0", "1", CV_INTEGER );

	ri.Cvar_SetDescription( r_mimir,
		"Mímir CUDA/Vulkan interop point-cloud viz (arXiv:2504.20937)." );

#ifdef USE_MIMIR_CUDA
	(void)MimirCuda_Init();
#endif

	ri.Cmd_AddCommand( "mimir_status", Mimir_Cmd_Status );
	ri.Cmd_AddCommand( "mimir_step", Mimir_Cmd_Step );
	ri.Cmd_AddCommand( "mimir_reset", Mimir_Cmd_Reset );
	ri.Cmd_AddCommand( "mimir_prepare", Mimir_Cmd_Prepare );
	ri.Cmd_AddCommand( "mimir_update", Mimir_Cmd_Update );

	if ( r_mimir->integer ) {
		ri.Printf( PRINT_ALL, "[Mímir] Enabled — mimir_step, see docs/MIMIR.md\n" );
	}
}

void R_Mimir_Shutdown( void )
{
	ri.Cmd_RemoveCommand( "mimir_status" );
	ri.Cmd_RemoveCommand( "mimir_step" );
	ri.Cmd_RemoveCommand( "mimir_reset" );
	ri.Cmd_RemoveCommand( "mimir_prepare" );
	ri.Cmd_RemoveCommand( "mimir_update" );
	Mimir_DestroyPipeline( &mimir_clear );
	Mimir_DestroyPipeline( &mimir_brownian );
	Mimir_DestroyPipeline( &mimir_splat );
	Mimir_DestroyGpu();
#ifdef USE_MIMIR_CUDA
	MimirCuda_Shutdown();
#endif
}

qboolean R_Mimir_Active( void )
{
	return ( r_mimir && r_mimir->integer && vk.device != VK_NULL_HANDLE ) ? qtrue : qfalse;
}

qboolean R_Mimir_RunStep( void )
{
	uint32_t w;
	uint32_t h;

	if ( !R_Mimir_Active() ) {
		return qfalse;
	}

	w = r_mimir_width ? (uint32_t)r_mimir_width->integer : MIMIR_DEFAULT_W;
	h = r_mimir_height ? (uint32_t)r_mimir_height->integer : MIMIR_DEFAULT_H;
	if ( !Mimir_EnsureRenderTarget( w, h ) || !Mimir_EnsureBuffers() ) {
		return qfalse;
	}
	Mimir_EnsurePipelines();
	if ( r_mimir_sync && r_mimir_sync->integer ) {
		mimir.compute_section = qtrue;
	}
	return Mimir_RunOneStep();
}

#ifdef USE_MIMIR_CUDA
qboolean R_Mimir_ExportPositions( mimirCudaExport_t *out )
{
	if ( !out || mimir_pos_fd < 0 || !mimir.ready ) {
		return qfalse;
	}
	out->fd = mimir_pos_fd;
	out->size = (uint64_t)mimir.point_count * MIMIR_POS_STRIDE;
	out->valid = qtrue;
	return qtrue;
}
#endif
