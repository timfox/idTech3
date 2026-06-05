/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Raygun (Hirsch & Thoman, arXiv:2001.09792) Vulkan RT demo — primary rays with
optional reflection/refraction/shadow secondary traces and FXAA post. Requires
USE_VULKAN_RTX, r_raygun 1 (latched), r_rtxDemo 1, r_fbo 1, vid_restart.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_raygun.h"
#include "vk_rtx.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include "vk_image_layout.h"
#include "vk_hybrid1.h"

cvar_t	*r_raygun;
cvar_t	*r_raygun_fxaa;
cvar_t	*r_raygun_reflection;
cvar_t	*r_raygun_refraction;
cvar_t	*r_raygun_shadow;
cvar_t	*r_raygun_ior;
cvar_t	*r_raygun_composite;
cvar_t	*r_raygun_samples;

#ifdef USE_VULKAN_RTX

#include "vk_raygun_spirv.inc"

typedef struct {
	float invViewProj[16];
	float viewOrigin[4];
	float zNearFar[4];
	float outputSize[4];
	float sunDirection[4];
	float traceParams[4];
} VkRaygunFrameUBO_t;

typedef struct {
	float outputSize[4];
} VkRaygunFxaaUBO_t;

static struct {
	qboolean            ready;
	uint32_t            width;
	uint32_t            height;
	uint32_t            handle_size;
	uint32_t            shader_group_base_alignment;
	VkShaderModule      rgen;
	VkShaderModule      rmiss;
	VkShaderModule      rchit;
	VkShaderModule      shadow_rchit;
	VkShaderModule      fxaa_cs;
	VkDescriptorPool    rt_pool;
	VkDescriptorSetLayout rt_dsl;
	VkPipelineLayout    rt_pl;
	VkPipeline          rt_pipeline;
	VkDescriptorSet     rt_set;
	VkBuffer            sbt_buffer;
	VkDeviceMemory      sbt_memory;
	VkBuffer            ubo;
	VkDeviceMemory      ubo_memory;
	void                *ubo_ptr;
	VkDescriptorPool    fxaa_pool;
	VkDescriptorSetLayout fxaa_dsl;
	VkPipelineLayout    fxaa_pl;
	VkPipeline          fxaa_pipeline;
	VkDescriptorSet     fxaa_set;
	VkBuffer            fxaa_ubo;
	VkDeviceMemory      fxaa_ubo_memory;
	void                *fxaa_ubo_ptr;
	VkImage             rt_image;
	VkDeviceMemory      rt_image_memory;
	VkImageView         rt_image_view;
	VkImage             fxaa_image;
	VkDeviceMemory      fxaa_image_memory;
	VkImageView         fxaa_image_view;
	qboolean            rt_image_traced;
	qboolean            cmds_registered;
} raygun;

static void RAYGUN_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"[VK][Raygun] active=%d ready=%d rtx=%d fbo=%d demo=%d %ux%u\n"
		"  fxaa=%d reflection=%d refraction=%d shadow=%d ior=%.2f composite=%.2f samples=%d\n",
		vk_raygun_active() ? 1 : 0,
		raygun.ready ? 1 : 0,
		vk.rtxAvailable ? 1 : 0,
		vk.fboActive ? 1 : 0,
		( r_rtxDemo && r_rtxDemo->integer ) ? 1 : 0,
		raygun.width, raygun.height,
		( r_raygun_fxaa && r_raygun_fxaa->integer ) ? 1 : 0,
		( r_raygun_reflection && r_raygun_reflection->integer ) ? 1 : 0,
		( r_raygun_refraction && r_raygun_refraction->integer ) ? 1 : 0,
		( r_raygun_shadow && r_raygun_shadow->integer ) ? 1 : 0,
		r_raygun_ior ? r_raygun_ior->value : 1.45f,
		r_raygun_composite ? r_raygun_composite->value : 1.0f,
		r_raygun_samples ? r_raygun_samples->integer : 1 );
}

static void RAYGUN_RegisterCommands( void )
{
	if ( raygun.cmds_registered ) {
		return;
	}
	ri.Cmd_AddCommand( "raygun_status", RAYGUN_Status_f );
	raygun.cmds_registered = qtrue;
}

static void RAYGUN_UnregisterCommands( void )
{
	if ( !raygun.cmds_registered ) {
		return;
	}
	ri.Cmd_RemoveCommand( "raygun_status" );
	raygun.cmds_registered = qfalse;
}

static VkShaderModule RAYGUN_ShaderModule( const uint8_t *code, uint32_t codeSize, const char *name )
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

static void RAYGUN_DestroyBuffer( VkBuffer *buf, VkDeviceMemory *mem )
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

static void RAYGUN_DestroyRtOutput( void )
{
	if ( raygun.rt_image_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, raygun.rt_image_view, NULL );
		raygun.rt_image_view = VK_NULL_HANDLE;
	}
	if ( raygun.rt_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, raygun.rt_image, NULL );
		raygun.rt_image = VK_NULL_HANDLE;
	}
	if ( raygun.rt_image_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, raygun.rt_image_memory, NULL );
		raygun.rt_image_memory = VK_NULL_HANDLE;
	}
}

static void RAYGUN_DestroyFxaaOutput( void )
{
	if ( raygun.fxaa_image_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, raygun.fxaa_image_view, NULL );
		raygun.fxaa_image_view = VK_NULL_HANDLE;
	}
	if ( raygun.fxaa_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, raygun.fxaa_image, NULL );
		raygun.fxaa_image = VK_NULL_HANDLE;
	}
	if ( raygun.fxaa_image_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, raygun.fxaa_image_memory, NULL );
		raygun.fxaa_image_memory = VK_NULL_HANDLE;
	}
}

static void RAYGUN_CreateStorageImage( uint32_t w, uint32_t h, VkImage *outImage, VkDeviceMemory *outMem,
	VkImageView *outView, const char *label )
{
	VkImageCreateInfo ici;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;
	VkImageViewCreateInfo ivci;

	if ( *outView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, *outView, NULL );
		*outView = VK_NULL_HANDLE;
	}
	if ( *outImage != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, *outImage, NULL );
		*outImage = VK_NULL_HANDLE;
	}
	if ( *outMem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, *outMem, NULL );
		*outMem = VK_NULL_HANDLE;
	}

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
	ici.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &ici, NULL, outImage ) );
	qvkGetImageMemoryRequirements( vk.device, *outImage, &req );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, outMem ) );
	VK_CHECK( qvkBindImageMemory( vk.device, *outImage, *outMem, 0 ) );

	Com_Memset( &ivci, 0, sizeof( ivci ) );
	ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ivci.image = *outImage;
	ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ivci.format = vk.color_format;
	ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ivci.subresourceRange.levelCount = 1;
	ivci.subresourceRange.layerCount = 1;
	VK_CHECK( qvkCreateImageView( vk.device, &ivci, NULL, outView ) );
	SET_OBJECT_NAME( *outImage, label, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
}

static void RAYGUN_CreateOutputs( uint32_t w, uint32_t h )
{
	RAYGUN_DestroyRtOutput();
	RAYGUN_DestroyFxaaOutput();
	RAYGUN_CreateStorageImage( w, h, &raygun.rt_image, &raygun.rt_image_memory, &raygun.rt_image_view, "raygun_rt" );
	RAYGUN_CreateStorageImage( w, h, &raygun.fxaa_image, &raygun.fxaa_image_memory, &raygun.fxaa_image_view, "raygun_fxaa" );
	raygun.width = w;
	raygun.height = h;
	raygun.rt_image_traced = qfalse;
}

static VkSampler RAYGUN_NearestSampler( void )
{
	Vk_Sampler_Def sd;

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.max_lod_1_0 = qtrue;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static void RAYGUN_WriteUboBinding( VkDescriptorSet set, uint32_t binding, VkBuffer buf, VkDeviceSize range )
{
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet write;

	Com_Memset( &info, 0, sizeof( info ) );
	info.buffer = buf;
	info.offset = 0;
	info.range = range;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = set;
	write.dstBinding = binding;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write.pBufferInfo = &info;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
}

static void RAYGUN_UpdateRtDescriptors( void )
{
	VkDescriptorImageInfo outInfo;
	VkDescriptorImageInfo depthInfo;
	VkWriteDescriptorSet writes[2];
	VkImageView depth_view;

	if ( raygun.rt_set == VK_NULL_HANDLE ) {
		return;
	}

	depth_view = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;

	Com_Memset( &outInfo, 0, sizeof( outInfo ) );
	outInfo.imageView = raygun.rt_image_view;
	outInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( &depthInfo, 0, sizeof( depthInfo ) );
	depthInfo.sampler = RAYGUN_NearestSampler();
	depthInfo.imageView = depth_view;
	depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	vk_rtx_bind_tlas_descriptor( raygun.rt_set );

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = raygun.rt_set;
	writes[0].dstBinding = 1;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[0].pImageInfo = &outInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = raygun.rt_set;
	writes[1].dstBinding = 3;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &depthInfo;
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	RAYGUN_WriteUboBinding( raygun.rt_set, 2, raygun.ubo, sizeof( VkRaygunFrameUBO_t ) );
}

static void RAYGUN_UpdateFxaaDescriptors( void )
{
	VkDescriptorImageInfo srcInfo;
	VkDescriptorImageInfo dstInfo;
	VkWriteDescriptorSet writes[2];

	if ( raygun.fxaa_set == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &srcInfo, 0, sizeof( srcInfo ) );
	srcInfo.imageView = raygun.rt_image_view;
	srcInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( &dstInfo, 0, sizeof( dstInfo ) );
	dstInfo.imageView = raygun.fxaa_image_view;
	dstInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = raygun.fxaa_set;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[0].pImageInfo = &srcInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = raygun.fxaa_set;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[1].pImageInfo = &dstInfo;
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	RAYGUN_WriteUboBinding( raygun.fxaa_set, 2, raygun.fxaa_ubo, sizeof( VkRaygunFxaaUBO_t ) );
}

static VkPipeline RAYGUN_CreateRtPipeline( void )
{
	VkPipelineShaderStageCreateInfo stages[4];
	VkRayTracingShaderGroupCreateInfoKHR groups[4];
	VkRayTracingPipelineCreateInfoKHR rtpci;
	VkPipeline pipeline;
	VkResult res;

	Com_Memset( stages, 0, sizeof( stages ) );
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	stages[0].module = raygun.rgen;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[1].module = raygun.rmiss;
	stages[1].pName = "main";
	stages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	stages[2].module = raygun.rchit;
	stages[2].pName = "main";
	stages[3].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[3].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	stages[3].module = raygun.shadow_rchit;
	stages[3].pName = "main";

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
	groups[3].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[3].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	groups[3].closestHitShader = 3;

	Com_Memset( &rtpci, 0, sizeof( rtpci ) );
	rtpci.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	rtpci.stageCount = 4;
	rtpci.pStages = stages;
	rtpci.groupCount = 4;
	rtpci.pGroups = groups;
	rtpci.maxPipelineRayRecursionDepth = 4;
	rtpci.layout = raygun.rt_pl;
	res = qvkCreateRayTracingPipelinesKHR( vk.device, VK_NULL_HANDLE, vk.pipelineCache, 1, &rtpci, NULL, &pipeline );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "[VK][Raygun] CreateRayTracingPipelinesKHR failed (%s)\n", vk_result_string( res ) );
		return VK_NULL_HANDLE;
	}
	SET_OBJECT_NAME( pipeline, "raygun_rt", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
	return pipeline;
}

static void RAYGUN_BuildSbt( void )
{
	VkDeviceSize sbtSize;
	size_t hbufSize;
	uint8_t *host;
	uint8_t packed[128];
	uint32_t gi;

	RAYGUN_DestroyBuffer( &raygun.sbt_buffer, &raygun.sbt_memory );

	hbufSize = (size_t)raygun.shader_group_base_alignment * 4u;
	sbtSize = (VkDeviceSize)hbufSize;

	{
		VkBufferCreateInfo bi;
		VkMemoryRequirements req;
		VkMemoryAllocateInfo ai;
		VkMemoryAllocateFlagsInfo flagsInfo;

		Com_Memset( &bi, 0, sizeof( bi ) );
		bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bi.size = sbtSize;
		bi.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, &raygun.sbt_buffer ) );
		qvkGetBufferMemoryRequirements( vk.device, raygun.sbt_buffer, &req );
		Com_Memset( &flagsInfo, 0, sizeof( flagsInfo ) );
		flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
		flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
		Com_Memset( &ai, 0, sizeof( ai ) );
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.pNext = &flagsInfo;
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &raygun.sbt_memory ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, raygun.sbt_buffer, raygun.sbt_memory, 0 ) );
	}

	VK_CHECK( qvkMapMemory( vk.device, raygun.sbt_memory, 0, sbtSize, 0, (void **)&host ) );
	Com_Memset( host, 0, hbufSize );
	VK_CHECK( qvkGetRayTracingShaderGroupHandlesKHR( vk.device, raygun.rt_pipeline, 0, 4,
		raygun.handle_size * 4u, packed ) );
	for ( gi = 0; gi < 4; gi++ ) {
		Com_Memcpy( host + (size_t)raygun.shader_group_base_alignment * (size_t)gi,
			packed + (size_t)raygun.handle_size * (size_t)gi, raygun.handle_size );
	}
	qvkUnmapMemory( vk.device, raygun.sbt_memory );
}

static void RAYGUN_UpdateFrameUbo( void )
{
	VkRaygunFrameUBO_t frameUbo;
	float viewProj[16];
	float zNear, zFar;
	float sunLen;
	int samples;

	if ( !raygun.ubo_ptr ) {
		return;
	}

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
	frameUbo.zNearFar[0] = zNear;
	frameUbo.zNearFar[1] = zFar;
	frameUbo.zNearFar[2] = 0.0f;
	frameUbo.zNearFar[3] = 0.0f;

	frameUbo.outputSize[0] = (float)raygun.width;
	frameUbo.outputSize[1] = (float)raygun.height;
	frameUbo.outputSize[2] = ( r_raygun_fxaa && r_raygun_fxaa->integer ) ? 1.0f : 0.0f;
	frameUbo.outputSize[3] = r_raygun_composite ? r_raygun_composite->value : 1.0f;
	if ( frameUbo.outputSize[3] < 0.0f ) {
		frameUbo.outputSize[3] = 0.0f;
	}
	if ( frameUbo.outputSize[3] > 1.0f ) {
		frameUbo.outputSize[3] = 1.0f;
	}

	frameUbo.sunDirection[0] = 0.35f;
	frameUbo.sunDirection[1] = 0.75f;
	frameUbo.sunDirection[2] = 0.55f;
	sunLen = sqrtf( frameUbo.sunDirection[0] * frameUbo.sunDirection[0] +
		frameUbo.sunDirection[1] * frameUbo.sunDirection[1] +
		frameUbo.sunDirection[2] * frameUbo.sunDirection[2] );
	if ( sunLen > 1e-6f ) {
		frameUbo.sunDirection[0] /= sunLen;
		frameUbo.sunDirection[1] /= sunLen;
		frameUbo.sunDirection[2] /= sunLen;
	}
	frameUbo.sunDirection[3] = ( r_raygun_reflection && r_raygun_reflection->integer ) ? 1.0f : 0.0f;

	samples = r_raygun_samples ? r_raygun_samples->integer : 1;
	if ( samples < 1 ) {
		samples = 1;
	}
	if ( samples > 4 ) {
		samples = 4;
	}
	frameUbo.traceParams[0] = (float)samples;
	frameUbo.traceParams[1] = ( r_raygun_refraction && r_raygun_refraction->integer ) ? 1.0f : 0.0f;
	frameUbo.traceParams[2] = r_raygun_shadow ? r_raygun_shadow->value : 1.0f;
	if ( frameUbo.traceParams[2] < 0.0f ) {
		frameUbo.traceParams[2] = 0.0f;
	}
	if ( frameUbo.traceParams[2] > 1.0f ) {
		frameUbo.traceParams[2] = 1.0f;
	}
	frameUbo.traceParams[3] = r_raygun_ior ? r_raygun_ior->value : 1.45f;
	if ( frameUbo.traceParams[3] < 1.01f ) {
		frameUbo.traceParams[3] = 1.01f;
	}

	Com_Memcpy( raygun.ubo_ptr, &frameUbo, sizeof( frameUbo ) );
}

static void RAYGUN_UpdateFxaaUbo( void )
{
	VkRaygunFxaaUBO_t fxaaUbo;

	if ( !raygun.fxaa_ubo_ptr ) {
		return;
	}
	fxaaUbo.outputSize[0] = (float)raygun.width;
	fxaaUbo.outputSize[1] = (float)raygun.height;
	fxaaUbo.outputSize[2] = 0.0f;
	fxaaUbo.outputSize[3] = 0.0f;
	Com_Memcpy( raygun.fxaa_ubo_ptr, &fxaaUbo, sizeof( fxaaUbo ) );
}

static void RAYGUN_TraceDispatch( VkCommandBuffer cmd )
{
	VkBufferDeviceAddressInfo addrInfo;
	VkDeviceAddress sbtBase;
	VkStridedDeviceAddressRegionKHR raygenRegion;
	VkStridedDeviceAddressRegionKHR missRegion;
	VkStridedDeviceAddressRegionKHR hitRegion;
	VkStridedDeviceAddressRegionKHR callableRegion;

	Com_Memset( &addrInfo, 0, sizeof( addrInfo ) );
	addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addrInfo.buffer = raygun.sbt_buffer;
	sbtBase = qvkGetBufferDeviceAddress( vk.device, &addrInfo );

	Com_Memset( &raygenRegion, 0, sizeof( raygenRegion ) );
	Com_Memset( &missRegion, 0, sizeof( missRegion ) );
	Com_Memset( &hitRegion, 0, sizeof( hitRegion ) );
	Com_Memset( &callableRegion, 0, sizeof( callableRegion ) );

	raygenRegion.deviceAddress = sbtBase;
	raygenRegion.stride = raygun.shader_group_base_alignment;
	raygenRegion.size = raygun.shader_group_base_alignment;
	missRegion.deviceAddress = sbtBase + raygun.shader_group_base_alignment;
	missRegion.stride = raygun.shader_group_base_alignment;
	missRegion.size = raygun.shader_group_base_alignment;
	hitRegion.deviceAddress = sbtBase + 2u * raygun.shader_group_base_alignment;
	hitRegion.stride = raygun.shader_group_base_alignment;
	hitRegion.size = raygun.shader_group_base_alignment * 2u;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, raygun.rt_pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, raygun.rt_pl, 0, 1, &raygun.rt_set, 0, NULL );
	qvkCmdTraceRaysKHR( cmd, &raygenRegion, &missRegion, &hitRegion, &callableRegion, raygun.width, raygun.height, 1 );
}

static void RAYGUN_RecordFxaa( VkCommandBuffer cmd )
{
	VkImageMemoryBarrier barrier;
	uint32_t gx, gy;

	if ( !cmd || !r_raygun_fxaa || !r_raygun_fxaa->integer ) {
		return;
	}
	if ( raygun.fxaa_pipeline == VK_NULL_HANDLE || raygun.fxaa_set == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = raygun.rt_image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );

	barrier.image = raygun.fxaa_image;
	barrier.oldLayout = raygun.rt_image_traced ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcAccessMask = raygun.rt_image_traced ? VK_ACCESS_SHADER_WRITE_BIT : 0;
	barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	qvkCmdPipelineBarrier( cmd,
		raygun.rt_image_traced ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier );

	RAYGUN_UpdateFxaaDescriptors();
	RAYGUN_UpdateFxaaUbo();

	gx = ( raygun.width + 7u ) / 8u;
	gy = ( raygun.height + 7u ) / 8u;
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, raygun.fxaa_pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, raygun.fxaa_pl, 0, 1, &raygun.fxaa_set, 0, NULL );
	qvkCmdDispatch( cmd, gx, gy, 1 );
}

static void RAYGUN_BlitToColor( VkCommandBuffer cmd, VkImage srcImage, VkImageLayout srcLayout,
	VkImageLayout colorOldLayout, VkImageLayout colorRestoreLayout )
{
	VkImageMemoryBarrier barriers[2];
	VkImageBlit blit;

	Com_Memset( barriers, 0, sizeof( barriers ) );
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].image = srcImage;
	barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[0].subresourceRange.levelCount = 1;
	barriers[0].subresourceRange.layerCount = 1;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].oldLayout = srcLayout;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[1].image = vk.color_image;
	barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[1].subresourceRange.levelCount = 1;
	barriers[1].subresourceRange.layerCount = 1;
	barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].oldLayout = colorOldLayout;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	qvkCmdPipelineBarrier( cmd,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 2, barriers );

	Com_Memset( &blit, 0, sizeof( blit ) );
	blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.srcSubresource.mipLevel = 0;
	blit.srcSubresource.baseArrayLayer = 0;
	blit.srcSubresource.layerCount = 1;
	blit.dstSubresource = blit.srcSubresource;
	blit.srcOffsets[0].x = 0;
	blit.srcOffsets[0].y = 0;
	blit.srcOffsets[0].z = 0;
	blit.srcOffsets[1].x = (int32_t)raygun.width;
	blit.srcOffsets[1].y = (int32_t)raygun.height;
	blit.srcOffsets[1].z = 1;
	blit.dstOffsets[0] = blit.srcOffsets[0];
	blit.dstOffsets[1] = blit.srcOffsets[1];
	qvkCmdBlitImage( cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR );

	barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

	barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].newLayout = colorRestoreLayout;
	barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	if ( colorRestoreLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
		barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	} else {
		barriers[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	}

	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
		( colorRestoreLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL )
			? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0, 0, NULL, 0, NULL, 2, barriers );
}

void R_Raygun_Init( void )
{
	r_raygun = ri.Cvar_Get( "r_raygun", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_raygun, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_raygun,
		"Raygun RT demo (Hirsch & Thoman arXiv:2001.09792): primary rays with reflection/refraction/shadow. "
		"Requires USE_VULKAN_RTX, r_rtxDemo 1, r_fbo 1, vid_restart." );
	ri.Cvar_SetGroup( r_raygun, CVG_RENDERER );

	r_raygun_fxaa = ri.Cvar_Get( "r_raygun_fxaa", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_raygun_fxaa, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_raygun_fxaa, "Raygun: edge-aware FXAA compute pass after trace." );
	ri.Cvar_SetGroup( r_raygun_fxaa, CVG_RENDERER );

	r_raygun_reflection = ri.Cvar_Get( "r_raygun_reflection", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_raygun_reflection, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_raygun_reflection, "Raygun: trace mirror reflections on metallic material (mat 2)." );
	ri.Cvar_SetGroup( r_raygun_reflection, CVG_RENDERER );

	r_raygun_refraction = ri.Cvar_Get( "r_raygun_refraction", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_raygun_refraction, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_raygun_refraction, "Raygun: trace refraction on glass material (mat 3)." );
	ri.Cvar_SetGroup( r_raygun_refraction, CVG_RENDERER );

	r_raygun_shadow = ri.Cvar_Get( "r_raygun_shadow", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_raygun_shadow, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_raygun_shadow, "Raygun: sun shadow trace strength (0=off, 1=full)." );
	ri.Cvar_SetGroup( r_raygun_shadow, CVG_RENDERER );

	r_raygun_ior = ri.Cvar_Get( "r_raygun_ior", "1.45", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_raygun_ior, "1.01", "2.5", CV_FLOAT );
	ri.Cvar_SetDescription( r_raygun_ior, "Raygun: index of refraction for glass material." );
	ri.Cvar_SetGroup( r_raygun_ior, CVG_RENDERER );

	r_raygun_composite = ri.Cvar_Get( "r_raygun_composite", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_raygun_composite, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_raygun_composite, "Raygun: composite blend weight passed to trace UBO." );
	ri.Cvar_SetGroup( r_raygun_composite, CVG_RENDERER );

	r_raygun_samples = ri.Cvar_Get( "r_raygun_samples", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_raygun_samples, "1", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_raygun_samples, "Raygun: primary rays per pixel (1-4)." );
	ri.Cvar_SetGroup( r_raygun_samples, CVG_RENDERER );

	if ( r_raygun && r_raygun->integer ) {
		ri.Printf( PRINT_ALL,
			"[VK][Raygun] r_raygun=1 (latched; build USE_VULKAN_RTX, set r_rtxDemo 1 + r_fbo 1, vid_restart)\n" );
	}

	RAYGUN_RegisterCommands();
}

void R_Raygun_Shutdown( void )
{
	RAYGUN_UnregisterCommands();
}

void vk_raygun_shutdown( void )
{
	if ( raygun.fxaa_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, raygun.fxaa_pipeline, NULL );
	}
	if ( raygun.rt_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, raygun.rt_pipeline, NULL );
	}
	if ( raygun.fxaa_pl != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, raygun.fxaa_pl, NULL );
	}
	if ( raygun.rt_pl != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, raygun.rt_pl, NULL );
	}
	if ( raygun.fxaa_dsl != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, raygun.fxaa_dsl, NULL );
	}
	if ( raygun.rt_dsl != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, raygun.rt_dsl, NULL );
	}
	if ( raygun.fxaa_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, raygun.fxaa_pool, NULL );
	}
	if ( raygun.rt_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, raygun.rt_pool, NULL );
	}
	if ( raygun.rgen != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, raygun.rgen, NULL );
	}
	if ( raygun.rmiss != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, raygun.rmiss, NULL );
	}
	if ( raygun.rchit != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, raygun.rchit, NULL );
	}
	if ( raygun.shadow_rchit != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, raygun.shadow_rchit, NULL );
	}
	if ( raygun.fxaa_cs != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, raygun.fxaa_cs, NULL );
	}
	RAYGUN_DestroyBuffer( &raygun.sbt_buffer, &raygun.sbt_memory );
	RAYGUN_DestroyBuffer( &raygun.ubo, &raygun.ubo_memory );
	raygun.ubo_ptr = NULL;
	RAYGUN_DestroyBuffer( &raygun.fxaa_ubo, &raygun.fxaa_ubo_memory );
	raygun.fxaa_ubo_ptr = NULL;
	RAYGUN_DestroyRtOutput();
	RAYGUN_DestroyFxaaOutput();
	Com_Memset( &raygun, 0, sizeof( raygun ) );
}

void vk_raygun_init( void )
{
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps;
	VkPhysicalDeviceProperties2 props2;
	VkDescriptorSetLayoutBinding rtBindings[4];
	VkDescriptorSetLayoutBinding fxaaBindings[3];
	VkDescriptorSetLayoutCreateInfo dslci;
	VkPipelineLayoutCreateInfo plci;
	VkDescriptorPoolSize poolSizes[4];
	VkDescriptorPoolCreateInfo dpci;
	VkDescriptorSetAllocateInfo allocInfo;
	VkComputePipelineCreateInfo cpci;
	VkPipelineShaderStageCreateInfo csStage;
	VkBufferCreateInfo bi;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;
	VkDeviceSize uboSize;
	uint32_t w, h;

	vk_raygun_shutdown();

	if ( !vk.rtxAvailable || !r_raygun || r_raygun->integer <= 0 ) {
		return;
	}
	if ( !r_rtxDemo || !r_rtxDemo->integer ) {
		ri.Printf( PRINT_WARNING, "[VK][Raygun] requires r_rtxDemo 1 (latched) for world TLAS\n" );
		return;
	}
	if ( !vk_rtx_scene_ready() ) {
		ri.Printf( PRINT_WARNING,
			"[VK][Raygun] RTX scene not ready — set r_rtxDemo 1 and vid_restart (shares world TLAS)\n" );
		return;
	}

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
	raygun.handle_size = rtProps.shaderGroupHandleSize;
	raygun.shader_group_base_alignment = rtProps.shaderGroupBaseAlignment;

	raygun.rgen = RAYGUN_ShaderModule( vk_raygun_rgen_spv, VK_RAYGUN_RGEN_SPV_SIZE, "raygun.rgen" );
	raygun.rmiss = RAYGUN_ShaderModule( vk_raygun_rmiss_spv, VK_RAYGUN_RMISS_SPV_SIZE, "raygun.rmiss" );
	raygun.rchit = RAYGUN_ShaderModule( vk_raygun_rchit_spv, VK_RAYGUN_RCHIT_SPV_SIZE, "raygun.rchit" );
	raygun.shadow_rchit = RAYGUN_ShaderModule( vk_raygun_shadow_rchit_spv, VK_RAYGUN_SHADOW_RCHIT_SPV_SIZE, "raygun_shadow.rchit" );
	raygun.fxaa_cs = RAYGUN_ShaderModule( vk_raygun_fxaa_cs_spv, VK_RAYGUN_FXAA_CS_SPV_SIZE, "raygun_fxaa.comp" );

	vk_rtx_scene_extent( &w, &h );
	RAYGUN_CreateOutputs( w, h );

	uboSize = (VkDeviceSize)PAD( (uint32_t)sizeof( VkRaygunFrameUBO_t ), (uint32_t)vk.uniform_alignment );
	Com_Memset( &bi, 0, sizeof( bi ) );
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = uboSize;
	bi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, &raygun.ubo ) );
	qvkGetBufferMemoryRequirements( vk.device, raygun.ubo, &req );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &raygun.ubo_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, raygun.ubo, raygun.ubo_memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, raygun.ubo_memory, 0, uboSize, 0, &raygun.ubo_ptr ) );

	uboSize = (VkDeviceSize)PAD( (uint32_t)sizeof( VkRaygunFxaaUBO_t ), (uint32_t)vk.uniform_alignment );
	Com_Memset( &bi, 0, sizeof( bi ) );
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = uboSize;
	bi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, &raygun.fxaa_ubo ) );
	qvkGetBufferMemoryRequirements( vk.device, raygun.fxaa_ubo, &req );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &raygun.fxaa_ubo_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, raygun.fxaa_ubo, raygun.fxaa_ubo_memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, raygun.fxaa_ubo_memory, 0, uboSize, 0, &raygun.fxaa_ubo_ptr ) );

	Com_Memset( rtBindings, 0, sizeof( rtBindings ) );
	rtBindings[0].binding = 0;
	rtBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	rtBindings[0].descriptorCount = 1;
	rtBindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	rtBindings[1].binding = 1;
	rtBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	rtBindings[1].descriptorCount = 1;
	rtBindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	rtBindings[2].binding = 2;
	rtBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	rtBindings[2].descriptorCount = 1;
	rtBindings[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
	rtBindings[3].binding = 3;
	rtBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	rtBindings[3].descriptorCount = 1;
	rtBindings[3].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	Com_Memset( &dslci, 0, sizeof( dslci ) );
	dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslci.bindingCount = 4;
	dslci.pBindings = rtBindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &dslci, NULL, &raygun.rt_dsl ) );

	Com_Memset( poolSizes, 0, sizeof( poolSizes ) );
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[1].descriptorCount = 1;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[2].descriptorCount = 1;
	poolSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[3].descriptorCount = 1;
	Com_Memset( &dpci, 0, sizeof( dpci ) );
	dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpci.maxSets = 1;
	dpci.poolSizeCount = 4;
	dpci.pPoolSizes = poolSizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &dpci, NULL, &raygun.rt_pool ) );

	Com_Memset( &plci, 0, sizeof( plci ) );
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &raygun.rt_dsl;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &raygun.rt_pl ) );

	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = raygun.rt_pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &raygun.rt_dsl;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &raygun.rt_set ) );
	RAYGUN_UpdateRtDescriptors();

	raygun.rt_pipeline = RAYGUN_CreateRtPipeline();
	if ( raygun.rt_pipeline == VK_NULL_HANDLE ) {
		vk_raygun_shutdown();
		return;
	}
	RAYGUN_BuildSbt();

	Com_Memset( fxaaBindings, 0, sizeof( fxaaBindings ) );
	fxaaBindings[0].binding = 0;
	fxaaBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	fxaaBindings[0].descriptorCount = 1;
	fxaaBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	fxaaBindings[1].binding = 1;
	fxaaBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	fxaaBindings[1].descriptorCount = 1;
	fxaaBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	fxaaBindings[2].binding = 2;
	fxaaBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	fxaaBindings[2].descriptorCount = 1;
	fxaaBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	dslci.bindingCount = 3;
	dslci.pBindings = fxaaBindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &dslci, NULL, &raygun.fxaa_dsl ) );

	poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[0].descriptorCount = 2;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[1].descriptorCount = 1;
	dpci.maxSets = 1;
	dpci.poolSizeCount = 2;
	dpci.pPoolSizes = poolSizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &dpci, NULL, &raygun.fxaa_pool ) );

	plci.pSetLayouts = &raygun.fxaa_dsl;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &raygun.fxaa_pl ) );

	allocInfo.descriptorPool = raygun.fxaa_pool;
	allocInfo.pSetLayouts = &raygun.fxaa_dsl;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &raygun.fxaa_set ) );
	RAYGUN_UpdateFxaaDescriptors();

	Com_Memset( &csStage, 0, sizeof( csStage ) );
	csStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	csStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	csStage.module = raygun.fxaa_cs;
	csStage.pName = "main";
	Com_Memset( &cpci, 0, sizeof( cpci ) );
	cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	cpci.stage = csStage;
	cpci.layout = raygun.fxaa_pl;
	VK_CHECK( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &cpci, NULL, &raygun.fxaa_pipeline ) );
	SET_OBJECT_NAME( raygun.fxaa_pipeline, "raygun_fxaa", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	raygun.ready = qtrue;
	ri.Printf( PRINT_ALL,
		"[VK][Raygun] Hirsch/Thoman RT demo ready %ux%u (fxaa=%d reflection=%d refraction=%d shadow=%.2f samples=%d)\n",
		raygun.width, raygun.height,
		( r_raygun_fxaa && r_raygun_fxaa->integer ) ? 1 : 0,
		( r_raygun_reflection && r_raygun_reflection->integer ) ? 1 : 0,
		( r_raygun_refraction && r_raygun_refraction->integer ) ? 1 : 0,
		r_raygun_shadow ? r_raygun_shadow->value : 1.0f,
		r_raygun_samples ? r_raygun_samples->integer : 1 );
}

qboolean vk_raygun_active( void )
{
	if ( !r_raygun || r_raygun->integer <= 0 ) {
		return qfalse;
	}
	if ( !r_rtxDemo || !r_rtxDemo->integer ) {
		return qfalse;
	}
	if ( !vk.fboActive || !vk.rtxAvailable ) {
		return qfalse;
	}
	if ( vk_hybrid1_active() ) {
		return qfalse;
	}
	return ( raygun.ready && vk_rtx_scene_ready() ) ? qtrue : qfalse;
}

void vk_raygun_frame_begin( void )
{
	uint32_t w, h;

	if ( !raygun.ready ) {
		if ( r_raygun && r_raygun->integer > 0 && vk.rtxAvailable && vk_rtx_scene_ready() ) {
			vk_raygun_init();
		}
		return;
	}

	vk_rtx_scene_extent( &w, &h );
	if ( w == raygun.width && h == raygun.height ) {
		return;
	}

	RAYGUN_CreateOutputs( w, h );
	RAYGUN_UpdateRtDescriptors();
	RAYGUN_UpdateFxaaDescriptors();
	ri.Printf( PRINT_ALL, "[VK][Raygun] Resized RT/FXAA targets to %ux%u\n", w, h );
}

void vk_raygun_record_pass( VkCommandBuffer cmd )
{
	VkImageMemoryBarrier barriers[2];
	VkImageLayout colorOldLayout;
	VkImageLayout colorRestoreLayout;
	VkImageAspectFlags depthAspect;
	VkImage blitSource;
	qboolean useFxaa;
	uint32_t preBarrierCount;

	if ( !vk_raygun_active() || !cmd ) {
		return;
	}
	if ( vk.renderPassIndex != RENDER_PASS_MAIN && vk.renderPassIndex != RENDER_PASS_POST_BLOOM ) {
		return;
	}

	vk_rtx_scene_prepare();
	vk_rtx_bind_tlas_descriptor( raygun.rt_set );
	RAYGUN_UpdateRtDescriptors();
	RAYGUN_UpdateFrameUbo();

	depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depthAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	if ( vk.depth_image != VK_NULL_HANDLE && vk.renderPassIndex == RENDER_PASS_MAIN ) {
		record_depth_image_layout_transition( cmd, depthAspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 0, 0 );
	}

	colorOldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	colorRestoreLayout = ( vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) ?
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	Com_Memset( barriers, 0, sizeof( barriers ) );
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].oldLayout = raygun.rt_image_traced ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].image = raygun.rt_image;
	barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[0].subresourceRange.levelCount = 1;
	barriers[0].subresourceRange.layerCount = 1;
	barriers[0].srcAccessMask = raygun.rt_image_traced ? VK_ACCESS_SHADER_WRITE_BIT : 0;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

	preBarrierCount = 1;
	if ( vk.color_image != VK_NULL_HANDLE ) {
		barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[1].image = vk.color_image;
		barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barriers[1].subresourceRange.levelCount = 1;
		barriers[1].subresourceRange.layerCount = 1;
		barriers[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		preBarrierCount = 2;
	}

	qvkCmdPipelineBarrier( cmd,
		( raygun.rt_image_traced ? VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT ) |
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		0, 0, NULL, 0, NULL, preBarrierCount, barriers );

	RAYGUN_TraceDispatch( cmd );
	raygun.rt_image_traced = qtrue;

	useFxaa = ( r_raygun_fxaa && r_raygun_fxaa->integer ) ? qtrue : qfalse;
	if ( useFxaa ) {
		RAYGUN_RecordFxaa( cmd );
		blitSource = raygun.fxaa_image;
	} else {
		blitSource = raygun.rt_image;
	}

	RAYGUN_BlitToColor( cmd, blitSource, VK_IMAGE_LAYOUT_GENERAL, colorOldLayout, colorRestoreLayout );

	if ( vk.depth_image != VK_NULL_HANDLE && vk.renderPassIndex == RENDER_PASS_MAIN ) {
		record_depth_image_layout_transition( cmd, depthAspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );
	}
}

#else /* !USE_VULKAN_RTX */

void R_Raygun_Init( void )
{
	r_raygun = ri.Cvar_Get( "r_raygun", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_raygun, "Raygun RT demo (requires USE_VULKAN_RTX build)." );
}

void R_Raygun_Shutdown( void ) {}

void vk_raygun_init( void ) {}
void vk_raygun_shutdown( void ) {}
void vk_raygun_frame_begin( void ) {}
qboolean vk_raygun_active( void ) { return qfalse; }
void vk_raygun_record_pass( VkCommandBuffer cmd ) { (void)cmd; }

#endif
