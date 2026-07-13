/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Deferred G-buffer sidecar (r_renderMode 1/2) + experimental lighting (mode 1).
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_deferred_gbuffer.h"
#include "vk_image_layout.h"
#include "vk_post_fog.h"
#include "vk_render_pass.h"
#include "vk_scene_pass.h"
#include "vk_util.h"
#include "vk_view_state.h"

typedef struct {
	vec4_t projInfo;
	vec4_t materialParams;
	uint32_t extent[2];
} vk_deferred_gbuf_push_t;

typedef struct {
	int mode;
} vk_deferred_gbuf_debug_push_t;

typedef struct {
	float viewMatrix[16];
	float projInfo[4];
	uint32_t extent[2];
	uint32_t tileGrid[2];
	float strength;
	uint32_t maxPerTile;
	uint32_t numLights;
	uint32_t additive;
	uint32_t specular;
} vk_deferred_light_push_t;

typedef struct {
	uint32_t additive;
} vk_deferred_composite_push_t;

static void vk_dgb_destroy_composite_gfx_pipeline( void )
{
	if ( vk.deferred_gbuffer.composite_gfx_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.deferred_gbuffer.composite_gfx_pipeline, NULL );
		vk.deferred_gbuffer.composite_gfx_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.composite_gfx_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.deferred_gbuffer.composite_gfx_pipeline_layout, NULL );
		vk.deferred_gbuffer.composite_gfx_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.composite_gfx_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.deferred_gbuffer.composite_gfx_layout, NULL );
		vk.deferred_gbuffer.composite_gfx_layout = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.composite_gfx_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.deferred_gbuffer.composite_gfx_pool, NULL );
		vk.deferred_gbuffer.composite_gfx_pool = VK_NULL_HANDLE;
	}
	vk.deferred_gbuffer.composite_gfx_descriptor = VK_NULL_HANDLE;
	vk.deferred_gbuffer.composite_gfx_ready = qfalse;
}

static void vk_dgb_destroy_lighting_pipeline( void )
{
	if ( vk.deferred_gbuffer.lighting_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.deferred_gbuffer.lighting_pipeline, NULL );
		vk.deferred_gbuffer.lighting_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.lighting_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.deferred_gbuffer.lighting_pipeline_layout, NULL );
		vk.deferred_gbuffer.lighting_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.lighting_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.deferred_gbuffer.lighting_layout, NULL );
		vk.deferred_gbuffer.lighting_layout = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.lighting_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.deferred_gbuffer.lighting_pool, NULL );
		vk.deferred_gbuffer.lighting_pool = VK_NULL_HANDLE;
	}
	vk.deferred_gbuffer.lighting_descriptor = VK_NULL_HANDLE;
	vk.deferred_gbuffer.lighting_pipeline_ready = qfalse;
	vk.deferred_gbuffer.lighting_logged = qfalse;
	vk.deferred_gbuffer.composite_logged = qfalse;
}

static void vk_dgb_destroy_debug_gfx_pipeline( void )
{
	if ( vk.deferred_gbuffer.debug_gfx_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.deferred_gbuffer.debug_gfx_pipeline, NULL );
		vk.deferred_gbuffer.debug_gfx_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.debug_gfx_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.deferred_gbuffer.debug_gfx_pipeline_layout, NULL );
		vk.deferred_gbuffer.debug_gfx_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.debug_gfx_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.deferred_gbuffer.debug_gfx_layout, NULL );
		vk.deferred_gbuffer.debug_gfx_layout = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.debug_gfx_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.deferred_gbuffer.debug_gfx_pool, NULL );
		vk.deferred_gbuffer.debug_gfx_pool = VK_NULL_HANDLE;
	}
	vk.deferred_gbuffer.debug_gfx_descriptor = VK_NULL_HANDLE;
	vk.deferred_gbuffer.debug_gfx_ready = qfalse;
}

qboolean vk_deferred_gbuffer_active( void )
{
	return ( vk.deferredGbufferAllocated && r_renderMode &&
		( r_renderMode->integer == 1 || r_renderMode->integer == 2 ) &&
		r_deferredGBuffer && r_deferredGBuffer->integer ) ? qtrue : qfalse;
}

qboolean vk_deferred_gbuffer_fill_wanted( void )
{
	return ( vk_deferred_gbuffer_active() && r_deferredGBufferFill && r_deferredGBufferFill->integer ) ? qtrue : qfalse;
}

static qboolean vk_deferred_lighting_wanted( void )
{
	return ( vk_deferred_gbuffer_fill_wanted() && r_deferredLighting && r_deferredLighting->integer &&
		r_renderMode && r_renderMode->integer == 1 && r_forwardPlus && r_forwardPlus->integer ) ? qtrue : qfalse;
}

qboolean vk_deferred_lighting_active( void )
{
	return vk_deferred_lighting_wanted();
}

qboolean vk_deferred_unlit_base_wanted( void )
{
	if ( !vk_deferred_lighting_wanted() ) {
		return qfalse;
	}
	if ( !r_deferredUnlitBase || !r_deferredUnlitBase->integer ) {
		return qfalse;
	}
	return qtrue;
}

static void vk_dgb_destroy_pipeline( void )
{
	vk_dgb_destroy_debug_gfx_pipeline();
	vk_dgb_destroy_composite_gfx_pipeline();
	vk_dgb_destroy_lighting_pipeline();
	if ( vk.deferred_gbuffer.pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.deferred_gbuffer.pipeline, NULL );
		vk.deferred_gbuffer.pipeline = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.deferred_gbuffer.pipeline_layout, NULL );
		vk.deferred_gbuffer.pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.deferred_gbuffer.layout, NULL );
		vk.deferred_gbuffer.layout = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.deferred_gbuffer.pool, NULL );
		vk.deferred_gbuffer.pool = VK_NULL_HANDLE;
	}
	vk.deferred_gbuffer.descriptor = VK_NULL_HANDLE;
	vk.deferred_gbuffer.pipeline_ready = qfalse;
	vk.deferred_gbuffer.fill_logged = qfalse;
}

void vk_deferred_gbuffer_init( void )
{
	Com_Memset( &vk.deferred_gbuffer, 0, sizeof( vk.deferred_gbuffer ) );
}

void vk_deferred_gbuffer_shutdown( void )
{
	vk_dgb_destroy_pipeline();
}

static void vk_dgb_create_descriptor_layout( void )
{
	VkDescriptorSetLayoutBinding bindings[3];
	VkDescriptorSetLayoutCreateInfo desc;

	if ( vk.deferred_gbuffer.layout != VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desc.bindingCount = 3;
	desc.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &desc, NULL, &vk.deferred_gbuffer.layout ) );
}

static void vk_dgb_create_pipeline( void )
{
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.deferred_gbuffer.pipeline_ready ) {
		return;
	}
	if ( vk.modules.deferred_gbuffer_fill_cs == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][deferred] deferred_gbuffer_fill compute shader missing\n" S_COLOR_WHITE );
		return;
	}

	vk_dgb_create_descriptor_layout();

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_deferred_gbuf_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.deferred_gbuffer.layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.deferred_gbuffer.pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.deferred_gbuffer_fill_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.deferred_gbuffer.pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.deferred_gbuffer.pipeline ) );
	SET_OBJECT_NAME( vk.deferred_gbuffer.pipeline, "pipeline - deferred gbuffer fill", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	{
		VkDescriptorPoolSize pool_sizes[2];
		VkDescriptorPoolCreateInfo pool_ci;
		VkDescriptorSetAllocateInfo alloc;

		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[0].descriptorCount = 1;
		pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		pool_sizes[1].descriptorCount = 2;

		Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
		pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_ci.maxSets = 1;
		pool_ci.poolSizeCount = 2;
		pool_ci.pPoolSizes = pool_sizes;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.deferred_gbuffer.pool ) );

		Com_Memset( &alloc, 0, sizeof( alloc ) );
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.descriptorPool = vk.deferred_gbuffer.pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.deferred_gbuffer.layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.deferred_gbuffer.descriptor ) );
	}

	vk.deferred_gbuffer.pipeline_ready = qtrue;
	ri.Printf( PRINT_ALL, "[VK][deferred] G-buffer fill compute pipeline ready\n" );
}

static void vk_dgb_update_descriptors( void )
{
	VkDescriptorImageInfo depth_info;
	VkDescriptorImageInfo normal_info;
	VkDescriptorImageInfo material_info;
	VkWriteDescriptorSet writes[3];
	Vk_Sampler_Def depth_sd;
	VkImageView depth_view;

	if ( vk.deferred_gbuffer.descriptor == VK_NULL_HANDLE ) {
		return;
	}

	depth_view = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	Com_Memset( &depth_sd, 0, sizeof( depth_sd ) );
	depth_sd.gl_mag_filter = depth_sd.gl_min_filter = GL_NEAREST;
	depth_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	depth_sd.noAnisotropy = qtrue;

	Com_Memset( &depth_info, 0, sizeof( depth_info ) );
	depth_info.sampler = vk_find_sampler( &depth_sd );
	depth_info.imageView = depth_view;
	depth_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	Com_Memset( &normal_info, 0, sizeof( normal_info ) );
	normal_info.imageView = vk.deferred_gbuffer_normal_view;
	normal_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( &material_info, 0, sizeof( material_info ) );
	material_info.imageView = vk.deferred_gbuffer_material_view;
	material_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.deferred_gbuffer.descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &depth_info;

	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vk.deferred_gbuffer.descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[1].pImageInfo = &normal_info;

	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = vk.deferred_gbuffer.descriptor;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[2].pImageInfo = &material_info;

	qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );
}

static void vk_dgb_fill_proj_info( vk_deferred_gbuf_push_t *push )
{
	const float *proj = backEnd.useFirstPersonProjection ? backEnd.firstPersonProjectionMatrix :
		backEnd.viewParms.projectionMatrix;
	float proj_vk[16];
	float aspect = ( backEnd.viewParms.viewportHeight > 0 ) ?
		( (float)backEnd.viewParms.viewportWidth / (float)backEnd.viewParms.viewportHeight ) : 1.0f;

	vk_get_projection_matrix_vk( proj, proj_vk );
	push->projInfo[0] = 1.0f / ( proj_vk[0] * aspect );
	push->projInfo[1] = 1.0f / proj_vk[5];
	push->projInfo[2] = proj_vk[10];
	push->projInfo[3] = proj_vk[14];
	push->materialParams[0] = r_deferredDefaultMetalness ?
		Com_Clamp( 0.0f, 1.0f, r_deferredDefaultMetalness->value ) : 0.0f;
	push->materialParams[1] = r_deferredDefaultRoughness ?
		Com_Clamp( 0.04f, 1.0f, r_deferredDefaultRoughness->value ) : 0.55f;
	push->materialParams[2] = r_deferredNormalEdgeThreshold ?
		Com_Clamp( 0.001f, 1.0f, r_deferredNormalEdgeThreshold->value ) : 0.08f;
	push->materialParams[3] = 0.0f;
}

void vk_deferred_gbuffer_capture_after_geometry( void )
{
	VkImageCopy region;
	VkImageAspectFlags depth_aspect;
	uint32_t width, height;
	vk_deferred_gbuf_push_t push;
	uint32_t gx, gy;
	qboolean resume_main;

	if ( !vk_deferred_gbuffer_fill_wanted() ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.color_image == VK_NULL_HANDLE || vk.deferred_gbuffer_albedo == VK_NULL_HANDLE ||
		vk.deferred_gbuffer_normal == VK_NULL_HANDLE || vk.deferred_gbuffer_material == VK_NULL_HANDLE ||
		vk.depth_image == VK_NULL_HANDLE ) {
		return;
	}

	resume_main = ( vk.inRenderPass && vk.renderPassIndex == RENDER_PASS_MAIN ) ? qtrue : qfalse;
	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}

	vk_dgb_create_pipeline();
	if ( !vk.deferred_gbuffer.pipeline_ready || vk.deferred_gbuffer.pipeline == VK_NULL_HANDLE ) {
		if ( resume_main ) {
			vk_resume_current_render_pass();
		}
		return;
	}

	if ( !vk.deferred_gbuffer.fill_logged ) {
		ri.Printf( PRINT_ALL, "[VK][deferred] r_deferredGBufferFill=1 (albedo copy + depth normals)\n" );
		vk.deferred_gbuffer.fill_logged = qtrue;
	}

	width = vk_get_render_target_width();
	height = vk_get_render_target_height();
	if ( width == 0 || height == 0 ) {
		if ( resume_main ) {
			vk_resume_current_render_pass();
		}
		return;
	}

	depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT );

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		0, 0 );
	record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_gbuffer_albedo, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		0, 0 );
	record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_gbuffer_normal, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );
	record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_gbuffer_material, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );

	Com_Memset( &region, 0, sizeof( region ) );
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.srcSubresource.layerCount = 1;
	region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.dstSubresource.layerCount = 1;
	region.extent.width = width;
	region.extent.height = height;
	region.extent.depth = 1;
	qvkCmdCopyImage( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		vk.deferred_gbuffer_albedo, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

	record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_gbuffer_albedo, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );

	vk_dgb_update_descriptors();

	vk_dgb_fill_proj_info( &push );
	push.extent[0] = width;
	push.extent[1] = height;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.deferred_gbuffer.pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.deferred_gbuffer.pipeline_layout, 0, 1, &vk.deferred_gbuffer.descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.deferred_gbuffer.pipeline_layout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );

	gx = ( width + 7u ) / 8u;
	gy = ( height + 7u ) / 8u;
	qvkCmdDispatch( vk.cmd->command_buffer, gx, gy, 1 );

	record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_gbuffer_normal, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );
	record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_gbuffer_material, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );

	if ( resume_main ) {
		vk_resume_current_render_pass();
	}
}

static void vk_dgb_create_lighting_descriptor_layout( void )
{
	VkDescriptorSetLayoutBinding bindings[7];
	VkDescriptorSetLayoutCreateInfo desc;

	if ( vk.deferred_gbuffer.lighting_layout != VK_NULL_HANDLE ) {
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
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[5].descriptorCount = 1;
	bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[6].binding = 6;
	bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[6].descriptorCount = 1;
	bindings[6].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desc.bindingCount = 7;
	desc.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &desc, NULL, &vk.deferred_gbuffer.lighting_layout ) );
}

static void vk_dgb_create_lighting_pipeline( void )
{
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.deferred_gbuffer.lighting_pipeline_ready ) {
		return;
	}
	if ( vk.modules.deferred_lighting_cs == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][deferred] deferred_lighting compute shader missing\n" S_COLOR_WHITE );
		return;
	}

	vk_dgb_create_lighting_descriptor_layout();

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_deferred_light_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.deferred_gbuffer.lighting_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.deferred_gbuffer.lighting_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.deferred_lighting_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.deferred_gbuffer.lighting_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.deferred_gbuffer.lighting_pipeline ) );
	SET_OBJECT_NAME( vk.deferred_gbuffer.lighting_pipeline, "pipeline - deferred lighting", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	{
		VkDescriptorPoolSize pool_sizes[3];
		VkDescriptorPoolCreateInfo pool_ci;
		VkDescriptorSetAllocateInfo alloc;

		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		pool_sizes[0].descriptorCount = 2;
		pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[1].descriptorCount = 4;
		pool_sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		pool_sizes[2].descriptorCount = 1;

		Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
		pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_ci.maxSets = 1;
		pool_ci.poolSizeCount = 3;
		pool_ci.pPoolSizes = pool_sizes;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.deferred_gbuffer.lighting_pool ) );

		Com_Memset( &alloc, 0, sizeof( alloc ) );
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.descriptorPool = vk.deferred_gbuffer.lighting_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.deferred_gbuffer.lighting_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.deferred_gbuffer.lighting_descriptor ) );
	}

	vk.deferred_gbuffer.lighting_pipeline_ready = qtrue;
	ri.Printf( PRINT_ALL, "[VK][deferred] lighting compute pipeline ready\n" );
}

static void vk_dgb_update_lighting_descriptors( void )
{
	VkDescriptorBufferInfo buf_infos[2];
	VkDescriptorImageInfo img_infos[5];
	VkWriteDescriptorSet writes[7];
	Vk_Sampler_Def sd;
	VkImageView depth_view;
	int i;

	if ( vk.deferred_gbuffer.lighting_descriptor == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( buf_infos, 0, sizeof( buf_infos ) );
	buf_infos[0].buffer = vk.forward_plus.buffer;
	buf_infos[0].offset = 0;
	buf_infos[0].range = VK_WHOLE_SIZE;
	buf_infos[1].buffer = vk.forward_plus.tile_buffer;
	buf_infos[1].offset = 0;
	buf_infos[1].range = VK_WHOLE_SIZE;

	depth_view = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;

	Com_Memset( img_infos, 0, sizeof( img_infos ) );
	img_infos[0].sampler = vk_find_sampler( &sd );
	img_infos[0].imageView = depth_view;
	img_infos[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	img_infos[1].sampler = vk_find_sampler( &sd );
	img_infos[1].imageView = vk.deferred_gbuffer_albedo_view;
	img_infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_infos[2].sampler = vk_find_sampler( &sd );
	img_infos[2].imageView = vk.deferred_gbuffer_normal_view;
	img_infos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_infos[3].sampler = vk_find_sampler( &sd );
	img_infos[3].imageView = vk.deferred_gbuffer_material_view;
	img_infos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_infos[4].imageView = vk.deferred_lighting_view;
	img_infos[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	for ( i = 0; i < 2; i++ ) {
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = vk.deferred_gbuffer.lighting_descriptor;
		writes[i].dstBinding = (uint32_t)i;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[i].pBufferInfo = &buf_infos[i];
	}
	for ( i = 0; i < 4; i++ ) {
		writes[2 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2 + i].dstSet = vk.deferred_gbuffer.lighting_descriptor;
		writes[2 + i].dstBinding = (uint32_t)( 2 + i );
		writes[2 + i].descriptorCount = 1;
		writes[2 + i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[2 + i].pImageInfo = &img_infos[i];
	}
	writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[6].dstSet = vk.deferred_gbuffer.lighting_descriptor;
	writes[6].dstBinding = 6;
	writes[6].descriptorCount = 1;
	writes[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[6].pImageInfo = &img_infos[4];

	qvkUpdateDescriptorSets( vk.device, 7, writes, 0, NULL );
}

static void vk_dgb_fill_light_push( vk_deferred_light_push_t *push, uint32_t width, uint32_t height )
{
	const float *view = backEnd.viewParms.world.modelViewMatrix;
	const float *proj = backEnd.useFirstPersonProjection ? backEnd.firstPersonProjectionMatrix :
		backEnd.viewParms.projectionMatrix;
	float proj_vk[16];
	float aspect = ( backEnd.viewParms.viewportHeight > 0 ) ?
		( (float)backEnd.viewParms.viewportWidth / (float)backEnd.viewParms.viewportHeight ) : 1.0f;

	Com_Memcpy( push->viewMatrix, view, sizeof( push->viewMatrix ) );
	vk_get_projection_matrix_vk( proj, proj_vk );
	push->projInfo[0] = 1.0f / ( proj_vk[0] * aspect );
	push->projInfo[1] = 1.0f / proj_vk[5];
	push->projInfo[2] = proj_vk[10];
	push->projInfo[3] = proj_vk[14];
	push->extent[0] = width;
	push->extent[1] = height;
	push->tileGrid[0] = vk.forward_plus.tiles_x;
	push->tileGrid[1] = vk.forward_plus.tiles_y;
	push->strength = ( r_deferredLightingStrength && r_deferredLightingStrength->value > 0.0f ) ?
		Com_Clamp( 0.0f, 4.0f, r_deferredLightingStrength->value ) : 1.0f;
	push->maxPerTile = vk.forward_plus.max_per_tile;
	push->numLights = vk.forward_plus.last_packed_count;
	push->additive = vk_deferred_unlit_base_wanted() ? 1u : 0u;
	push->specular = ( r_deferredSpecular && r_deferredSpecular->integer ) ? 1u : 0u;
}

static void vk_dgb_dispatch_lighting_compute( uint32_t width, uint32_t height )
{
	vk_deferred_light_push_t push;
	uint32_t gx, gy;
	VkImageAspectFlags depth_aspect;

	if ( !vk_deferred_lighting_wanted() ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.forward_plus.buffer == VK_NULL_HANDLE || vk.forward_plus.tile_buffer == VK_NULL_HANDLE ||
		vk.deferred_lighting_image == VK_NULL_HANDLE || vk.deferred_lighting_view == VK_NULL_HANDLE ) {
		return;
	}

	vk_dgb_create_lighting_pipeline();
	if ( !vk.deferred_gbuffer.lighting_pipeline_ready || vk.deferred_gbuffer.lighting_pipeline == VK_NULL_HANDLE ) {
		return;
	}

	if ( !vk.deferred_gbuffer.lighting_logged ) {
		ri.Printf( PRINT_ALL,
			"[VK][deferred] r_deferredLighting=1 (%s dynamic; point+spot; strength=%.2f; specular=%s)\n",
			vk_deferred_unlit_base_wanted() ? "additive+sceneBase" : "multiply",
			( r_deferredLightingStrength && r_deferredLightingStrength->value > 0.0f ) ?
				r_deferredLightingStrength->value : 1.0f,
			( r_deferredSpecular && r_deferredSpecular->integer ) ? "on" : "off" );
		vk.deferred_gbuffer.lighting_logged = qtrue;
	}

	depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_lighting_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );

	vk_dgb_update_lighting_descriptors();
	vk_dgb_fill_light_push( &push, width, height );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.deferred_gbuffer.lighting_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.deferred_gbuffer.lighting_pipeline_layout, 0, 1, &vk.deferred_gbuffer.lighting_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.deferred_gbuffer.lighting_pipeline_layout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );

	gx = ( width + 7u ) / 8u;
	gy = ( height + 7u ) / 8u;
	qvkCmdDispatch( vk.cmd->command_buffer, gx, gy, 1 );

	record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_lighting_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );
}

static void vk_dgb_create_composite_gfx_pipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[2];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input;
	VkPipelineInputAssemblyStateCreateInfo input_assembly;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineRasterizationStateCreateInfo rasterization;
	VkPipelineMultisampleStateCreateInfo multisample;
	VkPipelineColorBlendAttachmentState blend_att;
	VkPipelineColorBlendStateCreateInfo blend;
	VkPipelineDepthStencilStateCreateInfo depth_stencil;
	VkGraphicsPipelineCreateInfo pipe_ci;
	VkDescriptorPoolSize pool_size;
	VkDescriptorPoolCreateInfo pool_ci;
	VkDescriptorSetAllocateInfo alloc;

	if ( vk.deferred_gbuffer.composite_gfx_ready ) {
		return;
	}
	if ( vk.modules.deferred_lighting_composite_fs == VK_NULL_HANDLE || vk.modules.gamma_vs == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.render_pass.post_bloom == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 2;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.deferred_gbuffer.composite_gfx_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_deferred_composite_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.deferred_gbuffer.composite_gfx_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.deferred_gbuffer.composite_gfx_pipeline_layout ) );

	Com_Memset( stages, 0, sizeof( stages ) );
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vk.modules.gamma_vs;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = vk.modules.deferred_lighting_composite_fs;
	stages[1].pName = "main";

	Com_Memset( &vertex_input, 0, sizeof( vertex_input ) );
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	Com_Memset( &input_assembly, 0, sizeof( input_assembly ) );
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	Com_Memset( &viewport_state, 0, sizeof( viewport_state ) );
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;

	Com_Memset( &rasterization, 0, sizeof( rasterization ) );
	rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization.cullMode = VK_CULL_MODE_NONE;
	rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterization.lineWidth = 1.0f;

	Com_Memset( &multisample, 0, sizeof( multisample ) );
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	Com_Memset( &blend_att, 0, sizeof( blend_att ) );
	blend_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	Com_Memset( &blend, 0, sizeof( blend ) );
	blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.attachmentCount = 1;
	blend.pAttachments = &blend_att;

	Com_Memset( &depth_stencil, 0, sizeof( depth_stencil ) );
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = VK_FALSE;
	depth_stencil.depthWriteEnable = VK_FALSE;

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipe_ci.stageCount = 2;
	pipe_ci.pStages = stages;
	pipe_ci.pVertexInputState = &vertex_input;
	pipe_ci.pInputAssemblyState = &input_assembly;
	pipe_ci.pViewportState = &viewport_state;
	pipe_ci.pRasterizationState = &rasterization;
	pipe_ci.pMultisampleState = &multisample;
	pipe_ci.pDepthStencilState = &depth_stencil;
	pipe_ci.pColorBlendState = &blend;
	pipe_ci.layout = vk.deferred_gbuffer.composite_gfx_pipeline_layout;
	pipe_ci.renderPass = vk.render_pass.post_bloom;
	pipe_ci.subpass = 0;
	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.deferred_gbuffer.composite_gfx_pipeline ) );

	pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_size.descriptorCount = 2;
	Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
	pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_ci.maxSets = 1;
	pool_ci.poolSizeCount = 1;
	pool_ci.pPoolSizes = &pool_size;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.deferred_gbuffer.composite_gfx_pool ) );

	Com_Memset( &alloc, 0, sizeof( alloc ) );
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = vk.deferred_gbuffer.composite_gfx_pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &vk.deferred_gbuffer.composite_gfx_layout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.deferred_gbuffer.composite_gfx_descriptor ) );

	vk.deferred_gbuffer.composite_gfx_ready = qtrue;
}

static void vk_dgb_update_composite_descriptor( void )
{
	VkDescriptorImageInfo img_infos[2];
	VkWriteDescriptorSet writes[2];
	Vk_Sampler_Def sd;
	int i;

	if ( vk.deferred_gbuffer.composite_gfx_descriptor == VK_NULL_HANDLE ||
		vk.deferred_lighting_view == VK_NULL_HANDLE || vk.deferred_gbuffer_albedo_view == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;

	Com_Memset( img_infos, 0, sizeof( img_infos ) );
	img_infos[0].sampler = vk_find_sampler( &sd );
	img_infos[0].imageView = vk.deferred_gbuffer_albedo_view;
	img_infos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_infos[1].sampler = vk_find_sampler( &sd );
	img_infos[1].imageView = vk.deferred_lighting_view;
	img_infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	for ( i = 0; i < 2; i++ ) {
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = vk.deferred_gbuffer.composite_gfx_descriptor;
		writes[i].dstBinding = (uint32_t)i;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[i].pImageInfo = &img_infos[i];
	}
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );
}

static void vk_dgb_composite_lit_to_color( uint32_t width, uint32_t height )
{
	vk_deferred_composite_push_t push;
	qboolean resume_main = ( vk.inRenderPass && vk.renderPassIndex == RENDER_PASS_MAIN ) ? qtrue : qfalse;

	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		0, 0 );

	vk_begin_post_bloom_render_pass();
	vk_dgb_update_composite_descriptor();

	push.additive = vk_deferred_unlit_base_wanted() ? 1u : 0u;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.deferred_gbuffer.composite_gfx_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.deferred_gbuffer.composite_gfx_pipeline_layout, 0, 1, &vk.deferred_gbuffer.composite_gfx_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.deferred_gbuffer.composite_gfx_pipeline_layout,
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );
	vk_set_fullscreen_viewport_scissor( width, height );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	if ( resume_main ) {
		vk_resume_current_render_pass();
	} else if ( vk.color_image_view != VK_NULL_HANDLE ) {
		vk_barrier_post_fog_source_for_sampling( vk.color_image_view, "deferred lighting composite" );
	}
}

void vk_deferred_lighting_apply_after_geometry( void )
{
	uint32_t width, height;

	if ( !vk_deferred_lighting_wanted() ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}

	width = vk_get_render_target_width();
	height = vk_get_render_target_height();
	if ( width == 0 || height == 0 ) {
		return;
	}

	vk_dgb_dispatch_lighting_compute( width, height );

	vk_dgb_create_composite_gfx_pipeline();
	if ( !vk.deferred_gbuffer.composite_gfx_ready || vk.deferred_gbuffer.composite_gfx_pipeline == VK_NULL_HANDLE ) {
		return;
	}

	if ( !vk.deferred_gbuffer.composite_logged ) {
		ri.Printf( PRINT_ALL,
			"[VK][deferred] composite scene base + dynamic lighting to color (additive=%s)\n",
			vk_deferred_unlit_base_wanted() ? "1" : "0" );
		vk.deferred_gbuffer.composite_logged = qtrue;
	}

	vk_dgb_composite_lit_to_color( width, height );
}

static void vk_dgb_create_debug_gfx_pipeline( void )
{
	VkDescriptorSetLayoutBinding binding;
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input;
	VkPipelineInputAssemblyStateCreateInfo input_assembly;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineRasterizationStateCreateInfo rasterization;
	VkPipelineMultisampleStateCreateInfo multisample;
	VkPipelineColorBlendAttachmentState blend_att;
	VkPipelineColorBlendStateCreateInfo blend;
	VkPipelineDepthStencilStateCreateInfo depth_stencil;
	VkGraphicsPipelineCreateInfo pipe_ci;
	VkDescriptorPoolSize pool_size;
	VkDescriptorPoolCreateInfo pool_ci;
	VkDescriptorSetAllocateInfo alloc;

	if ( vk.deferred_gbuffer.debug_gfx_ready ) {
		return;
	}
	if ( vk.modules.deferred_gbuffer_debug_fs == VK_NULL_HANDLE || vk.modules.gamma_vs == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.render_pass.post_bloom == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &binding, 0, sizeof( binding ) );
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 1;
	layout_ci.pBindings = &binding;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.deferred_gbuffer.debug_gfx_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_deferred_gbuf_debug_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.deferred_gbuffer.debug_gfx_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.deferred_gbuffer.debug_gfx_pipeline_layout ) );

	Com_Memset( stages, 0, sizeof( stages ) );
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vk.modules.gamma_vs;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = vk.modules.deferred_gbuffer_debug_fs;
	stages[1].pName = "main";

	Com_Memset( &vertex_input, 0, sizeof( vertex_input ) );
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	Com_Memset( &input_assembly, 0, sizeof( input_assembly ) );
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	Com_Memset( &viewport_state, 0, sizeof( viewport_state ) );
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;

	Com_Memset( &rasterization, 0, sizeof( rasterization ) );
	rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization.cullMode = VK_CULL_MODE_NONE;
	rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterization.lineWidth = 1.0f;

	Com_Memset( &multisample, 0, sizeof( multisample ) );
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	Com_Memset( &blend_att, 0, sizeof( blend_att ) );
	blend_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	Com_Memset( &blend, 0, sizeof( blend ) );
	blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.attachmentCount = 1;
	blend.pAttachments = &blend_att;

	Com_Memset( &depth_stencil, 0, sizeof( depth_stencil ) );
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = VK_FALSE;
	depth_stencil.depthWriteEnable = VK_FALSE;

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipe_ci.stageCount = 2;
	pipe_ci.pStages = stages;
	pipe_ci.pVertexInputState = &vertex_input;
	pipe_ci.pInputAssemblyState = &input_assembly;
	pipe_ci.pViewportState = &viewport_state;
	pipe_ci.pRasterizationState = &rasterization;
	pipe_ci.pMultisampleState = &multisample;
	pipe_ci.pDepthStencilState = &depth_stencil;
	pipe_ci.pColorBlendState = &blend;
	pipe_ci.layout = vk.deferred_gbuffer.debug_gfx_pipeline_layout;
	pipe_ci.renderPass = vk.render_pass.post_bloom;
	pipe_ci.subpass = 0;
	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.deferred_gbuffer.debug_gfx_pipeline ) );

	pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_size.descriptorCount = 1;
	Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
	pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_ci.maxSets = 1;
	pool_ci.poolSizeCount = 1;
	pool_ci.pPoolSizes = &pool_size;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.deferred_gbuffer.debug_gfx_pool ) );

	Com_Memset( &alloc, 0, sizeof( alloc ) );
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = vk.deferred_gbuffer.debug_gfx_pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &vk.deferred_gbuffer.debug_gfx_layout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.deferred_gbuffer.debug_gfx_descriptor ) );

	vk.deferred_gbuffer.debug_gfx_ready = qtrue;
}

static void vk_dgb_update_debug_descriptor( VkImageView view )
{
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet write;
	Vk_Sampler_Def sd;

	if ( vk.deferred_gbuffer.debug_gfx_descriptor == VK_NULL_HANDLE || view == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;

	Com_Memset( &info, 0, sizeof( info ) );
	info.sampler = vk_find_sampler( &sd );
	info.imageView = view;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = vk.deferred_gbuffer.debug_gfx_descriptor;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &info;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
}

qboolean vk_deferred_gbuffer_draw_debug( void )
{
	vk_deferred_gbuf_debug_push_t push;
	VkImageView src_view;
	int mode;
	uint32_t width;
	uint32_t height;
	static qboolean s_debug_logged;

	if ( !vk_deferred_gbuffer_active() || !r_deferredGBufferFill || !r_deferredGBufferFill->integer ) {
		return qfalse;
	}
	if ( !r_deferredGBufferDebug || r_deferredGBufferDebug->integer <= 0 ) {
		return qfalse;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !backEnd.doneSurfaces || !vk.fboActive ) {
		return qfalse;
	}

	mode = r_deferredGBufferDebug->integer;
	if ( mode < 1 ) {
		mode = 1;
	}
	if ( mode > 6 ) {
		mode = 6;
	}

	if ( mode == 1 ) {
		src_view = vk.deferred_gbuffer_albedo_view;
	} else if ( mode == 2 || mode == 5 ) {
		src_view = vk.deferred_gbuffer_normal_view;
	} else if ( mode == 3 ) {
		src_view = vk.deferred_gbuffer_material_view;
	} else if ( mode == 6 ) {
		src_view = vk.motion_vector_view;
	} else {
		src_view = vk.deferred_lighting_view;
	}
	if ( src_view == VK_NULL_HANDLE || vk.color_image == VK_NULL_HANDLE ) {
		return qfalse;
	}

	vk_dgb_create_debug_gfx_pipeline();
	if ( !vk.deferred_gbuffer.debug_gfx_ready || vk.deferred_gbuffer.debug_gfx_pipeline == VK_NULL_HANDLE ) {
		return qfalse;
	}

	if ( !s_debug_logged ) {
		ri.Printf( PRINT_ALL, "[VK][deferred] r_deferredGBufferDebug=%d (1=albedo 2=normal 3=material 4=lighting 5=normal confidence 6=motion)\n", mode );
		s_debug_logged = qtrue;
	}

	width = vk_get_render_target_width();
	height = vk_get_render_target_height();
	if ( width == 0 || height == 0 ) {
		return qfalse;
	}

	vk_end_render_pass();

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		0, 0 );

	vk_begin_post_bloom_render_pass();
	vk_dgb_update_debug_descriptor( src_view );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.deferred_gbuffer.debug_gfx_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.deferred_gbuffer.debug_gfx_pipeline_layout, 0, 1, &vk.deferred_gbuffer.debug_gfx_descriptor, 0, NULL );

	push.mode = mode;
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.deferred_gbuffer.debug_gfx_pipeline_layout,
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );

	vk_set_fullscreen_viewport_scissor( width, height );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	if ( vk.color_image_view != VK_NULL_HANDLE ) {
		vk_barrier_post_fog_source_for_sampling( vk.color_image_view, "deferred gbuffer debug" );
	}

	return qtrue;
}
