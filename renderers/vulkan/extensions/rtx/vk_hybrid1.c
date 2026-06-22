/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Hybrid Rendering 1 — Granja/Pereira thesis pipeline (July 2021):
separate 1-SPP shadow + indirect specular RT, SVGF temporal + variance color
clamping, adaptive separable A-trous, Reinhard pre-denoise on specular, composite
with raster direct lighting. Requires USE_VULKAN_RTX, r_hybrid1 or r_rtx 1, r_rtxDemo 1, r_fbo 1.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_hybrid1.h"
#include "vk_rtx.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include "vk_image_layout.h"
#include "vk_cmd.h"
#include "vk_deferred_gbuffer.h"
#include "vk_temporal.h"
#include "vk_skybox_hdr.h"
#include "vk_post_fog.h"

#ifdef USE_VULKAN_RTX

#include "vk_hybrid1_spirv.inc"

typedef struct {
	float invViewProj[16];
	float prevViewProj[16];
	float viewProj[16];
	float viewOrigin[4];
	float sunDirection[4];
	float outputSize[4];
	float params0[4];
	float params1[4];
} VkHybrid1FrameUBO_t;

typedef struct {
	VkImage     image;
	VkImageView view;
	VkDeviceMemory memory;
} hybrid1_image_t;

static struct {
	qboolean            ready;
	uint32_t            width;
	uint32_t            height;
	uint32_t            frame_index;
	uint32_t            handle_size;
	uint32_t            shader_group_base_alignment;
	VkShaderModule      shadow_rgen;
	VkShaderModule      shadow_rmiss;
	VkShaderModule      shadow_rchit;
	VkShaderModule      spec_rgen;
	VkShaderModule      spec_rmiss;
	VkShaderModule      spec_rchit;
	VkShaderModule      diffuse_rgen;
	VkShaderModule      diffuse_rmiss;
	VkShaderModule      diffuse_rchit;
	VkShaderModule      temporal_cs;
	VkShaderModule      atrous_cs;
	VkShaderModule      composite_cs;
	VkDescriptorPool    rt_pool;
	VkDescriptorSetLayout rt_dsl;
	VkPipelineLayout    rt_pl;
	VkPipeline          shadow_pipeline;
	VkPipeline          spec_pipeline;
	VkPipeline          diffuse_pipeline;
	VkDescriptorSet     shadow_set;
	VkDescriptorSet     spec_set;
	VkDescriptorSet     diffuse_set;
	VkBuffer            sbt_shadow_buffer;
	VkDeviceMemory      sbt_shadow_memory;
	VkBuffer            sbt_spec_buffer;
	VkDeviceMemory      sbt_spec_memory;
	VkBuffer            sbt_diffuse_buffer;
	VkDeviceMemory      sbt_diffuse_memory;
	VkBuffer            ubo;
	VkDeviceMemory      ubo_memory;
	void                *ubo_ptr;
	VkDescriptorSetLayout temporal_dsl;
	VkPipelineLayout    temporal_pl;
	VkPipeline          temporal_pipeline;
	VkDescriptorPool    temporal_pool;
	VkDescriptorSet     temporal_shadow_set;
	VkDescriptorSet     temporal_spec_set;
	VkDescriptorSetLayout atrous_dsl;
	VkPipelineLayout    atrous_pl;
	VkPipeline          atrous_pipeline;
	VkDescriptorPool    atrous_pool;
	VkDescriptorSet     atrous_shadow_set;
	VkDescriptorSet     atrous_spec_set;
	VkDescriptorSet     atrous_diffuse_set;
	VkDescriptorSetLayout composite_dsl;
	VkPipelineLayout    composite_pl;
	VkPipeline          composite_pipeline;
	VkDescriptorPool    composite_pool;
	VkDescriptorSet     composite_set;
	hybrid1_image_t     raw_shadow;
	hybrid1_image_t     raw_spec;
	hybrid1_image_t     raw_diffuse;
	hybrid1_image_t     filtered_shadow;
	hybrid1_image_t     filtered_spec;
	hybrid1_image_t     filtered_diffuse;
	hybrid1_image_t     atrous_shadow;
	hybrid1_image_t     atrous_spec;
	hybrid1_image_t     atrous_diffuse;
	hybrid1_image_t     hist_shadow[2];
	hybrid1_image_t     hist_spec[2];
	hybrid1_image_t     var_shadow[2];
	hybrid1_image_t     var_spec[2];
	qboolean            traced;
	qboolean            cmds_registered;
} hybrid1;

static void HYBRID1_ResetHistory( void )
{
	hybrid1.frame_index = 0;
	hybrid1.traced = qfalse;
}

static void HYBRID1_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"[VK][Hybrid1] active=%d ready=%d rtx=%d fbo=%d demo=%d %ux%u frame=%u\n"
		"  channels shadow=%d spec=%d diffuse=%d ibl=%d motion=%d taa=%d debug=%d\n"
		"  denoise clamp=%d gamma=%.2f alpha=%.2f atrous=%d separable=%d adaptive=%d reinhard=%d\n"
		"  composite shadowStr=%.2f specStr=%.2f diffuseStr=%.2f deferredGBuffer=%d\n",
		vk_hybrid1_active() ? 1 : 0,
		hybrid1.ready ? 1 : 0,
		vk.rtxAvailable ? 1 : 0,
		vk.fboActive ? 1 : 0,
		( r_rtxDemo && r_rtxDemo->integer ) ? 1 : 0,
		hybrid1.width, hybrid1.height,
		hybrid1.frame_index,
		( r_hybrid1_shadow && r_hybrid1_shadow->integer ) ? 1 : 0,
		( r_hybrid1_spec && r_hybrid1_spec->integer ) ? 1 : 0,
		( r_hybrid1_diffuse && r_hybrid1_diffuse->integer ) ? 1 : 0,
		( r_hybrid1_ibl && r_hybrid1_ibl->integer ) ? 1 : 0,
		( r_hybrid1_motion && r_hybrid1_motion->integer ) ? 1 : 0,
		( r_hybrid1_taa && r_hybrid1_taa->integer ) ? 1 : 0,
		r_hybrid1_debug ? r_hybrid1_debug->integer : 0,
		r_hybrid1_historyClamp ? r_hybrid1_historyClamp->integer : 1,
		r_hybrid1_historyGamma ? r_hybrid1_historyGamma->value : 1.25f,
		r_hybrid1_temporalAlpha ? r_hybrid1_temporalAlpha->value : 0.1f,
		r_hybrid1_atrousIters ? r_hybrid1_atrousIters->integer : 4,
		r_hybrid1_separableBlur ? r_hybrid1_separableBlur->integer : 1,
		r_hybrid1_adaptiveBlur ? r_hybrid1_adaptiveBlur->integer : 1,
		r_hybrid1_reinhard ? r_hybrid1_reinhard->integer : 1,
		r_hybrid1_shadowStrength ? r_hybrid1_shadowStrength->value : 0.85f,
		r_hybrid1_specStrength ? r_hybrid1_specStrength->value : 1.0f,
		r_hybrid1_diffuseStrength ? r_hybrid1_diffuseStrength->value : 1.0f,
		vk_deferred_gbuffer_fill_wanted() ? 1 : 0 );
}

static void HYBRID1_Reset_f( void )
{
	HYBRID1_ResetHistory();
	ri.Printf( PRINT_ALL, "[VK][Hybrid1] temporal history reset\n" );
}

static void HYBRID1_Reload_f( void )
{
	if ( !r_hybrid1 || r_hybrid1->integer <= 0 ) {
		ri.Printf( PRINT_ALL, "[VK][Hybrid1] reload ignored (r_hybrid1 0)\n" );
		return;
	}
	if ( !vk.rtxAvailable ) {
		ri.Printf( PRINT_WARNING, "[VK][Hybrid1] reload failed: RTX not available on this device/build\n" );
		return;
	}
	vk_hybrid1_init();
	ri.Printf( PRINT_ALL, "[VK][Hybrid1] pipeline reloaded\n" );
}

static qboolean HYBRID1_ConsumeCvarResets( void )
{
	qboolean reset = qfalse;
	cvar_t *watch[] = {
		r_hybrid1_shadow,
		r_hybrid1_spec,
		r_hybrid1_diffuse,
		r_hybrid1_historyClamp,
		r_hybrid1_historyGamma,
		r_hybrid1_temporalAlpha,
		r_hybrid1_adaptiveBlur,
		r_hybrid1_separableBlur,
		r_hybrid1_reinhard,
		r_hybrid1_atrousIters,
		r_hybrid1_ibl,
		r_hybrid1_motion,
		NULL
	};
	int i;

	for ( i = 0; watch[i]; i++ ) {
		if ( watch[i]->modified ) {
			watch[i]->modified = qfalse;
			reset = qtrue;
		}
	}
	if ( reset ) {
		HYBRID1_ResetHistory();
		ri.Printf( PRINT_DEVELOPER, "[VK][Hybrid1] denoise cvar change: temporal history reset\n" );
	}
	return reset;
}

static void HYBRID1_RegisterCommands( void )
{
	if ( hybrid1.cmds_registered ) {
		return;
	}
	ri.Cmd_AddCommand( "hybrid1_status", HYBRID1_Status_f );
	ri.Cmd_AddCommand( "hybrid1_reset", HYBRID1_Reset_f );
	ri.Cmd_AddCommand( "hybrid1_reload", HYBRID1_Reload_f );
	hybrid1.cmds_registered = qtrue;
}

static void HYBRID1_UnregisterCommands( void )
{
	if ( !hybrid1.cmds_registered ) {
		return;
	}
	ri.Cmd_RemoveCommand( "hybrid1_status" );
	ri.Cmd_RemoveCommand( "hybrid1_reset" );
	ri.Cmd_RemoveCommand( "hybrid1_reload" );
	hybrid1.cmds_registered = qfalse;
}

static VkShaderModule HYBRID1_ShaderModule( const uint8_t *code, uint32_t codeSize, const char *name )
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

static void HYBRID1_DestroyImage( hybrid1_image_t *img )
{
	if ( !img ) {
		return;
	}
	if ( img->view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, img->view, NULL );
		img->view = VK_NULL_HANDLE;
	}
	if ( img->image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, img->image, NULL );
		img->image = VK_NULL_HANDLE;
	}
	if ( img->memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, img->memory, NULL );
		img->memory = VK_NULL_HANDLE;
	}
}

static void HYBRID1_CreateImage( hybrid1_image_t *img, uint32_t w, uint32_t h, const char *label )
{
	VkImageCreateInfo ici;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;
	VkImageViewCreateInfo ivci;

	HYBRID1_DestroyImage( img );

	Com_Memset( &ici, 0, sizeof( ici ) );
	ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	ici.extent.width = w;
	ici.extent.height = h;
	ici.extent.depth = 1;
	ici.mipLevels = 1;
	ici.arrayLayers = 1;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &ici, NULL, &img->image ) );
	qvkGetImageMemoryRequirements( vk.device, img->image, &req );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &img->memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, img->image, img->memory, 0 ) );

	Com_Memset( &ivci, 0, sizeof( ivci ) );
	ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ivci.image = img->image;
	ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ivci.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ivci.subresourceRange.levelCount = 1;
	ivci.subresourceRange.layerCount = 1;
	VK_CHECK( qvkCreateImageView( vk.device, &ivci, NULL, &img->view ) );
	if ( label ) {
		SET_OBJECT_NAME( img->image, label, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	}
}

static void HYBRID1_DestroyAllImages( void )
{
	HYBRID1_DestroyImage( &hybrid1.raw_shadow );
	HYBRID1_DestroyImage( &hybrid1.raw_spec );
	HYBRID1_DestroyImage( &hybrid1.raw_diffuse );
	HYBRID1_DestroyImage( &hybrid1.filtered_shadow );
	HYBRID1_DestroyImage( &hybrid1.filtered_spec );
	HYBRID1_DestroyImage( &hybrid1.filtered_diffuse );
	HYBRID1_DestroyImage( &hybrid1.atrous_shadow );
	HYBRID1_DestroyImage( &hybrid1.atrous_spec );
	HYBRID1_DestroyImage( &hybrid1.atrous_diffuse );
	HYBRID1_DestroyImage( &hybrid1.hist_shadow[0] );
	HYBRID1_DestroyImage( &hybrid1.hist_shadow[1] );
	HYBRID1_DestroyImage( &hybrid1.hist_spec[0] );
	HYBRID1_DestroyImage( &hybrid1.hist_spec[1] );
	HYBRID1_DestroyImage( &hybrid1.var_shadow[0] );
	HYBRID1_DestroyImage( &hybrid1.var_shadow[1] );
	HYBRID1_DestroyImage( &hybrid1.var_spec[0] );
	HYBRID1_DestroyImage( &hybrid1.var_spec[1] );
}

static void HYBRID1_CreateImages( uint32_t w, uint32_t h )
{
	HYBRID1_CreateImage( &hybrid1.raw_shadow, w, h, "hybrid1 raw shadow" );
	HYBRID1_CreateImage( &hybrid1.raw_spec, w, h, "hybrid1 raw spec" );
	HYBRID1_CreateImage( &hybrid1.raw_diffuse, w, h, "hybrid1 raw diffuse" );
	HYBRID1_CreateImage( &hybrid1.filtered_shadow, w, h, "hybrid1 filtered shadow" );
	HYBRID1_CreateImage( &hybrid1.filtered_spec, w, h, "hybrid1 filtered spec" );
	HYBRID1_CreateImage( &hybrid1.filtered_diffuse, w, h, "hybrid1 filtered diffuse" );
	HYBRID1_CreateImage( &hybrid1.atrous_shadow, w, h, "hybrid1 atrous shadow" );
	HYBRID1_CreateImage( &hybrid1.atrous_spec, w, h, "hybrid1 atrous spec" );
	HYBRID1_CreateImage( &hybrid1.atrous_diffuse, w, h, "hybrid1 atrous diffuse" );
	HYBRID1_CreateImage( &hybrid1.hist_shadow[0], w, h, "hybrid1 hist shadow 0" );
	HYBRID1_CreateImage( &hybrid1.hist_shadow[1], w, h, "hybrid1 hist shadow 1" );
	HYBRID1_CreateImage( &hybrid1.hist_spec[0], w, h, "hybrid1 hist spec 0" );
	HYBRID1_CreateImage( &hybrid1.hist_spec[1], w, h, "hybrid1 hist spec 1" );
	HYBRID1_CreateImage( &hybrid1.var_shadow[0], w, h, "hybrid1 var shadow 0" );
	HYBRID1_CreateImage( &hybrid1.var_shadow[1], w, h, "hybrid1 var shadow 1" );
	HYBRID1_CreateImage( &hybrid1.var_spec[0], w, h, "hybrid1 var spec 0" );
	HYBRID1_CreateImage( &hybrid1.var_spec[1], w, h, "hybrid1 var spec 1" );
	hybrid1.width = w;
	hybrid1.height = h;
	hybrid1.traced = qfalse;
}

static VkSampler HYBRID1_NearestSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static VkSampler HYBRID1_LinearSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	return vk_find_sampler( &sd );
}

static void HYBRID1_GetCubemapViews( VkImageView *prefilterOut, VkImageView *irradianceOut )
{
	if ( !SkyboxHDR_GetCubemapViews( prefilterOut, irradianceOut ) ) {
		if ( prefilterOut ) {
			*prefilterOut = VK_NULL_HANDLE;
		}
		if ( irradianceOut ) {
			*irradianceOut = VK_NULL_HANDLE;
		}
	}
}

static void HYBRID1_WriteImageBinding( VkDescriptorSet set, uint32_t binding, VkImageView view,
	VkDescriptorType type, VkImageLayout layout )
{
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet write;

	Com_Memset( &info, 0, sizeof( info ) );
	info.imageView = view;
	info.imageLayout = layout;
	info.sampler = ( type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ) ? HYBRID1_LinearSampler() : VK_NULL_HANDLE;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = set;
	write.dstBinding = binding;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = &info;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
}

static void HYBRID1_WriteUboBinding( VkDescriptorSet set, uint32_t binding )
{
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet write;

	Com_Memset( &info, 0, sizeof( info ) );
	info.buffer = hybrid1.ubo;
	info.offset = 0;
	info.range = sizeof( VkHybrid1FrameUBO_t );

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = set;
	write.dstBinding = binding;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write.pBufferInfo = &info;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
}

static void HYBRID1_UpdateRtDescriptors( VkDescriptorSet set, VkImageView outputView )
{
	VkDescriptorImageInfo depthInfo;
	VkDescriptorImageInfo normalInfo;
	VkDescriptorImageInfo materialInfo;
	VkDescriptorImageInfo prefilterInfo;
	VkDescriptorImageInfo irradianceInfo;
	VkDescriptorImageInfo albedoInfo;
	VkWriteDescriptorSet writes[6];
	VkSampler nearest;
	VkSampler linear;
	VkImageView prefilterView;
	VkImageView irradianceView;

	nearest = HYBRID1_NearestSampler();
	linear = HYBRID1_LinearSampler();
	HYBRID1_GetCubemapViews( &prefilterView, &irradianceView );
	if ( prefilterView == VK_NULL_HANDLE && tr.emptyCubemap ) {
		prefilterView = tr.emptyCubemap->view;
	}
	if ( irradianceView == VK_NULL_HANDLE && tr.emptyCubemap ) {
		irradianceView = tr.emptyCubemap->view;
	}

	Com_Memset( &depthInfo, 0, sizeof( depthInfo ) );
	depthInfo.sampler = nearest;
	depthInfo.imageView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	Com_Memset( &normalInfo, 0, sizeof( normalInfo ) );
	normalInfo.sampler = nearest;
	normalInfo.imageView = vk.deferred_gbuffer_normal_view ? vk.deferred_gbuffer_normal_view : depthInfo.imageView;
	normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &materialInfo, 0, sizeof( materialInfo ) );
	materialInfo.sampler = nearest;
	materialInfo.imageView = vk.deferred_gbuffer_material_view ? vk.deferred_gbuffer_material_view : depthInfo.imageView;
	materialInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &prefilterInfo, 0, sizeof( prefilterInfo ) );
	prefilterInfo.sampler = linear;
	prefilterInfo.imageView = prefilterView;
	prefilterInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &irradianceInfo, 0, sizeof( irradianceInfo ) );
	irradianceInfo.sampler = linear;
	irradianceInfo.imageView = irradianceView;
	irradianceInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &albedoInfo, 0, sizeof( albedoInfo ) );
	albedoInfo.sampler = nearest;
	albedoInfo.imageView = vk.deferred_gbuffer_albedo_view ? vk.deferred_gbuffer_albedo_view :
		( vk.deferred_gbuffer_material_view ? vk.deferred_gbuffer_material_view : depthInfo.imageView );
	albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	vk_rtx_bind_tlas_descriptor( set );
	HYBRID1_WriteImageBinding( set, 1, outputView, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_LAYOUT_GENERAL );
	HYBRID1_WriteUboBinding( set, 2 );

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = set;
	writes[0].dstBinding = 3;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &depthInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = set;
	writes[1].dstBinding = 4;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &normalInfo;
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = set;
	writes[2].dstBinding = 5;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[2].pImageInfo = &materialInfo;
	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = set;
	writes[3].dstBinding = 6;
	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[3].pImageInfo = &prefilterInfo;
	writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[4].dstSet = set;
	writes[4].dstBinding = 7;
	writes[4].descriptorCount = 1;
	writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[4].pImageInfo = &irradianceInfo;
	writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[5].dstSet = set;
	writes[5].dstBinding = 8;
	writes[5].descriptorCount = 1;
	writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[5].pImageInfo = &albedoInfo;
	qvkUpdateDescriptorSets( vk.device, 6, writes, 0, NULL );
}

static void HYBRID1_RefreshRtDescriptors( void )
{
	if ( !hybrid1.ready ) {
		return;
	}
	HYBRID1_UpdateRtDescriptors( hybrid1.shadow_set, hybrid1.raw_shadow.view );
	HYBRID1_UpdateRtDescriptors( hybrid1.spec_set, hybrid1.raw_spec.view );
	HYBRID1_UpdateRtDescriptors( hybrid1.diffuse_set, hybrid1.raw_diffuse.view );
}

static VkPipeline HYBRID1_CreateRtPipeline( VkShaderModule rgen, VkShaderModule rmiss, VkShaderModule rchit, const char *name )
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
	stages[1].module = rmiss;
	stages[1].pName = "main";
	stages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	stages[2].module = rchit;
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
	rtpci.layout = hybrid1.rt_pl;
	res = qvkCreateRayTracingPipelinesKHR( vk.device, VK_NULL_HANDLE, vk.pipelineCache, 1, &rtpci, NULL, &pipeline );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "[VK][Hybrid1] CreateRayTracingPipelinesKHR failed (%s) for %s\n",
			vk_result_string( res ), name );
		return VK_NULL_HANDLE;
	}
	SET_OBJECT_NAME( pipeline, name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
	return pipeline;
}

static void HYBRID1_BuildSbt( VkPipeline pipeline, VkBuffer *buf, VkDeviceMemory *mem )
{
	VkDeviceSize sbtSize;
	size_t hbufSize;
	uint8_t *host;
	uint8_t packed[96];
	uint32_t gi;

	if ( *buf != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, *buf, NULL );
		*buf = VK_NULL_HANDLE;
	}
	if ( *mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, *mem, NULL );
		*mem = VK_NULL_HANDLE;
	}

	hbufSize = (size_t)hybrid1.shader_group_base_alignment * 3u;
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
		VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, buf ) );
		qvkGetBufferMemoryRequirements( vk.device, *buf, &req );
		Com_Memset( &flagsInfo, 0, sizeof( flagsInfo ) );
		flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
		flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
		Com_Memset( &ai, 0, sizeof( ai ) );
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.pNext = &flagsInfo;
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, mem ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, *buf, *mem, 0 ) );
	}

	VK_CHECK( qvkMapMemory( vk.device, *mem, 0, sbtSize, 0, (void **)&host ) );
	Com_Memset( host, 0, hbufSize );
	VK_CHECK( qvkGetRayTracingShaderGroupHandlesKHR( vk.device, pipeline, 0, 3, hybrid1.handle_size * 3u, packed ) );
	for ( gi = 0; gi < 3; gi++ ) {
		Com_Memcpy( host + (size_t)hybrid1.shader_group_base_alignment * (size_t)gi,
			packed + (size_t)hybrid1.handle_size * (size_t)gi, hybrid1.handle_size );
	}
	qvkUnmapMemory( vk.device, *mem );
}

static void HYBRID1_TraceDispatch( VkCommandBuffer cmd, VkPipeline pipeline, VkBuffer sbtBuffer )
{
	VkBufferDeviceAddressInfo addrInfo;
	VkDeviceAddress sbtBase;
	VkStridedDeviceAddressRegionKHR raygenRegion;
	VkStridedDeviceAddressRegionKHR missRegion;
	VkStridedDeviceAddressRegionKHR hitRegion;
	VkStridedDeviceAddressRegionKHR callableRegion;

	Com_Memset( &addrInfo, 0, sizeof( addrInfo ) );
	addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addrInfo.buffer = sbtBuffer;
	sbtBase = qvkGetBufferDeviceAddress( vk.device, &addrInfo );

	Com_Memset( &raygenRegion, 0, sizeof( raygenRegion ) );
	raygenRegion.deviceAddress = sbtBase;
	raygenRegion.stride = hybrid1.shader_group_base_alignment;
	raygenRegion.size = hybrid1.shader_group_base_alignment;

	Com_Memset( &missRegion, 0, sizeof( missRegion ) );
	missRegion.deviceAddress = sbtBase + hybrid1.shader_group_base_alignment;
	missRegion.stride = hybrid1.shader_group_base_alignment;
	missRegion.size = hybrid1.shader_group_base_alignment;

	Com_Memset( &hitRegion, 0, sizeof( hitRegion ) );
	hitRegion.deviceAddress = sbtBase + hybrid1.shader_group_base_alignment * 2u;
	hitRegion.stride = hybrid1.shader_group_base_alignment;
	hitRegion.size = hybrid1.shader_group_base_alignment;

	Com_Memset( &callableRegion, 0, sizeof( callableRegion ) );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline );
	qvkCmdTraceRaysKHR( cmd, &raygenRegion, &missRegion, &hitRegion, &callableRegion,
		hybrid1.width, hybrid1.height, 1 );
}

static void HYBRID1_BarrierImage( VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
	VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage )
{
	VkImageMemoryBarrier b;

	Com_Memset( &b, 0, sizeof( b ) );
	b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	b.oldLayout = oldLayout;
	b.newLayout = newLayout;
	b.srcAccessMask = srcAccess;
	b.dstAccessMask = dstAccess;
	b.image = image;
	b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	b.subresourceRange.levelCount = 1;
	b.subresourceRange.layerCount = 1;
	qvkCmdPipelineBarrier( cmd, srcStage, dstStage, 0, 0, NULL, 0, NULL, 1, &b );
}

static void HYBRID1_BarrierColorRead( VkCommandBuffer cmd, VkImage image )
{
	HYBRID1_BarrierImage( cmd, image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
}

static void HYBRID1_BarrierColorWrite( VkCommandBuffer cmd, VkImage image, qboolean firstFrame )
{
	VkImageLayout oldLayout = firstFrame ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	HYBRID1_BarrierImage( cmd, image, oldLayout, VK_IMAGE_LAYOUT_GENERAL,
		0, VK_ACCESS_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
}

static void HYBRID1_CopyColorImage( VkCommandBuffer cmd, VkImage src, VkImage dst, qboolean dstFirstUse )
{
	VkImageCopy region;
	VkImageLayout dstOld = dstFirstUse ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &region, 0, sizeof( region ) );
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.srcSubresource.layerCount = 1;
	region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.dstSubresource.layerCount = 1;
	region.extent.width = hybrid1.width;
	region.extent.height = hybrid1.height;
	region.extent.depth = 1;

	HYBRID1_BarrierImage( cmd, src, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
	HYBRID1_BarrierImage( cmd, dst, dstOld, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		0, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
	qvkCmdCopyImage( cmd, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
	HYBRID1_BarrierImage( cmd, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	HYBRID1_BarrierImage( cmd, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
}

static void HYBRID1_BarrierDenoiseTargets( VkCommandBuffer cmd, qboolean firstFrame, qboolean doShadow, qboolean doSpec )
{
	if ( doShadow ) {
		HYBRID1_BarrierColorWrite( cmd, hybrid1.filtered_shadow.image, firstFrame );
		HYBRID1_BarrierColorWrite( cmd, hybrid1.hist_shadow[hybrid1.frame_index & 1u].image, firstFrame );
		HYBRID1_BarrierColorWrite( cmd, hybrid1.var_shadow[hybrid1.frame_index & 1u].image, firstFrame );
		HYBRID1_BarrierColorWrite( cmd, hybrid1.atrous_shadow.image, firstFrame );
	}
	if ( doSpec ) {
		HYBRID1_BarrierColorWrite( cmd, hybrid1.filtered_spec.image, firstFrame );
		HYBRID1_BarrierColorWrite( cmd, hybrid1.hist_spec[hybrid1.frame_index & 1u].image, firstFrame );
		HYBRID1_BarrierColorWrite( cmd, hybrid1.var_spec[hybrid1.frame_index & 1u].image, firstFrame );
		HYBRID1_BarrierColorWrite( cmd, hybrid1.atrous_spec.image, firstFrame );
	}
}

static void HYBRID1_FillFrameUbo( VkHybrid1FrameUBO_t *ubo )
{
	const float *view = backEnd.viewParms.world.modelViewMatrix;
	const float *projection = backEnd.useFirstPersonProjection
		? backEnd.firstPersonProjectionMatrix
		: backEnd.viewParms.projectionMatrix;
	float viewProj[16];
	float proj_vk[16];

	vk_get_projection_matrix_vk( projection, proj_vk );
	myGlMultMatrix( view, proj_vk, viewProj );
	Com_Memcpy( ubo->viewProj, viewProj, sizeof( ubo->viewProj ) );
	if ( !vk_mat4_inverse( viewProj, ubo->invViewProj ) ) {
		Com_Memcpy( ubo->invViewProj, viewProj, sizeof( ubo->invViewProj ) );
	}
	Com_Memcpy( ubo->prevViewProj, vk_prev_viewproj_matrix, sizeof( ubo->prevViewProj ) );
	ubo->viewOrigin[0] = backEnd.viewParms.or.origin[0];
	ubo->viewOrigin[1] = backEnd.viewParms.or.origin[1];
	ubo->viewOrigin[2] = backEnd.viewParms.or.origin[2];
	ubo->viewOrigin[3] = 0.0f;
	ubo->sunDirection[0] = tr.sunDirection[0];
	ubo->sunDirection[1] = tr.sunDirection[1];
	ubo->sunDirection[2] = tr.sunDirection[2];
	ubo->sunDirection[3] = 0.0f;
	ubo->outputSize[0] = (float)hybrid1.width;
	ubo->outputSize[1] = (float)hybrid1.height;
	ubo->outputSize[2] = 0.0f;
	ubo->outputSize[3] = 0.0f;
	ubo->params0[0] = r_hybrid1_historyGamma ? r_hybrid1_historyGamma->value : 1.25f;
	ubo->params0[1] = r_hybrid1_temporalAlpha ? r_hybrid1_temporalAlpha->value : 0.1f;
	ubo->params0[2] = ( r_hybrid1_reinhard && r_hybrid1_reinhard->integer ) ? 1.0f : 0.0f;
	ubo->params0[3] = (float)( hybrid1.frame_index + 1u );
	ubo->params1[0] = r_hybrid1_shadowStrength ? r_hybrid1_shadowStrength->value : 0.85f;
	ubo->params1[1] = r_hybrid1_specStrength ? r_hybrid1_specStrength->value : 1.0f;
	ubo->params1[2] = vk_deferred_gbuffer_fill_wanted() ? 1.0f : 0.0f;
	ubo->params1[3] = ( r_hybrid1_ibl && r_hybrid1_ibl->integer ) ? 1.0f : 0.0f;
}

qboolean vk_hybrid1_active( void )
{
	if ( !r_hybrid1 || r_hybrid1->integer <= 0 ) {
		return qfalse;
	}
	if ( !vk.rtxAvailable || !vk.fboActive ) {
		return qfalse;
	}
	if ( !r_rtxDemo || !r_rtxDemo->integer ) {
		return qfalse;
	}
	return ( hybrid1.ready && vk_rtx_scene_ready() ) ? qtrue : qfalse;
}

void vk_hybrid1_shutdown( void )
{
	if ( hybrid1.composite_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, hybrid1.composite_pipeline, NULL );
	}
	if ( hybrid1.atrous_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, hybrid1.atrous_pipeline, NULL );
	}
	if ( hybrid1.temporal_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, hybrid1.temporal_pipeline, NULL );
	}
	if ( hybrid1.shadow_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, hybrid1.shadow_pipeline, NULL );
	}
	if ( hybrid1.spec_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, hybrid1.spec_pipeline, NULL );
	}
	if ( hybrid1.diffuse_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, hybrid1.diffuse_pipeline, NULL );
	}
	if ( hybrid1.composite_pl != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, hybrid1.composite_pl, NULL );
	}
	if ( hybrid1.atrous_pl != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, hybrid1.atrous_pl, NULL );
	}
	if ( hybrid1.temporal_pl != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, hybrid1.temporal_pl, NULL );
	}
	if ( hybrid1.rt_pl != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, hybrid1.rt_pl, NULL );
	}
	if ( hybrid1.composite_dsl != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, hybrid1.composite_dsl, NULL );
	}
	if ( hybrid1.atrous_dsl != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, hybrid1.atrous_dsl, NULL );
	}
	if ( hybrid1.temporal_dsl != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, hybrid1.temporal_dsl, NULL );
	}
	if ( hybrid1.rt_dsl != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, hybrid1.rt_dsl, NULL );
	}
	if ( hybrid1.composite_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, hybrid1.composite_pool, NULL );
	}
	if ( hybrid1.atrous_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, hybrid1.atrous_pool, NULL );
	}
	if ( hybrid1.temporal_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, hybrid1.temporal_pool, NULL );
	}
	if ( hybrid1.rt_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, hybrid1.rt_pool, NULL );
	}
	if ( hybrid1.shadow_rgen != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, hybrid1.shadow_rgen, NULL );
	}
	if ( hybrid1.shadow_rmiss != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, hybrid1.shadow_rmiss, NULL );
	}
	if ( hybrid1.shadow_rchit != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, hybrid1.shadow_rchit, NULL );
	}
	if ( hybrid1.spec_rgen != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, hybrid1.spec_rgen, NULL );
	}
	if ( hybrid1.spec_rmiss != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, hybrid1.spec_rmiss, NULL );
	}
	if ( hybrid1.spec_rchit != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, hybrid1.spec_rchit, NULL );
	}
	if ( hybrid1.diffuse_rgen != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, hybrid1.diffuse_rgen, NULL );
	}
	if ( hybrid1.diffuse_rmiss != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, hybrid1.diffuse_rmiss, NULL );
	}
	if ( hybrid1.diffuse_rchit != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, hybrid1.diffuse_rchit, NULL );
	}
	if ( hybrid1.temporal_cs != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, hybrid1.temporal_cs, NULL );
	}
	if ( hybrid1.atrous_cs != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, hybrid1.atrous_cs, NULL );
	}
	if ( hybrid1.composite_cs != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, hybrid1.composite_cs, NULL );
	}
	if ( hybrid1.sbt_shadow_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, hybrid1.sbt_shadow_buffer, NULL );
	}
	if ( hybrid1.sbt_shadow_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, hybrid1.sbt_shadow_memory, NULL );
	}
	if ( hybrid1.sbt_spec_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, hybrid1.sbt_spec_buffer, NULL );
	}
	if ( hybrid1.sbt_spec_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, hybrid1.sbt_spec_memory, NULL );
	}
	if ( hybrid1.sbt_diffuse_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, hybrid1.sbt_diffuse_buffer, NULL );
	}
	if ( hybrid1.sbt_diffuse_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, hybrid1.sbt_diffuse_memory, NULL );
	}
	if ( hybrid1.ubo != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, hybrid1.ubo, NULL );
	}
	if ( hybrid1.ubo_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, hybrid1.ubo_memory, NULL );
	}
	HYBRID1_DestroyAllImages();
	HYBRID1_UnregisterCommands();
	Com_Memset( &hybrid1, 0, sizeof( hybrid1 ) );
}

void vk_hybrid1_init( void )
{
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps;
	VkPhysicalDeviceProperties2 props2;
	VkDescriptorSetLayoutBinding rtBindings[9];
	VkDescriptorSetLayoutBinding temporalBindings[9];
	VkDescriptorSetLayoutBinding atrousBindings[7];
	VkDescriptorSetLayoutBinding compositeBindings[6];
	VkDescriptorSetLayoutCreateInfo dslci;
	VkPipelineLayoutCreateInfo plci;
	VkDescriptorPoolSize poolSizes[8];
	VkDescriptorPoolCreateInfo dpci;
	VkDescriptorSetAllocateInfo allocInfo;
	VkComputePipelineCreateInfo cpci;
	VkPipelineShaderStageCreateInfo csStage;
	VkPushConstantRange pcRange;
	uint32_t w, h;
	VkDeviceSize uboSize;

	vk_hybrid1_shutdown();

	if ( !vk.rtxAvailable || !r_hybrid1 || r_hybrid1->integer <= 0 ) {
		return;
	}
	if ( !r_rtxDemo || !r_rtxDemo->integer ) {
		ri.Printf( PRINT_WARNING, "[VK][Hybrid1] requires r_rtxDemo 1 (latched) for world TLAS\n" );
		return;
	}
	if ( !vk_rtx_scene_ready() ) {
		ri.Printf( PRINT_DEVELOPER, "[VK][Hybrid1] defer init until RTX scene ready\n" );
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
	hybrid1.handle_size = rtProps.shaderGroupHandleSize;
	hybrid1.shader_group_base_alignment = rtProps.shaderGroupBaseAlignment;

	hybrid1.shadow_rgen = HYBRID1_ShaderModule( vk_hybrid1_shadow_rgen_spv, VK_HYBRID1_SHADOW_RGEN_SPV_SIZE, "hybrid1_shadow.rgen" );
	hybrid1.shadow_rmiss = HYBRID1_ShaderModule( vk_hybrid1_shadow_rmiss_spv, VK_HYBRID1_SHADOW_RMISS_SPV_SIZE, "hybrid1_shadow.rmiss" );
	hybrid1.shadow_rchit = HYBRID1_ShaderModule( vk_hybrid1_shadow_rchit_spv, VK_HYBRID1_SHADOW_RCHIT_SPV_SIZE, "hybrid1_shadow.rchit" );
	hybrid1.spec_rgen = HYBRID1_ShaderModule( vk_hybrid1_spec_rgen_spv, VK_HYBRID1_SPEC_RGEN_SPV_SIZE, "hybrid1_spec.rgen" );
	hybrid1.spec_rmiss = HYBRID1_ShaderModule( vk_hybrid1_spec_rmiss_spv, VK_HYBRID1_SPEC_RMISS_SPV_SIZE, "hybrid1_spec.rmiss" );
	hybrid1.spec_rchit = HYBRID1_ShaderModule( vk_hybrid1_spec_rchit_spv, VK_HYBRID1_SPEC_RCHIT_SPV_SIZE, "hybrid1_spec.rchit" );
	hybrid1.diffuse_rgen = HYBRID1_ShaderModule( vk_hybrid1_diffuse_rgen_spv, VK_HYBRID1_DIFFUSE_RGEN_SPV_SIZE, "hybrid1_diffuse.rgen" );
	hybrid1.diffuse_rmiss = HYBRID1_ShaderModule( vk_hybrid1_diffuse_rmiss_spv, VK_HYBRID1_DIFFUSE_RMISS_SPV_SIZE, "hybrid1_diffuse.rmiss" );
	hybrid1.diffuse_rchit = HYBRID1_ShaderModule( vk_hybrid1_diffuse_rchit_spv, VK_HYBRID1_DIFFUSE_RCHIT_SPV_SIZE, "hybrid1_diffuse.rchit" );
	hybrid1.temporal_cs = HYBRID1_ShaderModule( vk_hybrid1_temporal_cs_spv, VK_HYBRID1_TEMPORAL_CS_SPV_SIZE, "hybrid1_temporal.comp" );
	hybrid1.atrous_cs = HYBRID1_ShaderModule( vk_hybrid1_atrous_cs_spv, VK_HYBRID1_ATROUS_CS_SPV_SIZE, "hybrid1_atrous.comp" );
	hybrid1.composite_cs = HYBRID1_ShaderModule( vk_hybrid1_composite_cs_spv, VK_HYBRID1_COMPOSITE_CS_SPV_SIZE, "hybrid1_composite.comp" );

	vk_rtx_scene_extent( &w, &h );
	HYBRID1_CreateImages( w, h );

	uboSize = (VkDeviceSize)PAD( (uint32_t)sizeof( VkHybrid1FrameUBO_t ), (uint32_t)vk.uniform_alignment );
	{
		VkBufferCreateInfo bi;
		VkMemoryRequirements req;
		VkMemoryAllocateInfo ai;

		Com_Memset( &bi, 0, sizeof( bi ) );
		bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bi.size = uboSize;
		bi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, &hybrid1.ubo ) );
		qvkGetBufferMemoryRequirements( vk.device, hybrid1.ubo, &req );
		Com_Memset( &ai, 0, sizeof( ai ) );
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &hybrid1.ubo_memory ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, hybrid1.ubo, hybrid1.ubo_memory, 0 ) );
		VK_CHECK( qvkMapMemory( vk.device, hybrid1.ubo_memory, 0, uboSize, 0, &hybrid1.ubo_ptr ) );
	}

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
	rtBindings[4].binding = 4;
	rtBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	rtBindings[4].descriptorCount = 1;
	rtBindings[4].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	rtBindings[5].binding = 5;
	rtBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	rtBindings[5].descriptorCount = 1;
	rtBindings[5].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	rtBindings[6].binding = 6;
	rtBindings[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	rtBindings[6].descriptorCount = 1;
	rtBindings[6].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
	rtBindings[7].binding = 7;
	rtBindings[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	rtBindings[7].descriptorCount = 1;
	rtBindings[7].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;

	rtBindings[8].binding = 8;
	rtBindings[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	rtBindings[8].descriptorCount = 1;
	rtBindings[8].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

	Com_Memset( &dslci, 0, sizeof( dslci ) );
	dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslci.bindingCount = 9;
	dslci.pBindings = rtBindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &dslci, NULL, &hybrid1.rt_dsl ) );

	Com_Memset( poolSizes, 0, sizeof( poolSizes ) );
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	poolSizes[0].descriptorCount = 3;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[1].descriptorCount = 3;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[2].descriptorCount = 3;
	poolSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[3].descriptorCount = 18;
	Com_Memset( &dpci, 0, sizeof( dpci ) );
	dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpci.maxSets = 3;
	dpci.poolSizeCount = 4;
	dpci.pPoolSizes = poolSizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &dpci, NULL, &hybrid1.rt_pool ) );

	Com_Memset( &plci, 0, sizeof( plci ) );
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &hybrid1.rt_dsl;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &hybrid1.rt_pl ) );

	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = hybrid1.rt_pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &hybrid1.rt_dsl;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &hybrid1.shadow_set ) );
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &hybrid1.spec_set ) );
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &hybrid1.diffuse_set ) );

	HYBRID1_UpdateRtDescriptors( hybrid1.shadow_set, hybrid1.raw_shadow.view );
	HYBRID1_UpdateRtDescriptors( hybrid1.spec_set, hybrid1.raw_spec.view );
	HYBRID1_UpdateRtDescriptors( hybrid1.diffuse_set, hybrid1.raw_diffuse.view );

	hybrid1.shadow_pipeline = HYBRID1_CreateRtPipeline( hybrid1.shadow_rgen, hybrid1.shadow_rmiss, hybrid1.shadow_rchit, "hybrid1_shadow_rt" );
	hybrid1.spec_pipeline = HYBRID1_CreateRtPipeline( hybrid1.spec_rgen, hybrid1.spec_rmiss, hybrid1.spec_rchit, "hybrid1_spec_rt" );
	hybrid1.diffuse_pipeline = HYBRID1_CreateRtPipeline( hybrid1.diffuse_rgen, hybrid1.diffuse_rmiss, hybrid1.diffuse_rchit, "hybrid1_diffuse_rt" );
	if ( hybrid1.shadow_pipeline == VK_NULL_HANDLE || hybrid1.spec_pipeline == VK_NULL_HANDLE ||
		hybrid1.diffuse_pipeline == VK_NULL_HANDLE ) {
		vk_hybrid1_shutdown();
		return;
	}
	HYBRID1_BuildSbt( hybrid1.shadow_pipeline, &hybrid1.sbt_shadow_buffer, &hybrid1.sbt_shadow_memory );
	HYBRID1_BuildSbt( hybrid1.spec_pipeline, &hybrid1.sbt_spec_buffer, &hybrid1.sbt_spec_memory );
	HYBRID1_BuildSbt( hybrid1.diffuse_pipeline, &hybrid1.sbt_diffuse_buffer, &hybrid1.sbt_diffuse_memory );

	/* temporal compute DSL */
	Com_Memset( temporalBindings, 0, sizeof( temporalBindings ) );
	temporalBindings[0].binding = 0;
	temporalBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	temporalBindings[0].descriptorCount = 1;
	temporalBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	temporalBindings[1].binding = 1;
	temporalBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	temporalBindings[1].descriptorCount = 1;
	temporalBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	temporalBindings[2].binding = 2;
	temporalBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	temporalBindings[2].descriptorCount = 1;
	temporalBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	temporalBindings[3].binding = 3;
	temporalBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	temporalBindings[3].descriptorCount = 1;
	temporalBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	temporalBindings[4].binding = 4;
	temporalBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	temporalBindings[4].descriptorCount = 1;
	temporalBindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	temporalBindings[5].binding = 5;
	temporalBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	temporalBindings[5].descriptorCount = 1;
	temporalBindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	temporalBindings[6].binding = 6;
	temporalBindings[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	temporalBindings[6].descriptorCount = 1;
	temporalBindings[6].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	temporalBindings[7].binding = 7;
	temporalBindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	temporalBindings[7].descriptorCount = 1;
	temporalBindings[7].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	temporalBindings[8].binding = 8;
	temporalBindings[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	temporalBindings[8].descriptorCount = 1;
	temporalBindings[8].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	dslci.bindingCount = 9;
	dslci.pBindings = temporalBindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &dslci, NULL, &hybrid1.temporal_dsl ) );

	pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pcRange.offset = 0;
	pcRange.size = 160;
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &hybrid1.temporal_dsl;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pcRange;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &hybrid1.temporal_pl ) );

	Com_Memset( poolSizes, 0, sizeof( poolSizes ) );
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[0].descriptorCount = 20;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[1].descriptorCount = 8;
	dpci.maxSets = 2;
	dpci.poolSizeCount = 2;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &dpci, NULL, &hybrid1.temporal_pool ) );
	allocInfo.descriptorPool = hybrid1.temporal_pool;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &hybrid1.temporal_shadow_set ) );
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &hybrid1.temporal_spec_set ) );

	Com_Memset( &csStage, 0, sizeof( csStage ) );
	csStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	csStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	csStage.module = hybrid1.temporal_cs;
	csStage.pName = "main";

	Com_Memset( &cpci, 0, sizeof( cpci ) );
	cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	cpci.stage = csStage;
	cpci.layout = hybrid1.temporal_pl;
	VK_CHECK( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &cpci, NULL, &hybrid1.temporal_pipeline ) );

	/* atrous */
	Com_Memset( atrousBindings, 0, sizeof( atrousBindings ) );
	for ( w = 0; w < 6; w++ ) {
		atrousBindings[w].binding = w;
		atrousBindings[w].descriptorCount = 1;
		atrousBindings[w].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}
	atrousBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	atrousBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	atrousBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	atrousBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	atrousBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	atrousBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	atrousBindings[6].binding = 6;
	atrousBindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	atrousBindings[6].descriptorCount = 1;
	atrousBindings[6].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	dslci.bindingCount = 7;
	dslci.pBindings = atrousBindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &dslci, NULL, &hybrid1.atrous_dsl ) );
	pcRange.size = 48;
	plci.pSetLayouts = &hybrid1.atrous_dsl;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &hybrid1.atrous_pl ) );
	dpci.maxSets = 3;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &dpci, NULL, &hybrid1.atrous_pool ) );
	allocInfo.descriptorPool = hybrid1.atrous_pool;
	allocInfo.pSetLayouts = &hybrid1.atrous_dsl;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &hybrid1.atrous_shadow_set ) );
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &hybrid1.atrous_spec_set ) );
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &hybrid1.atrous_diffuse_set ) );
	csStage.module = hybrid1.atrous_cs;
	cpci.layout = hybrid1.atrous_pl;
	VK_CHECK( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &cpci, NULL, &hybrid1.atrous_pipeline ) );

	/* composite */
	Com_Memset( compositeBindings, 0, sizeof( compositeBindings ) );
	compositeBindings[0].binding = 0;
	compositeBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	compositeBindings[0].descriptorCount = 1;
	compositeBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	compositeBindings[1].binding = 1;
	compositeBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	compositeBindings[1].descriptorCount = 1;
	compositeBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	compositeBindings[2].binding = 2;
	compositeBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	compositeBindings[2].descriptorCount = 1;
	compositeBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	compositeBindings[3].binding = 3;
	compositeBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	compositeBindings[3].descriptorCount = 1;
	compositeBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	compositeBindings[4].binding = 4;
	compositeBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	compositeBindings[4].descriptorCount = 1;
	compositeBindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	compositeBindings[5].binding = 5;
	compositeBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	compositeBindings[5].descriptorCount = 1;
	compositeBindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	dslci.bindingCount = 6;
	dslci.pBindings = compositeBindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &dslci, NULL, &hybrid1.composite_dsl ) );
	pcRange.size = 32;
	plci.pSetLayouts = &hybrid1.composite_dsl;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &hybrid1.composite_pl ) );
	dpci.maxSets = 1;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &dpci, NULL, &hybrid1.composite_pool ) );
	allocInfo.descriptorPool = hybrid1.composite_pool;
	allocInfo.pSetLayouts = &hybrid1.composite_dsl;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &hybrid1.composite_set ) );
	csStage.module = hybrid1.composite_cs;
	cpci.layout = hybrid1.composite_pl;
	VK_CHECK( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &cpci, NULL, &hybrid1.composite_pipeline ) );

	hybrid1.ready = qtrue;
	ri.Printf( PRINT_ALL,
		"[VK][Hybrid1] Granja/Pereira hybrid pipeline ready (historyClamp=%d adaptiveBlur=%d separable=%d reinhard=%d atrousIters=%d ibl=%d diffuse=%d motion=%d)\n",
		r_hybrid1_historyClamp ? r_hybrid1_historyClamp->integer : 1,
		r_hybrid1_adaptiveBlur ? r_hybrid1_adaptiveBlur->integer : 1,
		r_hybrid1_separableBlur ? r_hybrid1_separableBlur->integer : 1,
		r_hybrid1_reinhard ? r_hybrid1_reinhard->integer : 1,
		r_hybrid1_atrousIters ? r_hybrid1_atrousIters->integer : 4,
		r_hybrid1_ibl ? r_hybrid1_ibl->integer : 1,
		r_hybrid1_diffuse ? r_hybrid1_diffuse->integer : 0,
		r_hybrid1_motion ? r_hybrid1_motion->integer : 1 );
	HYBRID1_RegisterCommands();
}

void vk_hybrid1_frame_begin( void )
{
	uint32_t w, h;

	HYBRID1_ConsumeCvarResets();

	if ( !hybrid1.ready ) {
		if ( r_hybrid1 && r_hybrid1->integer > 0 && vk.rtxAvailable && vk_rtx_scene_ready() ) {
			vk_hybrid1_init();
		}
		return;
	}
	vk_rtx_scene_extent( &w, &h );
	if ( w != hybrid1.width || h != hybrid1.height ) {
		HYBRID1_CreateImages( w, h );
		HYBRID1_UpdateRtDescriptors( hybrid1.shadow_set, hybrid1.raw_shadow.view );
		HYBRID1_UpdateRtDescriptors( hybrid1.spec_set, hybrid1.raw_spec.view );
		HYBRID1_UpdateRtDescriptors( hybrid1.diffuse_set, hybrid1.raw_diffuse.view );
		HYBRID1_ResetHistory();
	}
}

static void HYBRID1_BindTemporalSet( VkDescriptorSet set, hybrid1_image_t *raw, hybrid1_image_t *histRead,
	hybrid1_image_t *filtered, hybrid1_image_t *histWrite, hybrid1_image_t *varRead, hybrid1_image_t *varWrite )
{
	VkSampler nearest = HYBRID1_NearestSampler();
	VkDescriptorImageInfo infos[9];
	VkWriteDescriptorSet writes[9];
	uint32_t i;
	VkImageView motionView;

	Com_Memset( infos, 0, sizeof( infos ) );
	Com_Memset( writes, 0, sizeof( writes ) );
	infos[0].sampler = nearest;
	infos[0].imageView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	infos[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	motionView = vk.motion_vector_view ? vk.motion_vector_view : infos[0].imageView;
	infos[1].sampler = nearest;
	infos[1].imageView = vk.deferred_gbuffer_normal_view ? vk.deferred_gbuffer_normal_view : infos[0].imageView;
	infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	infos[2].sampler = nearest;
	infos[2].imageView = raw->view;
	infos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	infos[3].sampler = nearest;
	infos[3].imageView = histRead->view;
	infos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	infos[4].imageView = filtered->view;
	infos[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	infos[5].imageView = histWrite->view;
	infos[5].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	infos[6].sampler = nearest;
	infos[6].imageView = varRead->view;
	infos[6].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	infos[7].imageView = varWrite->view;
	infos[7].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	infos[8].sampler = nearest;
	infos[8].imageView = motionView;
	infos[8].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	for ( i = 0; i < 9; i++ ) {
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = set;
		writes[i].dstBinding = i;
		writes[i].descriptorCount = 1;
		if ( i <= 3 || i == 6 || i == 8 ) {
			writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[i].pImageInfo = &infos[i];
		} else {
			writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			writes[i].pImageInfo = &infos[i];
		}
	}
	qvkUpdateDescriptorSets( vk.device, 9, writes, 0, NULL );
}

static void HYBRID1_RecordTemporal( VkCommandBuffer cmd, VkDescriptorSet set, float channel,
	hybrid1_image_t *raw, hybrid1_image_t *histRead, hybrid1_image_t *filtered,
	hybrid1_image_t *histWrite, hybrid1_image_t *varRead, hybrid1_image_t *varWrite )
{
	struct {
		float invViewProj[16];
		float prevViewProj[16];
		float extent[4];
		float params[4];
		uint32_t hasGBuffer;
		uint32_t firstFrame;
		uint32_t useMotion;
		uint32_t pad1;
	} push;
	VkHybrid1FrameUBO_t uboLocal;
	uint32_t gx;
	uint32_t gy;

	HYBRID1_BindTemporalSet( set, raw, histRead, filtered, histWrite, varRead, varWrite );
	HYBRID1_FillFrameUbo( &uboLocal );
	if ( hybrid1.ubo_ptr ) {
		Com_Memcpy( hybrid1.ubo_ptr, &uboLocal, sizeof( uboLocal ) );
	}

	Com_Memcpy( push.invViewProj, uboLocal.invViewProj, sizeof( push.invViewProj ) );
	Com_Memcpy( push.prevViewProj, uboLocal.prevViewProj, sizeof( push.prevViewProj ) );
	push.extent[0] = (float)hybrid1.width;
	push.extent[1] = (float)hybrid1.height;
	push.extent[2] = 0.0f;
	push.extent[3] = 0.0f;
	push.params[0] = uboLocal.params0[0];
	push.params[1] = uboLocal.params0[1];
	push.params[2] = ( r_hybrid1_historyClamp && r_hybrid1_historyClamp->integer ) ? 1.0f : 0.0f;
	push.params[3] = channel;
	push.hasGBuffer = vk_deferred_gbuffer_fill_wanted() ? 1u : 0u;
	push.firstFrame = ( hybrid1.frame_index == 0 ) ? 1u : 0u;
	push.useMotion = ( r_hybrid1_motion && r_hybrid1_motion->integer &&
		vk.motion_vector_view != VK_NULL_HANDLE && !vk.temporal.unreliableMotionThisFrame ) ? 1u : 0u;

	gx = ( hybrid1.width + 7u ) / 8u;
	gy = ( hybrid1.height + 7u ) / 8u;
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, hybrid1.temporal_pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, hybrid1.temporal_pl, 0, 1, &set, 0, NULL );
	qvkCmdPushConstants( cmd, hybrid1.temporal_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
	qvkCmdDispatch( cmd, gx, gy, 1 );
}

static hybrid1_image_t *HYBRID1_RecordAtrous( VkCommandBuffer cmd, VkDescriptorSet set, hybrid1_image_t *rawGuide,
	hybrid1_image_t *varImg, hybrid1_image_t *inImg, hybrid1_image_t *outImg, float channel )
{
	struct {
		float extent[4];
		float params[4];
		uint32_t hasGBuffer;
		uint32_t adaptiveBlur;
		uint32_t separableBlur;
		uint32_t reinhard;
	} push;
	VkSampler nearest = HYBRID1_NearestSampler();
	VkDescriptorImageInfo infos[7];
	VkWriteDescriptorSet writes[7];
	hybrid1_image_t *ping = inImg;
	hybrid1_image_t *pong = outImg;
	int iter;
	int axis;
	int maxIter;

	maxIter = r_hybrid1_atrousIters ? r_hybrid1_atrousIters->integer : 4;
	if ( maxIter < 0 ) {
		maxIter = 0;
	}
	if ( maxIter > 4 ) {
		maxIter = 4;
	}
	if ( maxIter == 0 ) {
		return inImg;
	}

	for ( iter = 0; iter < maxIter; iter++ ) {
		int numAxes = ( r_hybrid1_separableBlur && r_hybrid1_separableBlur->integer ) ? 2 : 1;
		for ( axis = 0; axis < numAxes; axis++ ) {
			Com_Memset( infos, 0, sizeof( infos ) );
			Com_Memset( writes, 0, sizeof( writes ) );
			infos[0].sampler = nearest;
			infos[0].imageView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
			infos[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			infos[1].sampler = nearest;
			infos[1].imageView = vk.deferred_gbuffer_normal_view ? vk.deferred_gbuffer_normal_view : infos[0].imageView;
			infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			infos[2].sampler = nearest;
			infos[2].imageView = vk.deferred_gbuffer_material_view ? vk.deferred_gbuffer_material_view : infos[0].imageView;
			infos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			infos[3].sampler = nearest;
			infos[3].imageView = rawGuide->view;
			infos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			infos[4].sampler = nearest;
			infos[4].imageView = varImg->view;
			infos[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			infos[5].sampler = nearest;
			infos[5].imageView = ping->view;
			infos[5].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			infos[6].imageView = pong->view;
			infos[6].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			{
				uint32_t wi;
				for ( wi = 0; wi < 7; wi++ ) {
					writes[wi].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					writes[wi].dstSet = set;
					writes[wi].dstBinding = wi;
					writes[wi].descriptorCount = 1;
					if ( wi < 6 ) {
						writes[wi].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						writes[wi].pImageInfo = &infos[wi];
					} else {
						writes[wi].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
						writes[wi].pImageInfo = &infos[wi];
					}
				}
			}
			qvkUpdateDescriptorSets( vk.device, 7, writes, 0, NULL );

			push.extent[0] = (float)hybrid1.width;
			push.extent[1] = (float)hybrid1.height;
			push.params[0] = (float)iter;
			push.params[1] = 0.35f;
			push.params[2] = channel;
			push.params[3] = ( r_hybrid1_separableBlur && r_hybrid1_separableBlur->integer ) ? (float)axis : 0.0f;
			push.hasGBuffer = vk_deferred_gbuffer_fill_wanted() ? 1u : 0u;
			push.adaptiveBlur = ( r_hybrid1_adaptiveBlur && r_hybrid1_adaptiveBlur->integer ) ? 1u : 0u;
			push.separableBlur = ( r_hybrid1_separableBlur && r_hybrid1_separableBlur->integer ) ? 1u : 0u;
			push.reinhard = ( channel < 1.5f && r_hybrid1_reinhard && r_hybrid1_reinhard->integer ) ? 1u : 0u;

			qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, hybrid1.atrous_pipeline );
			qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, hybrid1.atrous_pl, 0, 1, &set, 0, NULL );
			qvkCmdPushConstants( cmd, hybrid1.atrous_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
			qvkCmdDispatch( cmd, ( hybrid1.width + 7u ) / 8u, ( hybrid1.height + 7u ) / 8u, 1 );

			{
				hybrid1_image_t *tmp = ping;
				ping = pong;
				pong = tmp;
			}
		}
	}
	return ping;
}

void vk_hybrid1_record_pass( VkCommandBuffer cmd )
{
	VkHybrid1FrameUBO_t uboLocal;
	VkImageLayout colorRestoreLayout;
	VkImageAspectFlags depthAspect;
	uint32_t cur;
	uint32_t prev;
	hybrid1_image_t *denoisedShadow;
	hybrid1_image_t *denoisedSpec;
	hybrid1_image_t *denoisedDiffuse;
	qboolean doDiffuse;
	qboolean doShadow;
	qboolean doSpec;

	if ( !vk_hybrid1_active() || !cmd || !hybrid1.ready ) {
		return;
	}
	if ( vk.renderPassIndex != RENDER_PASS_MAIN && vk.renderPassIndex != RENDER_PASS_POST_BLOOM ) {
		return;
	}

	vk_rtx_scene_prepare();

	if ( vk_temporal_has_reason( VK_TEMPORAL_RESET_CAMERA_CUT | VK_TEMPORAL_RESET_MISSING_PREV_DATA |
		VK_TEMPORAL_RESET_RENDERER_INIT | VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE |
		VK_TEMPORAL_RESET_RENDER_SIZE_CHANGE | VK_TEMPORAL_RESET_WORLD_CHANGE |
		VK_TEMPORAL_RESET_CLIENT_STATE_CHANGE ) ) {
		HYBRID1_ResetHistory();
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

	doDiffuse = ( r_hybrid1_diffuse && r_hybrid1_diffuse->integer ) ? qtrue : qfalse;
	doShadow = ( !r_hybrid1_shadow || r_hybrid1_shadow->integer ) ? qtrue : qfalse;
	doSpec = ( !r_hybrid1_spec || r_hybrid1_spec->integer ) ? qtrue : qfalse;
	denoisedDiffuse = &hybrid1.filtered_diffuse;
	denoisedShadow = &hybrid1.filtered_shadow;
	denoisedSpec = &hybrid1.filtered_spec;

	cur = hybrid1.frame_index & 1u;
	prev = cur ^ 1u;

	HYBRID1_FillFrameUbo( &uboLocal );
	if ( hybrid1.ubo_ptr ) {
		Com_Memcpy( hybrid1.ubo_ptr, &uboLocal, sizeof( uboLocal ) );
	}
	HYBRID1_RefreshRtDescriptors();

	if ( doShadow ) {
		HYBRID1_BarrierImage( cmd, hybrid1.raw_shadow.image,
			hybrid1.traced ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR );
	}
	if ( doSpec ) {
		HYBRID1_BarrierImage( cmd, hybrid1.raw_spec.image,
			hybrid1.traced ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR );
	}
	if ( doDiffuse ) {
		HYBRID1_BarrierImage( cmd, hybrid1.raw_diffuse.image,
			hybrid1.traced ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR );
	}

	if ( doShadow ) {
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, hybrid1.rt_pl, 0, 1, &hybrid1.shadow_set, 0, NULL );
		HYBRID1_TraceDispatch( cmd, hybrid1.shadow_pipeline, hybrid1.sbt_shadow_buffer );
	}
	if ( doSpec ) {
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, hybrid1.rt_pl, 0, 1, &hybrid1.spec_set, 0, NULL );
		HYBRID1_TraceDispatch( cmd, hybrid1.spec_pipeline, hybrid1.sbt_spec_buffer );
	}
	if ( doDiffuse ) {
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, hybrid1.rt_pl, 0, 1, &hybrid1.diffuse_set, 0, NULL );
		HYBRID1_TraceDispatch( cmd, hybrid1.diffuse_pipeline, hybrid1.sbt_diffuse_buffer );
	}
	hybrid1.traced = qtrue;

	if ( doShadow ) {
		HYBRID1_BarrierImage( cmd, hybrid1.raw_shadow.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	}
	if ( doSpec ) {
		HYBRID1_BarrierImage( cmd, hybrid1.raw_spec.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	}
	if ( doDiffuse ) {
		HYBRID1_BarrierImage( cmd, hybrid1.raw_diffuse.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	}

	if ( doShadow || doSpec ) {
		qboolean firstFrame = ( hybrid1.frame_index == 0 ) ? qtrue : qfalse;

		if ( r_hybrid1_motion && r_hybrid1_motion->integer && vk.motion_vector_image != VK_NULL_HANDLE ) {
			vk_barrier_motion_vector_for_sampling( "hybrid1 pre-temporal" );
		}

		if ( !firstFrame ) {
			if ( doShadow ) {
				HYBRID1_BarrierColorRead( cmd, hybrid1.hist_shadow[prev].image );
				HYBRID1_BarrierColorRead( cmd, hybrid1.var_shadow[prev].image );
			}
			if ( doSpec ) {
				HYBRID1_BarrierColorRead( cmd, hybrid1.hist_spec[prev].image );
				HYBRID1_BarrierColorRead( cmd, hybrid1.var_spec[prev].image );
			}
		}
		HYBRID1_BarrierDenoiseTargets( cmd, firstFrame, doShadow, doSpec );
	}

	if ( doShadow ) {
		HYBRID1_RecordTemporal( cmd, hybrid1.temporal_shadow_set, 0.0f,
			&hybrid1.raw_shadow, &hybrid1.hist_shadow[prev], &hybrid1.filtered_shadow,
			&hybrid1.hist_shadow[cur], &hybrid1.var_shadow[prev], &hybrid1.var_shadow[cur] );
	}
	if ( doSpec ) {
		HYBRID1_RecordTemporal( cmd, hybrid1.temporal_spec_set, 1.0f,
			&hybrid1.raw_spec, &hybrid1.hist_spec[prev], &hybrid1.filtered_spec,
			&hybrid1.hist_spec[cur], &hybrid1.var_spec[prev], &hybrid1.var_spec[cur] );
	}

	if ( doShadow ) {
		HYBRID1_BarrierColorRead( cmd, hybrid1.filtered_shadow.image );
		HYBRID1_BarrierColorRead( cmd, hybrid1.var_shadow[cur].image );
	}
	if ( doSpec ) {
		HYBRID1_BarrierColorRead( cmd, hybrid1.filtered_spec.image );
		HYBRID1_BarrierColorRead( cmd, hybrid1.var_spec[cur].image );
	}

	if ( r_hybrid1_atrousIters && r_hybrid1_atrousIters->integer > 0 ) {
		if ( doShadow ) {
			denoisedShadow = HYBRID1_RecordAtrous( cmd, hybrid1.atrous_shadow_set, &hybrid1.raw_shadow,
				&hybrid1.var_shadow[cur], &hybrid1.filtered_shadow, &hybrid1.atrous_shadow, 0.0f );
		}
		if ( doSpec ) {
			denoisedSpec = HYBRID1_RecordAtrous( cmd, hybrid1.atrous_spec_set, &hybrid1.raw_spec,
				&hybrid1.var_spec[cur], &hybrid1.filtered_spec, &hybrid1.atrous_spec, 1.0f );
		}
	}

	if ( doDiffuse ) {
		if ( r_hybrid1_atrousIters && r_hybrid1_atrousIters->integer > 0 ) {
			HYBRID1_CopyColorImage( cmd, hybrid1.raw_diffuse.image, hybrid1.filtered_diffuse.image,
				( hybrid1.frame_index == 0 ) ? qtrue : qfalse );
			HYBRID1_BarrierColorWrite( cmd, hybrid1.atrous_diffuse.image, qfalse );
			denoisedDiffuse = HYBRID1_RecordAtrous( cmd, hybrid1.atrous_diffuse_set, &hybrid1.raw_diffuse,
				&hybrid1.raw_diffuse, &hybrid1.filtered_diffuse, &hybrid1.atrous_diffuse, 2.0f );
		} else {
			denoisedDiffuse = &hybrid1.raw_diffuse;
		}
	}

	if ( !doShadow ) {
		denoisedShadow = &hybrid1.raw_shadow;
	}
	if ( !doSpec ) {
		denoisedSpec = &hybrid1.raw_spec;
	}
	if ( !doDiffuse ) {
		denoisedDiffuse = &hybrid1.raw_diffuse;
	}

	if ( denoisedShadow ) {
		HYBRID1_BarrierColorRead( cmd, denoisedShadow->image );
	}
	if ( denoisedSpec ) {
		HYBRID1_BarrierColorRead( cmd, denoisedSpec->image );
	}
	if ( doDiffuse && denoisedDiffuse ) {
		HYBRID1_BarrierColorRead( cmd, denoisedDiffuse->image );
	}

	{
		struct {
			float extent[4];
			float strengths[4];
		} push;
		VkSampler nearest = HYBRID1_NearestSampler();
		VkDescriptorImageInfo infos[6];
		VkWriteDescriptorSet writes[6];
		VkImageView albedoView;

		albedoView = vk.deferred_gbuffer_albedo_view ? vk.deferred_gbuffer_albedo_view :
			( vk.deferred_gbuffer_material_view ? vk.deferred_gbuffer_material_view : vk.color_image_view );

		HYBRID1_BarrierImage( cmd, vk.color_image, colorRestoreLayout, VK_IMAGE_LAYOUT_GENERAL,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

		Com_Memset( infos, 0, sizeof( infos ) );
		Com_Memset( writes, 0, sizeof( writes ) );
		infos[0].sampler = nearest;
		infos[0].imageView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
		infos[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		infos[1].sampler = nearest;
		infos[1].imageView = denoisedShadow->view;
		infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		infos[2].sampler = nearest;
		infos[2].imageView = denoisedSpec->view;
		infos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		infos[3].sampler = nearest;
		infos[3].imageView = denoisedDiffuse->view;
		infos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		infos[4].sampler = nearest;
		infos[4].imageView = albedoView;
		infos[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		infos[5].imageView = vk.color_image_view;
		infos[5].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = hybrid1.composite_set;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].pImageInfo = &infos[0];
		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = hybrid1.composite_set;
		writes[1].dstBinding = 1;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].pImageInfo = &infos[1];
		writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstSet = hybrid1.composite_set;
		writes[2].dstBinding = 2;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[2].pImageInfo = &infos[2];
		writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].dstSet = hybrid1.composite_set;
		writes[3].dstBinding = 3;
		writes[3].descriptorCount = 1;
		writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[3].pImageInfo = &infos[3];
		writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[4].dstSet = hybrid1.composite_set;
		writes[4].dstBinding = 4;
		writes[4].descriptorCount = 1;
		writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[4].pImageInfo = &infos[4];
		writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[5].dstSet = hybrid1.composite_set;
		writes[5].dstBinding = 5;
		writes[5].descriptorCount = 1;
		writes[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[5].pImageInfo = &infos[5];
		qvkUpdateDescriptorSets( vk.device, 6, writes, 0, NULL );

		push.extent[0] = (float)hybrid1.width;
		push.extent[1] = (float)hybrid1.height;
		push.strengths[0] = doShadow ? ( r_hybrid1_shadowStrength ? r_hybrid1_shadowStrength->value : 0.85f ) : 0.0f;
		push.strengths[1] = doSpec ? ( r_hybrid1_specStrength ? r_hybrid1_specStrength->value : 1.0f ) : 0.0f;
		push.strengths[2] = (float)( r_hybrid1_debug ? r_hybrid1_debug->integer : 0 );
		push.strengths[3] = doDiffuse ? ( r_hybrid1_diffuseStrength ? r_hybrid1_diffuseStrength->value : 1.0f ) : 0.0f;

		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, hybrid1.composite_pipeline );
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, hybrid1.composite_pl, 0, 1, &hybrid1.composite_set, 0, NULL );
		qvkCmdPushConstants( cmd, hybrid1.composite_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
		qvkCmdDispatch( cmd, ( hybrid1.width + 7u ) / 8u, ( hybrid1.height + 7u ) / 8u, 1 );

		HYBRID1_BarrierImage( cmd, vk.color_image, VK_IMAGE_LAYOUT_GENERAL, colorRestoreLayout,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	}

	hybrid1.frame_index++;
}

#else /* !USE_VULKAN_RTX */

void vk_hybrid1_init( void )
{
	static qboolean s_logged;

	if ( !s_logged ) {
		ri.Printf( PRINT_ALL, "[VK][Hybrid1] stub (build with -DUSE_VULKAN_RTX=ON)\n" );
		s_logged = qtrue;
	}
}
void vk_hybrid1_shutdown( void ) {}
void vk_hybrid1_frame_begin( void ) {}
qboolean vk_hybrid1_active( void ) { return qfalse; }
void vk_hybrid1_record_pass( VkCommandBuffer cmd ) { (void)cmd; }

#endif
