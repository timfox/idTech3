/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Experimental path tracing (megakernel vs wavefront) over shared RTX world TLAS.
Requires USE_VULKAN_RTX, r_rtx 1, r_rtxDemo 1, r_pathtrace 1 (latched) + vid_restart.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_pathtrace.h"
#include "vk_rtx.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include "vk_image_layout.h"
#include "vk_cmd.h"

#ifdef USE_VULKAN_RTX

#include "vk_pathtrace_spirv.inc"

typedef struct {
	float invViewProj[16];
	float viewOrigin[4];
	float outputSize[4];
	float traceParams[4];
} VkPtFrameUBO_t;

typedef struct {
	float origin[4];
	float direction[4];
	float throughput[4];
	float radiance[4];
} VkPtSlot_t;

/* r_pathtrace* registered in tr_init.c */

static struct {
	qboolean		ready;
	qboolean		wavefront;
	uint32_t		width;
	uint32_t		height;
	uint32_t		pixel_count;
	uint32_t		handle_size;
	uint32_t		shader_group_base_alignment;
	VkShaderModule		mega_rgen;
	VkShaderModule		wave_rgen;
	VkShaderModule		rmiss;
	VkShaderModule		rchit;
	VkShaderModule		compact_cs;
	VkShaderModule		denoise_cs;
	VkShaderModule		composite_cs;
	VkDescriptorPool	pool;
	VkDescriptorSetLayout	dsl;
	VkDescriptorSetLayout	dsl_post;
	VkPipelineLayout	pl;
	VkPipelineLayout	pl_compact;
	VkPipelineLayout	pl_post;
	VkPipeline		pipeline_mega;
	VkPipeline		pipeline_wave;
	VkPipeline		pipeline_compact;
	VkPipeline		pipeline_denoise;
	VkPipeline		pipeline_composite;
	VkDescriptorSet		descriptor_set;
	VkDescriptorSet		descriptor_post;
	VkBuffer		sbt_buffer;
	VkDeviceMemory		sbt_memory;
	VkBuffer		queue_buffer;
	VkDeviceMemory		queue_memory;
	VkBuffer		counter_buffer;
	VkDeviceMemory		counter_memory;
	void			*counter_ptr;
	VkImage			rt_image;
	VkDeviceMemory		rt_image_memory;
	VkImageView		rt_image_view;
	VkBuffer		pt_ubo;
	VkDeviceMemory		pt_ubo_memory;
	void			*pt_ubo_ptr;
	qboolean		rt_image_traced;
	qboolean		cmds_registered;
} pathtrace;

static void PATHTRACE_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"[VK][PathTrace] active=%d ready=%d arch=%s %ux%u\n"
		"  bounces=%d samples=%d denoise=%d strength=%.2f depthTol=%.3f\n"
		"  composite=%.2f debug=%d rtx=%d fbo=%d demo=%d\n",
		( pathtrace.ready && r_pathtrace && r_pathtrace->integer > 0 ) ? 1 : 0,
		pathtrace.ready ? 1 : 0,
		( r_pathtrace_arch && r_pathtrace_arch->string[0] ) ? r_pathtrace_arch->string : "megakernel",
		pathtrace.width, pathtrace.height,
		r_pathtrace_bounces ? r_pathtrace_bounces->integer : 4,
		r_pathtrace_samples ? r_pathtrace_samples->integer : 1,
		r_pathtrace_denoise ? r_pathtrace_denoise->integer : 0,
		r_pathtrace_denoiseStrength ? r_pathtrace_denoiseStrength->value : 0.65f,
		r_pathtrace_denoiseDepthTol ? r_pathtrace_denoiseDepthTol->value : 0.02f,
		r_pathtrace_composite ? r_pathtrace_composite->value : 1.0f,
		r_pathtrace_debug ? r_pathtrace_debug->integer : 0,
		vk.rtxAvailable ? 1 : 0,
		vk.fboActive ? 1 : 0,
		( r_rtxDemo && r_rtxDemo->integer ) ? 1 : 0 );
}

static void PATHTRACE_RegisterCommands( void )
{
	if ( pathtrace.cmds_registered ) {
		return;
	}
	ri.Cmd_AddCommand( "pathtrace_status", PATHTRACE_Status_f );
	pathtrace.cmds_registered = qtrue;
}

static VkShaderModule vk_pathtrace_shader_module( const uint8_t *code, uint32_t codeSize, const char *name )
{
	VkShaderModuleCreateInfo ci;
	VkShaderModule mod;

	Com_Memset( &ci, 0, sizeof( ci ) );
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = (size_t)codeSize;
	ci.pCode = (const uint32_t *)(uintptr_t)code;
	VK_CHECK( qvkCreateShaderModule( vk.device, &ci, NULL, &mod ) );
	SET_OBJECT_NAME( mod, name, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	return mod;
}

static void vk_pathtrace_alloc_buffer( VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps,
	VkBuffer *outBuf, VkDeviceMemory *outMem )
{
	VkBufferCreateInfo bi;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;
	uint32_t memType;

	Com_Memset( &bi, 0, sizeof( bi ) );
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = size;
	bi.usage = usage;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, outBuf ) );
	qvkGetBufferMemoryRequirements( vk.device, *outBuf, &req );
	memType = vk_find_memory_type( vk.physical_device, req.memoryTypeBits, memProps );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = memType;
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, outMem ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, *outBuf, *outMem, 0 ) );
}

static void vk_pathtrace_destroy_buffer( VkBuffer *buf, VkDeviceMemory *mem )
{
	if ( *buf != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, *buf, NULL );
		*buf = VK_NULL_HANDLE;
	}
	if ( *mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, *mem, NULL );
		*mem = VK_NULL_HANDLE;
	}
}

static void vk_pathtrace_destroy_rt_output( void )
{
	if ( pathtrace.rt_image_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, pathtrace.rt_image_view, NULL );
		pathtrace.rt_image_view = VK_NULL_HANDLE;
	}
	if ( pathtrace.rt_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, pathtrace.rt_image, NULL );
		pathtrace.rt_image = VK_NULL_HANDLE;
	}
	if ( pathtrace.rt_image_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, pathtrace.rt_image_memory, NULL );
		pathtrace.rt_image_memory = VK_NULL_HANDLE;
	}
}

static void vk_pathtrace_create_rt_output( uint32_t w, uint32_t h )
{
	VkImageCreateInfo ici;
	VkMemoryRequirements imgReq;
	VkMemoryAllocateInfo ai;
	VkImageViewCreateInfo ivci;
	VkDescriptorImageInfo imgInfo;
	VkWriteDescriptorSet write;

	vk_pathtrace_destroy_rt_output();

	Com_Memset( &ici, 0, sizeof( ici ) );
	ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.format = vk.color_format;
	ici.extent.width = w;
	ici.extent.height = h;
	ici.extent.depth = 1;
	ici.mipLevels = 1;
	ici.arrayLayers = 1;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &ici, NULL, &pathtrace.rt_image ) );
	qvkGetImageMemoryRequirements( vk.device, pathtrace.rt_image, &imgReq );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = imgReq.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, imgReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &pathtrace.rt_image_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, pathtrace.rt_image, pathtrace.rt_image_memory, 0 ) );

	Com_Memset( &ivci, 0, sizeof( ivci ) );
	ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ivci.image = pathtrace.rt_image;
	ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ivci.format = vk.color_format;
	ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ivci.subresourceRange.levelCount = 1;
	ivci.subresourceRange.layerCount = 1;
	VK_CHECK( qvkCreateImageView( vk.device, &ivci, NULL, &pathtrace.rt_image_view ) );

	Com_Memset( &imgInfo, 0, sizeof( imgInfo ) );
	imgInfo.imageView = pathtrace.rt_image_view;
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = pathtrace.descriptor_set;
	write.dstBinding = 1;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write.pImageInfo = &imgInfo;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );

	pathtrace.width = w;
	pathtrace.height = h;
	pathtrace.pixel_count = w * h;
	pathtrace.rt_image_traced = qfalse;
}

static VkPipeline vk_pathtrace_create_rt_pipeline( VkShaderModule rgen, const char *name )
{
	VkPipelineShaderStageCreateInfo stages[3];
	VkRayTracingShaderGroupCreateInfoKHR groups[3];
	VkRayTracingPipelineCreateInfoKHR rtpci;
	VkPipeline pipeline;
	VkResult res;

	Com_Memset( stages, 0, sizeof( stages ) );
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	stages[0].module = rgen;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[1].module = pathtrace.rmiss;
	stages[1].pName = "main";
	stages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	stages[2].module = pathtrace.rchit;
	stages[2].pName = "main";

	Com_Memset( groups, 0, sizeof( groups ) );
	groups[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groups[0].generalShader = 0;
	groups[1].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groups[1].generalShader = 1;
	groups[2].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	groups[2].closestHitShader = 2;

	Com_Memset( &rtpci, 0, sizeof( rtpci ) );
	rtpci.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	rtpci.stageCount = 3;
	rtpci.pStages = stages;
	rtpci.groupCount = 3;
	rtpci.pGroups = groups;
	rtpci.maxPipelineRayRecursionDepth = 1;
	rtpci.layout = pathtrace.pl;
	res = qvkCreateRayTracingPipelinesKHR( vk.device, VK_NULL_HANDLE, vk.pipelineCache, 1, &rtpci, NULL, &pipeline );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "[VK][PathTrace] pipeline %s failed (%s)\n", name, vk_result_string( res ) );
		return VK_NULL_HANDLE;
	}
	SET_OBJECT_NAME( pipeline, name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
	return pipeline;
}

static void vk_pathtrace_build_sbt( VkPipeline pipeline )
{
	VkDeviceSize sbtSize;
	size_t hbufSize;
	uint8_t *sbtHost;
	int gi;

	vk_pathtrace_destroy_buffer( &pathtrace.sbt_buffer, &pathtrace.sbt_memory );
	hbufSize = (size_t)pathtrace.shader_group_base_alignment * 3u;
	sbtSize = (VkDeviceSize)hbufSize;
	vk_pathtrace_alloc_buffer( sbtSize,
		VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&pathtrace.sbt_buffer, &pathtrace.sbt_memory );
	VK_CHECK( qvkMapMemory( vk.device, pathtrace.sbt_memory, 0, sbtSize, 0, (void **)&sbtHost ) );
	Com_Memset( sbtHost, 0, hbufSize );
	{
		uint8_t packedHandles[96];
		VK_CHECK( qvkGetRayTracingShaderGroupHandlesKHR( vk.device, pipeline, 0, 3,
			pathtrace.handle_size * 3u, packedHandles ) );
		for ( gi = 0; gi < 3; gi++ ) {
			Com_Memcpy( sbtHost + (size_t)pathtrace.shader_group_base_alignment * (size_t)gi,
				packedHandles + (size_t)pathtrace.handle_size * (size_t)gi,
				pathtrace.handle_size );
		}
	}
	qvkUnmapMemory( vk.device, pathtrace.sbt_memory );
}

static qboolean vk_pathtrace_is_wavefront( void )
{
	if ( !r_pathtrace_arch || !r_pathtrace_arch->string[0] ) {
		return qfalse;
	}
	return ( Q_stricmp( r_pathtrace_arch->string, "wavefront" ) == 0 ) ? qtrue : qfalse;
}

typedef struct {
	float extent[4];
	float strength;
	float depthTol;
} vk_pt_denoise_push_t;

typedef struct {
	float extent[4];
	float blend;
} vk_pt_composite_push_t;

static void vk_pathtrace_update_post_descriptors( void )
{
	VkDescriptorImageInfo depthInfo;
	VkDescriptorImageInfo traceInfo;
	VkDescriptorImageInfo colorInfo;
	VkWriteDescriptorSet writes[3];
	Vk_Sampler_Def sd;

	if ( pathtrace.descriptor_post == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.max_lod_1_0 = qtrue;
	sd.noAnisotropy = qtrue;

	Com_Memset( &depthInfo, 0, sizeof( depthInfo ) );
	depthInfo.sampler = vk_find_sampler( &sd );
	depthInfo.imageView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	Com_Memset( &traceInfo, 0, sizeof( traceInfo ) );
	traceInfo.imageView = pathtrace.rt_image_view;
	traceInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( &colorInfo, 0, sizeof( colorInfo ) );
	colorInfo.imageView = vk.color_image_view;
	colorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = pathtrace.descriptor_post;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &depthInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = pathtrace.descriptor_post;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[1].pImageInfo = &traceInfo;
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = pathtrace.descriptor_post;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[2].pImageInfo = &colorInfo;
	qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );
}

static void vk_pathtrace_record_denoise( VkCommandBuffer cmd, VkImageLayout colorRestoreLayout )
{
	vk_pt_denoise_push_t push;
	uint32_t gx, gy;

	if ( !cmd || !r_pathtrace_denoise || !r_pathtrace_denoise->integer ) {
		return;
	}
	if ( pathtrace.pipeline_denoise == VK_NULL_HANDLE || pathtrace.descriptor_post == VK_NULL_HANDLE ) {
		return;
	}

	{
		VkImageMemoryBarrier barrier;
		Com_Memset( &barrier, 0, sizeof( barrier ) );
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = pathtrace.rt_image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier );
	}

	vk_pathtrace_update_post_descriptors();

	push.extent[0] = (float)pathtrace.width;
	push.extent[1] = (float)pathtrace.height;
	push.extent[2] = 0.0f;
	push.extent[3] = 0.0f;
	push.strength = r_pathtrace_denoiseStrength ? r_pathtrace_denoiseStrength->value : 0.65f;
	push.depthTol = r_pathtrace_denoiseDepthTol ? r_pathtrace_denoiseDepthTol->value : 0.02f;

	gx = ( pathtrace.width + 7u ) / 8u;
	gy = ( pathtrace.height + 7u ) / 8u;
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pathtrace.pipeline_denoise );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pathtrace.pl_post, 0, 1,
		&pathtrace.descriptor_post, 0, NULL );
	qvkCmdPushConstants( cmd, pathtrace.pl_post, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
	qvkCmdDispatch( cmd, gx, gy, 1 );
	(void)colorRestoreLayout;
}

static void vk_pathtrace_record_composite( VkCommandBuffer cmd, float blend, VkImageLayout colorRestoreLayout )
{
	VkImageMemoryBarrier barriers[2];
	vk_pt_composite_push_t push;
	uint32_t gx, gy;

	if ( !cmd || blend <= 0.0f || vk.color_image == VK_NULL_HANDLE ) {
		return;
	}
	if ( pathtrace.pipeline_composite == VK_NULL_HANDLE || pathtrace.descriptor_post == VK_NULL_HANDLE ) {
		return;
	}

	vk_pathtrace_update_post_descriptors();

	Com_Memset( barriers, 0, sizeof( barriers ) );
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].image = pathtrace.rt_image;
	barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[0].subresourceRange.levelCount = 1;
	barriers[0].subresourceRange.layerCount = 1;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[1].image = vk.color_image;
	barriers[1].subresourceRange = barriers[0].subresourceRange;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 2, barriers );

	push.extent[0] = (float)vk_get_render_target_width();
	push.extent[1] = (float)vk_get_render_target_height();
	push.extent[2] = 0.0f;
	push.extent[3] = 0.0f;
	push.blend = blend;

	gx = ( push.extent[0] > 0.0f ) ? ( (uint32_t)push.extent[0] + 7u ) / 8u : 1u;
	gy = ( push.extent[1] > 0.0f ) ? ( (uint32_t)push.extent[1] + 7u ) / 8u : 1u;
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pathtrace.pipeline_composite );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pathtrace.pl_post, 0, 1,
		&pathtrace.descriptor_post, 0, NULL );
	qvkCmdPushConstants( cmd, pathtrace.pl_post, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
	qvkCmdDispatch( cmd, gx, gy, 1 );

	barriers[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[1].newLayout = colorRestoreLayout;
	barriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	if ( colorRestoreLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
		barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	} else {
		barriers[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	}
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		( colorRestoreLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL )
			? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0, 0, NULL, 0, NULL, 1, &barriers[1] );
}

void vk_pathtrace_shutdown( void )
{
	if ( pathtrace.pt_ubo_memory != VK_NULL_HANDLE && pathtrace.pt_ubo_ptr ) {
		qvkUnmapMemory( vk.device, pathtrace.pt_ubo_memory );
		pathtrace.pt_ubo_ptr = NULL;
	}
	if ( pathtrace.counter_memory != VK_NULL_HANDLE && pathtrace.counter_ptr ) {
		qvkUnmapMemory( vk.device, pathtrace.counter_memory );
		pathtrace.counter_ptr = NULL;
	}
	if ( pathtrace.pipeline_mega != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, pathtrace.pipeline_mega, NULL );
		pathtrace.pipeline_mega = VK_NULL_HANDLE;
	}
	if ( pathtrace.pipeline_wave != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, pathtrace.pipeline_wave, NULL );
		pathtrace.pipeline_wave = VK_NULL_HANDLE;
	}
	if ( pathtrace.pipeline_compact != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, pathtrace.pipeline_compact, NULL );
		pathtrace.pipeline_compact = VK_NULL_HANDLE;
	}
	if ( pathtrace.pipeline_denoise != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, pathtrace.pipeline_denoise, NULL );
		pathtrace.pipeline_denoise = VK_NULL_HANDLE;
	}
	if ( pathtrace.pipeline_composite != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, pathtrace.pipeline_composite, NULL );
		pathtrace.pipeline_composite = VK_NULL_HANDLE;
	}
	if ( pathtrace.pl_post != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, pathtrace.pl_post, NULL );
		pathtrace.pl_post = VK_NULL_HANDLE;
	}
	if ( pathtrace.pl != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, pathtrace.pl, NULL );
		pathtrace.pl = VK_NULL_HANDLE;
	}
	if ( pathtrace.pl_compact != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, pathtrace.pl_compact, NULL );
		pathtrace.pl_compact = VK_NULL_HANDLE;
	}
	if ( pathtrace.dsl_post != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, pathtrace.dsl_post, NULL );
		pathtrace.dsl_post = VK_NULL_HANDLE;
	}
	if ( pathtrace.dsl != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, pathtrace.dsl, NULL );
		pathtrace.dsl = VK_NULL_HANDLE;
	}
	if ( pathtrace.pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, pathtrace.pool, NULL );
		pathtrace.pool = VK_NULL_HANDLE;
	}
	if ( pathtrace.mega_rgen != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, pathtrace.mega_rgen, NULL );
		pathtrace.mega_rgen = VK_NULL_HANDLE;
	}
	if ( pathtrace.wave_rgen != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, pathtrace.wave_rgen, NULL );
		pathtrace.wave_rgen = VK_NULL_HANDLE;
	}
	if ( pathtrace.rmiss != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, pathtrace.rmiss, NULL );
		pathtrace.rmiss = VK_NULL_HANDLE;
	}
	if ( pathtrace.rchit != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, pathtrace.rchit, NULL );
		pathtrace.rchit = VK_NULL_HANDLE;
	}
	if ( pathtrace.compact_cs != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, pathtrace.compact_cs, NULL );
		pathtrace.compact_cs = VK_NULL_HANDLE;
	}
	if ( pathtrace.denoise_cs != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, pathtrace.denoise_cs, NULL );
		pathtrace.denoise_cs = VK_NULL_HANDLE;
	}
	if ( pathtrace.composite_cs != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, pathtrace.composite_cs, NULL );
		pathtrace.composite_cs = VK_NULL_HANDLE;
	}
	vk_pathtrace_destroy_rt_output();
	vk_pathtrace_destroy_buffer( &pathtrace.sbt_buffer, &pathtrace.sbt_memory );
	vk_pathtrace_destroy_buffer( &pathtrace.queue_buffer, &pathtrace.queue_memory );
	vk_pathtrace_destroy_buffer( &pathtrace.counter_buffer, &pathtrace.counter_memory );
	vk_pathtrace_destroy_buffer( &pathtrace.pt_ubo, &pathtrace.pt_ubo_memory );
	{
		qboolean cmds = pathtrace.cmds_registered;
		Com_Memset( &pathtrace, 0, sizeof( pathtrace ) );
		pathtrace.cmds_registered = cmds;
	}
}

qboolean vk_pathtrace_active( void )
{
	return pathtrace.ready && r_pathtrace && r_pathtrace->integer > 0;
}

void vk_pathtrace_init( void )
{
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps;
	VkPhysicalDeviceProperties2 props2;
	VkDescriptorSetLayoutBinding bindings[8];
	VkDescriptorSetLayoutCreateInfo dslci;
	VkDescriptorPoolSize poolSizes[7];
	VkDescriptorPoolCreateInfo pci;
	VkPipelineLayoutCreateInfo plci;
	VkDescriptorSetAllocateInfo allocInfo;
	VkWriteDescriptorSet writes[4];
	VkDescriptorBufferInfo uboInfo;
	VkDescriptorBufferInfo queueInfo;
	VkDescriptorBufferInfo counterInfo;
	VkDescriptorImageInfo depthInfo;
	VkMemoryRequirements uboReq;
	VkMemoryAllocateInfo uboAi;
	VkBufferCreateInfo uboBi;
	Vk_Sampler_Def sd;
	VkComputePipelineCreateInfo cpci;
	VkPipelineShaderStageCreateInfo cstage;
	uint32_t w, h, uboMemType;
	VkDeviceSize uboAllocSize;
	VkDeviceSize queueBytes;
	const char *archName;

	vk_pathtrace_shutdown();

	PATHTRACE_RegisterCommands();

	if ( !vk.rtxAvailable || !r_pathtrace || r_pathtrace->integer <= 0 ) {
		return;
	}
	if ( !r_rtx || r_rtx->integer <= 0 ) {
		ri.Printf( PRINT_WARNING, "[VK][PathTrace] r_pathtrace 1 requires r_rtx 1 before vid_restart\n" );
		return;
	}
	if ( !vk_rtx_scene_ready() ) {
		ri.Printf( PRINT_WARNING,
			"[VK][PathTrace] RTX scene not ready — set r_rtxDemo 1 and vid_restart (shares world TLAS)\n" );
		return;
	}

	vk_rtx_scene_extent( &w, &h );
	pathtrace.wavefront = vk_pathtrace_is_wavefront();
	archName = pathtrace.wavefront ? "wavefront" : "megakernel";

	Com_Memset( &rtProps, 0, sizeof( rtProps ) );
	rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	Com_Memset( &props2, 0, sizeof( props2 ) );
	props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	props2.pNext = &rtProps;
	if ( qvkGetPhysicalDeviceProperties2 ) {
		qvkGetPhysicalDeviceProperties2( vk.physical_device, &props2 );
	} else {
		rtProps.shaderGroupHandleSize = 32;
		rtProps.shaderGroupBaseAlignment = 64;
	}
	pathtrace.handle_size = rtProps.shaderGroupHandleSize;
	pathtrace.shader_group_base_alignment = rtProps.shaderGroupBaseAlignment;

	pathtrace.mega_rgen = vk_pathtrace_shader_module( vk_pt_mega_rgen_spv, VK_PT_MEGA_RGEN_SPV_SIZE, "pt_mega.rgen" );
	pathtrace.wave_rgen = vk_pathtrace_shader_module( vk_pt_wave_rgen_spv, VK_PT_WAVE_RGEN_SPV_SIZE, "pt_wave.rgen" );
	pathtrace.rmiss = vk_pathtrace_shader_module( vk_pt_miss_rmiss_spv, VK_PT_MISS_RMISS_SPV_SIZE, "pt_miss.rmiss" );
	pathtrace.rchit = vk_pathtrace_shader_module( vk_pt_hit_rchit_spv, VK_PT_HIT_RCHIT_SPV_SIZE, "pt_hit.rchit" );
	pathtrace.compact_cs = vk_pathtrace_shader_module( vk_pt_wave_compact_cs_spv, VK_PT_WAVE_COMPACT_CS_SPV_SIZE, "pt_wave_compact.comp" );
	pathtrace.denoise_cs = vk_pathtrace_shader_module( vk_pt_denoise_cs_spv, VK_PT_DENOISE_CS_SPV_SIZE, "pt_denoise.comp" );
	pathtrace.composite_cs = vk_pathtrace_shader_module( vk_pt_composite_cs_spv, VK_PT_COMPOSITE_CS_SPV_SIZE, "pt_composite.comp" );

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	bindings[4].binding = 4;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	bindings[5].binding = 5;
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[5].descriptorCount = 1;
	bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[6].binding = 6;
	bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[6].descriptorCount = 1;
	bindings[6].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	bindings[7].binding = 7;
	bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[7].descriptorCount = 1;
	bindings[7].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

	Com_Memset( &dslci, 0, sizeof( dslci ) );
	dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslci.bindingCount = 8;
	dslci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &dslci, NULL, &pathtrace.dsl ) );

	poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[1].descriptorCount = 2;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[2].descriptorCount = 1;
	poolSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[3].descriptorCount = 2;
	poolSizes[4].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[4].descriptorCount = 4;
	poolSizes[5].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[5].descriptorCount = 1;
	{
		VkDescriptorSetLayoutBinding postBindings[3];
		VkDescriptorSetLayoutCreateInfo postDslci;

		Com_Memset( postBindings, 0, sizeof( postBindings ) );
		postBindings[0].binding = 0;
		postBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		postBindings[0].descriptorCount = 1;
		postBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		postBindings[1].binding = 1;
		postBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		postBindings[1].descriptorCount = 1;
		postBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		postBindings[2].binding = 2;
		postBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		postBindings[2].descriptorCount = 1;
		postBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		Com_Memset( &postDslci, 0, sizeof( postDslci ) );
		postDslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		postDslci.bindingCount = 3;
		postDslci.pBindings = postBindings;
		VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &postDslci, NULL, &pathtrace.dsl_post ) );
	}

	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pci.maxSets = 2;
	pci.poolSizeCount = 7;
	pci.pPoolSizes = poolSizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &pci, NULL, &pathtrace.pool ) );

	Com_Memset( &plci, 0, sizeof( plci ) );
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &pathtrace.dsl;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &pathtrace.pl ) );
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &pathtrace.pl_compact ) );
	plci.pSetLayouts = &pathtrace.dsl_post;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &pathtrace.pl_post ) );

	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = pathtrace.pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &pathtrace.dsl;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &pathtrace.descriptor_set ) );
	allocInfo.pSetLayouts = &pathtrace.dsl_post;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &pathtrace.descriptor_post ) );

	vk_pathtrace_create_rt_output( w, h );
	vk_rtx_bind_tlas_descriptor( pathtrace.descriptor_set );
	vk_rtx_bind_world_albedo_ssbo( pathtrace.descriptor_set, 6 );
	vk_rtx_bind_world_normal_ssbo( pathtrace.descriptor_set, 7 );

	uboAllocSize = (VkDeviceSize)PAD( (uint32_t)sizeof( VkPtFrameUBO_t ), (uint32_t)vk.uniform_alignment );
	Com_Memset( &uboBi, 0, sizeof( uboBi ) );
	uboBi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	uboBi.size = uboAllocSize;
	uboBi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &uboBi, NULL, &pathtrace.pt_ubo ) );
	qvkGetBufferMemoryRequirements( vk.device, pathtrace.pt_ubo, &uboReq );
	uboMemType = vk_find_memory_type( vk.physical_device, uboReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	Com_Memset( &uboAi, 0, sizeof( uboAi ) );
	uboAi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	uboAi.allocationSize = uboReq.size;
	uboAi.memoryTypeIndex = uboMemType;
	VK_CHECK( qvkAllocateMemory( vk.device, &uboAi, NULL, &pathtrace.pt_ubo_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, pathtrace.pt_ubo, pathtrace.pt_ubo_memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, pathtrace.pt_ubo_memory, 0, uboAllocSize, 0, &pathtrace.pt_ubo_ptr ) );

	queueBytes = (VkDeviceSize)pathtrace.pixel_count * sizeof( VkPtSlot_t );
	vk_pathtrace_alloc_buffer( queueBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &pathtrace.queue_buffer, &pathtrace.queue_memory );
	vk_pathtrace_alloc_buffer( 16u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&pathtrace.counter_buffer, &pathtrace.counter_memory );
	VK_CHECK( qvkMapMemory( vk.device, pathtrace.counter_memory, 0, 16u, 0, &pathtrace.counter_ptr ) );
	Com_Memset( pathtrace.counter_ptr, 0, 16 );

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.max_lod_1_0 = qtrue;
	sd.noAnisotropy = qtrue;
	Com_Memset( &depthInfo, 0, sizeof( depthInfo ) );
	depthInfo.sampler = vk_find_sampler( &sd );
	depthInfo.imageView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	Com_Memset( &uboInfo, 0, sizeof( uboInfo ) );
	uboInfo.buffer = pathtrace.pt_ubo;
	uboInfo.offset = 0;
	uboInfo.range = uboAllocSize;
	Com_Memset( &queueInfo, 0, sizeof( queueInfo ) );
	queueInfo.buffer = pathtrace.queue_buffer;
	queueInfo.offset = 0;
	queueInfo.range = queueBytes;
	Com_Memset( &counterInfo, 0, sizeof( counterInfo ) );
	counterInfo.buffer = pathtrace.counter_buffer;
	counterInfo.offset = 0;
	counterInfo.range = 16;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = pathtrace.descriptor_set;
	writes[0].dstBinding = 2;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writes[0].pBufferInfo = &uboInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = pathtrace.descriptor_set;
	writes[1].dstBinding = 3;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &depthInfo;
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = pathtrace.descriptor_set;
	writes[2].dstBinding = 4;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[2].pBufferInfo = &queueInfo;
	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = pathtrace.descriptor_set;
	writes[3].dstBinding = 5;
	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[3].pBufferInfo = &counterInfo;
	qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );

	pathtrace.pipeline_mega = vk_pathtrace_create_rt_pipeline( pathtrace.mega_rgen, "pt_mega_pipeline" );
	pathtrace.pipeline_wave = vk_pathtrace_create_rt_pipeline( pathtrace.wave_rgen, "pt_wave_pipeline" );
	if ( pathtrace.pipeline_mega == VK_NULL_HANDLE || pathtrace.pipeline_wave == VK_NULL_HANDLE ) {
		vk_pathtrace_shutdown();
		return;
	}

	Com_Memset( &cstage, 0, sizeof( cstage ) );
	cstage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	cstage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cstage.module = pathtrace.compact_cs;
	cstage.pName = "main";
	Com_Memset( &cpci, 0, sizeof( cpci ) );
	cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	cpci.stage = cstage;
	cpci.layout = pathtrace.pl_compact;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &cpci, NULL, &pathtrace.pipeline_compact ) );

	cstage.module = pathtrace.denoise_cs;
	cpci.layout = pathtrace.pl_post;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &cpci, NULL, &pathtrace.pipeline_denoise ) );

	cstage.module = pathtrace.composite_cs;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &cpci, NULL, &pathtrace.pipeline_composite ) );

	vk_pathtrace_update_post_descriptors();

	pathtrace.ready = qtrue;
	ri.Printf( PRINT_ALL,
		"[VK][PathTrace] Ready arch=%s bounces=%d samples=%d denoise=%d strength=%.2f depthTol=%.3f composite blend=%.2f debug=%d (experimental; shares RTX TLAS)\n",
		archName,
		r_pathtrace_bounces ? r_pathtrace_bounces->integer : 4,
		r_pathtrace_samples ? r_pathtrace_samples->integer : 1,
		r_pathtrace_denoise ? r_pathtrace_denoise->integer : 0,
		r_pathtrace_denoiseStrength ? r_pathtrace_denoiseStrength->value : 0.65f,
		r_pathtrace_denoiseDepthTol ? r_pathtrace_denoiseDepthTol->value : 0.02f,
		r_pathtrace_composite ? r_pathtrace_composite->value : 1.0f,
		r_pathtrace_debug ? r_pathtrace_debug->integer : 0 );
}

void vk_pathtrace_frame_begin( void )
{
	uint32_t w, h;

	if ( !pathtrace.ready ) {
		return;
	}
	vk_rtx_scene_extent( &w, &h );
	if ( w == pathtrace.width && h == pathtrace.height ) {
		return;
	}
	vk_pathtrace_create_rt_output( w, h );
	vk_rtx_bind_tlas_descriptor( pathtrace.descriptor_set );
	vk_rtx_bind_world_albedo_ssbo( pathtrace.descriptor_set, 6 );
	vk_rtx_bind_world_normal_ssbo( pathtrace.descriptor_set, 7 );
	vk_pathtrace_update_post_descriptors();
	{
		VkDescriptorBufferInfo queueInfo;
		VkWriteDescriptorSet write;
		VkDeviceSize queueBytes = (VkDeviceSize)( w * h ) * sizeof( VkPtSlot_t );
		vk_pathtrace_destroy_buffer( &pathtrace.queue_buffer, &pathtrace.queue_memory );
		vk_pathtrace_alloc_buffer( queueBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &pathtrace.queue_buffer, &pathtrace.queue_memory );
		Com_Memset( &queueInfo, 0, sizeof( queueInfo ) );
		queueInfo.buffer = pathtrace.queue_buffer;
		queueInfo.range = queueBytes;
		Com_Memset( &write, 0, sizeof( write ) );
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = pathtrace.descriptor_set;
		write.dstBinding = 4;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.pBufferInfo = &queueInfo;
		qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
	}
	ri.Printf( PRINT_ALL, "[VK][PathTrace] Resized trace target to %ux%u\n", w, h );
}

static void vk_pathtrace_trace_dispatch( VkCommandBuffer cmd, VkPipeline pipeline, int waveBounce )
{
	VkBufferDeviceAddressInfo addr;
	VkDeviceAddress sbtBase;
	VkStridedDeviceAddressRegionKHR raygenRegion, missRegion, hitRegion, callableRegion;
	VkPtFrameUBO_t frameUbo;
	float viewProj[16];
	float zNear, zFar;
	int bounces, samples, dbg;

	if ( !pathtrace.pt_ubo_ptr ) {
		return;
	}

	vk_rtx_scene_prepare();
	vk_rtx_bind_tlas_descriptor( pathtrace.descriptor_set );
	vk_rtx_bind_world_albedo_ssbo( pathtrace.descriptor_set, 6 );
	vk_rtx_bind_world_normal_ssbo( pathtrace.descriptor_set, 7 );

	const float *view = backEnd.viewParms.world.modelViewMatrix;
	const float *projection = backEnd.useFirstPersonProjection ?
		backEnd.firstPersonProjectionMatrix : backEnd.viewParms.projectionMatrix;
	float proj_vk[16];

	vk_get_projection_matrix_vk( projection, proj_vk );
	myGlMultMatrix( view, proj_vk, viewProj );
	if ( !vk_mat4_inverse( viewProj, frameUbo.invViewProj ) ) {
		Com_Memcpy( frameUbo.invViewProj, viewProj, sizeof( frameUbo.invViewProj ) );
	}
	frameUbo.viewOrigin[0] = backEnd.viewParms.or.origin[0];
	frameUbo.viewOrigin[1] = backEnd.viewParms.or.origin[1];
	frameUbo.viewOrigin[2] = backEnd.viewParms.or.origin[2];
	frameUbo.viewOrigin[3] = 0.0f;
	zNear = ( r_znear && r_znear->value > 0.0f ) ? r_znear->value : 8.0f;
	zFar = backEnd.viewParms.zFar;
	if ( zFar <= zNear ) {
		zFar = zNear + 100.0f;
	}
	frameUbo.outputSize[0] = (float)pathtrace.width;
	frameUbo.outputSize[1] = (float)pathtrace.height;
	dbg = r_pathtrace_debug ? r_pathtrace_debug->integer : 0;
	if ( dbg < 0 ) {
		dbg = 0;
	}
	if ( dbg > 2 ) {
		dbg = 2;
	}
	frameUbo.outputSize[2] = (float)dbg;
	frameUbo.outputSize[3] = (float)waveBounce;
	bounces = r_pathtrace_bounces ? r_pathtrace_bounces->integer : 4;
	if ( bounces < 1 ) {
		bounces = 1;
	}
	if ( bounces > 8 ) {
		bounces = 8;
	}
	samples = r_pathtrace_samples ? r_pathtrace_samples->integer : 1;
	if ( samples < 1 ) {
		samples = 1;
	}
	if ( samples > 64 ) {
		samples = 64;
	}
	frameUbo.traceParams[0] = (float)bounces;
	frameUbo.traceParams[1] = (float)samples;
	frameUbo.traceParams[2] = (float)( tr.frameCount & 0xFFFF );
	frameUbo.traceParams[3] = r_pathtrace_denoise ? (float)r_pathtrace_denoise->integer : 0.0f;
	Com_Memcpy( pathtrace.pt_ubo_ptr, &frameUbo, sizeof( frameUbo ) );

	vk_pathtrace_build_sbt( pipeline );

	Com_Memset( &addr, 0, sizeof( addr ) );
	addr.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addr.buffer = pathtrace.sbt_buffer;
	sbtBase = qvkGetBufferDeviceAddress( vk.device, &addr );
	raygenRegion.deviceAddress = sbtBase;
	raygenRegion.stride = pathtrace.shader_group_base_alignment;
	raygenRegion.size = pathtrace.shader_group_base_alignment;
	missRegion.deviceAddress = sbtBase + pathtrace.shader_group_base_alignment;
	missRegion.stride = pathtrace.shader_group_base_alignment;
	missRegion.size = pathtrace.shader_group_base_alignment;
	hitRegion.deviceAddress = sbtBase + 2u * pathtrace.shader_group_base_alignment;
	hitRegion.stride = pathtrace.shader_group_base_alignment;
	hitRegion.size = pathtrace.shader_group_base_alignment;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pathtrace.pl, 0, 1,
		&pathtrace.descriptor_set, 0, NULL );
	qvkCmdTraceRaysKHR( cmd, &raygenRegion, &missRegion, &hitRegion, &callableRegion,
		pathtrace.width, pathtrace.height, 1 );
}

void vk_pathtrace_record_pass( VkCommandBuffer cmd )
{
	VkImageMemoryBarrier barriers[2];
	VkImageBlit blit;
	VkImageLayout colorRestoreLayout;
	VkImageAspectFlags depthAspect;
	uint32_t preBarrierCount;
	int maxBounces;
	int b;

	if ( !vk_pathtrace_active() || !cmd || !vk.fboActive ) {
		return;
	}

	if ( vk.renderPassIndex != RENDER_PASS_MAIN && vk.renderPassIndex != RENDER_PASS_POST_BLOOM ) {
		return;
	}

	depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depthAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	if ( vk.depth_image != VK_NULL_HANDLE ) {
		record_depth_image_layout_transition( cmd, depthAspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 0, 0 );
	}

	colorRestoreLayout = ( vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) ?
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	Com_Memset( barriers, 0, sizeof( barriers ) );
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].oldLayout = pathtrace.rt_image_traced ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[0].image = pathtrace.rt_image;
	barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[0].subresourceRange.levelCount = 1;
	barriers[0].subresourceRange.layerCount = 1;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	preBarrierCount = 1;
	barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barriers[1].image = vk.color_image;
	barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[1].subresourceRange.levelCount = 1;
	barriers[1].subresourceRange.layerCount = 1;
	barriers[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	preBarrierCount = 2;

	qvkCmdPipelineBarrier( cmd,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		0, 0, NULL, 0, NULL, preBarrierCount, barriers );

	maxBounces = r_pathtrace_bounces ? r_pathtrace_bounces->integer : 4;
	if ( maxBounces < 1 ) {
		maxBounces = 1;
	}
	if ( maxBounces > 8 ) {
		maxBounces = 8;
	}

	if ( pathtrace.wavefront ) {
		for ( b = 0; b <= maxBounces; b++ ) {
			vk_pathtrace_trace_dispatch( cmd, pathtrace.pipeline_wave, b );
			if ( r_pathtrace_debug && r_pathtrace_debug->integer >= 2 && pathtrace.counter_ptr ) {
				uint32_t pushPc[1];
				uint32_t *ctr = (uint32_t *)pathtrace.counter_ptr;
				ctr[0] = 0;
				pushPc[0] = pathtrace.pixel_count;
				qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pathtrace.pipeline_compact );
				qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pathtrace.pl_compact, 0, 1,
					&pathtrace.descriptor_set, 0, NULL );
				qvkCmdPushConstants( cmd, pathtrace.pl_compact, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( pushPc ), pushPc );
				qvkCmdDispatch( cmd, ( pathtrace.pixel_count + 255u ) / 256u, 1, 1 );
				if ( r_pathtrace_debug->integer >= 2 ) {
					ri.Printf( PRINT_DEVELOPER, "[VK][PathTrace] wave bounce %d alive=%u\n", b, ctr[0] );
				}
			}
		}
	} else {
		vk_pathtrace_trace_dispatch( cmd, pathtrace.pipeline_mega, 0 );
	}

	pathtrace.rt_image_traced = qtrue;

	vk_pathtrace_record_denoise( cmd, colorRestoreLayout );

	{
		float blend = r_pathtrace_composite ? r_pathtrace_composite->value : 1.0f;

		if ( blend < 0.0f ) {
			blend = 0.0f;
		}
		if ( blend > 1.0f ) {
			blend = 1.0f;
		}

		if ( blend >= 0.999f ) {
			barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barriers[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, NULL, 0, NULL, 2, barriers );

			Com_Memset( &blit, 0, sizeof( blit ) );
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.layerCount = 1;
			blit.dstSubresource = blit.srcSubresource;
			blit.srcOffsets[1].x = (int32_t)pathtrace.width;
			blit.srcOffsets[1].y = (int32_t)pathtrace.height;
			blit.srcOffsets[1].z = 1;
			blit.dstOffsets[1].x = (int32_t)vk_get_render_target_width();
			blit.dstOffsets[1].y = (int32_t)vk_get_render_target_height();
			blit.dstOffsets[1].z = 1;
			qvkCmdBlitImage( cmd, pathtrace.rt_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR );

			barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
			barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barriers[1].newLayout = colorRestoreLayout;
			qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				0, 0, NULL, 0, NULL, 2, barriers );
		} else if ( blend > 0.001f ) {
			vk_pathtrace_record_composite( cmd, blend, colorRestoreLayout );
		}
	}

	if ( vk.depth_image != VK_NULL_HANDLE && vk.renderPassIndex == RENDER_PASS_MAIN ) {
		record_depth_image_layout_transition( cmd, depthAspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );
	}
}

#else /* !USE_VULKAN_RTX */

void vk_pathtrace_init( void )
{
	static qboolean s_logged;

	if ( !s_logged ) {
		ri.Printf( PRINT_ALL, "[VK][PathTrace] stub (build with -DUSE_VULKAN_RTX=ON)\n" );
		s_logged = qtrue;
	}
}
void vk_pathtrace_shutdown( void ) {}
void vk_pathtrace_frame_begin( void ) {}
qboolean vk_pathtrace_active( void ) { return qfalse; }
void vk_pathtrace_record_pass( VkCommandBuffer cmd ) { (void)cmd; }

#endif
