/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Visibility-buffer sidecar (r_visibilityBuffer) + material classification.
Phase 1 of the 2027 hybrid visibility renderer — see docs/RENDERER_2027.md.
Coexists with classic G-buffer; does not replace deferred lighting consumers.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_visibility_buffer.h"
#include "vk_deferred_gbuffer.h"
#include "vk_image_layout.h"
#include "vk_render_pass.h"
#include "vk_scene_pass.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include "vk_post_fog.h"

static void vk_visbuf_validate_compute_break( const char *stage, qboolean resume_main )
{
	if ( !r_fboDebug || r_fboDebug->integer < 1 || !vk_post_fog_fbo_debug_throttle() ) {
		return;
	}

	if ( vk.inRenderPass ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][visbuf] %s: expected out-of-pass compute window, still in render pass %d\n",
			stage ? stage : "compute_break", (int)vk.renderPassIndex );
	}

	if ( resume_main && vk.renderPassIndex != RENDER_PASS_MAIN ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][visbuf] %s: resume_main requested but renderPassIndex=%d instead of main\n",
			stage ? stage : "compute_break", (int)vk.renderPassIndex );
	}

	if ( vk.cmd == NULL || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][visbuf] %s: command buffer unavailable during visibility compute break\n",
			stage ? stage : "compute_break" );
	}
}

typedef struct {
	uint32_t extent[2];
	uint32_t tileSize;
	uint32_t reserved;
} vk_visbuf_fill_push_t;

typedef struct {
	uint32_t extent[2];
	uint32_t hasMaterial;
	uint32_t reserved;
} vk_visbuf_classify_push_t;

typedef struct {
	int mode;
} vk_visbuf_debug_push_t;

static void vk_visbuf_create_fill_pipeline( void );
static void vk_visbuf_create_classify_pipeline( void );
static void vk_visbuf_create_debug_gfx_pipeline( void );

qboolean vk_visibility_buffer_active( void )
{
	return ( vk.visibilityBufferAllocated && r_renderMode &&
		( r_renderMode->integer == 1 || r_renderMode->integer == 2 || r_renderMode->integer == 3 ) &&
		r_visibilityBuffer && r_visibilityBuffer->integer ) ? qtrue : qfalse;
}

qboolean vk_visibility_buffer_fill_wanted( void )
{
	return ( vk_visibility_buffer_active() && r_visibilityBufferFill &&
		r_visibilityBufferFill->integer ) ? qtrue : qfalse;
}

qboolean vk_material_classify_wanted( void )
{
	/* Classify needs the class RT (visibility buffer latch), not Morton fill. */
	return ( vk_visibility_buffer_active() && r_materialClassify &&
		r_materialClassify->integer ) ? qtrue : qfalse;
}

static void vk_visbuf_destroy_fill_pipeline( void )
{
	if ( vk.visibility_buffer.pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.visibility_buffer.pipeline, NULL );
		vk.visibility_buffer.pipeline = VK_NULL_HANDLE;
	}
	if ( vk.visibility_buffer.pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.visibility_buffer.pipeline_layout, NULL );
		vk.visibility_buffer.pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.visibility_buffer.pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.visibility_buffer.pool, NULL );
		vk.visibility_buffer.pool = VK_NULL_HANDLE;
		vk.visibility_buffer.descriptor = VK_NULL_HANDLE;
	}
	if ( vk.visibility_buffer.layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.visibility_buffer.layout, NULL );
		vk.visibility_buffer.layout = VK_NULL_HANDLE;
	}
	vk.visibility_buffer.pipeline_ready = qfalse;
}

static void vk_visbuf_destroy_classify_pipeline( void )
{
	if ( vk.visibility_buffer.classify_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.visibility_buffer.classify_pipeline, NULL );
		vk.visibility_buffer.classify_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.visibility_buffer.classify_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.visibility_buffer.classify_pipeline_layout, NULL );
		vk.visibility_buffer.classify_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.visibility_buffer.classify_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.visibility_buffer.classify_pool, NULL );
		vk.visibility_buffer.classify_pool = VK_NULL_HANDLE;
		vk.visibility_buffer.classify_descriptor = VK_NULL_HANDLE;
	}
	if ( vk.visibility_buffer.classify_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.visibility_buffer.classify_layout, NULL );
		vk.visibility_buffer.classify_layout = VK_NULL_HANDLE;
	}
	vk.visibility_buffer.classify_pipeline_ready = qfalse;
}

static void vk_visbuf_destroy_debug_gfx_pipeline( void )
{
	if ( vk.visibility_buffer.debug_gfx_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.visibility_buffer.debug_gfx_pipeline, NULL );
		vk.visibility_buffer.debug_gfx_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.visibility_buffer.debug_gfx_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.visibility_buffer.debug_gfx_pipeline_layout, NULL );
		vk.visibility_buffer.debug_gfx_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.visibility_buffer.debug_gfx_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.visibility_buffer.debug_gfx_pool, NULL );
		vk.visibility_buffer.debug_gfx_pool = VK_NULL_HANDLE;
		vk.visibility_buffer.debug_gfx_descriptor = VK_NULL_HANDLE;
	}
	if ( vk.visibility_buffer.debug_gfx_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.visibility_buffer.debug_gfx_layout, NULL );
		vk.visibility_buffer.debug_gfx_layout = VK_NULL_HANDLE;
	}
	vk.visibility_buffer.debug_gfx_ready = qfalse;
}

void vk_visibility_buffer_init( void )
{
	Com_Memset( &vk.visibility_buffer, 0, sizeof( vk.visibility_buffer ) );
}

void vk_visibility_buffer_shutdown( void )
{
	vk_visbuf_destroy_fill_pipeline();
	vk_visbuf_destroy_classify_pipeline();
	vk_visbuf_destroy_debug_gfx_pipeline();
}

void vk_visibility_buffer_ensure_runtime( void )
{
	if ( !vk_visibility_buffer_active() || !vk.device || vk.device_lost ) {
		return;
	}

	if ( vk.visibility_buffer.layout == VK_NULL_HANDLE ||
		vk.visibility_buffer.pool == VK_NULL_HANDLE ||
		vk.visibility_buffer.descriptor == VK_NULL_HANDLE ||
		!vk.visibility_buffer.pipeline_ready ||
		vk.visibility_buffer.pipeline == VK_NULL_HANDLE ) {
		vk_visbuf_create_fill_pipeline();
	}

	if ( vk_material_classify_wanted() ) {
		if ( vk.visibility_buffer.classify_layout == VK_NULL_HANDLE ||
			vk.visibility_buffer.classify_pool == VK_NULL_HANDLE ||
			vk.visibility_buffer.classify_descriptor == VK_NULL_HANDLE ||
			!vk.visibility_buffer.classify_pipeline_ready ||
			vk.visibility_buffer.classify_pipeline == VK_NULL_HANDLE ) {
			vk_visbuf_create_classify_pipeline();
		}
	}

	if ( r_visibilityBufferDebug && r_visibilityBufferDebug->integer > 0 &&
		( vk.visibility_buffer.debug_gfx_layout == VK_NULL_HANDLE ||
		  vk.visibility_buffer.debug_gfx_pool == VK_NULL_HANDLE ||
		  vk.visibility_buffer.debug_gfx_descriptor == VK_NULL_HANDLE ||
		  !vk.visibility_buffer.debug_gfx_ready ||
		  vk.visibility_buffer.debug_gfx_pipeline == VK_NULL_HANDLE ) ) {
		vk_visbuf_create_debug_gfx_pipeline();
	}
}

static void vk_visbuf_create_fill_pipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[3];
	VkDescriptorSetLayoutCreateInfo desc;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;
	VkDescriptorPoolSize pool_sizes[2];
	VkDescriptorPoolCreateInfo pool_ci;
	VkDescriptorSetAllocateInfo alloc;

	if ( vk.visibility_buffer.pipeline_ready ) {
		return;
	}
	if ( vk.modules.visibility_buffer_fill_cs == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][visbuf] visibility_buffer_fill compute shader missing\n" S_COLOR_WHITE );
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
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &desc, NULL, &vk.visibility_buffer.layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_visbuf_fill_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.visibility_buffer.layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.visibility_buffer.pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.visibility_buffer_fill_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.visibility_buffer.pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL,
		&vk.visibility_buffer.pipeline ) );
	SET_OBJECT_NAME( vk.visibility_buffer.pipeline, "pipeline - visibility buffer fill",
		VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[0].descriptorCount = 1;
	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	pool_sizes[1].descriptorCount = 2;

	Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
	pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_ci.maxSets = 1;
	pool_ci.poolSizeCount = 2;
	pool_ci.pPoolSizes = pool_sizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.visibility_buffer.pool ) );

	Com_Memset( &alloc, 0, sizeof( alloc ) );
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = vk.visibility_buffer.pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &vk.visibility_buffer.layout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.visibility_buffer.descriptor ) );

	vk.visibility_buffer.pipeline_ready = qtrue;
	ri.Printf( PRINT_ALL, "[VK][visbuf] visibility fill compute pipeline ready\n" );
}

static void vk_visbuf_create_classify_pipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[3];
	VkDescriptorSetLayoutCreateInfo desc;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;
	VkDescriptorPoolSize pool_sizes[2];
	VkDescriptorPoolCreateInfo pool_ci;
	VkDescriptorSetAllocateInfo alloc;

	if ( vk.visibility_buffer.classify_pipeline_ready ) {
		return;
	}
	if ( vk.modules.material_classify_cs == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][visbuf] material_classify compute shader missing\n" S_COLOR_WHITE );
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

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desc.bindingCount = 3;
	desc.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &desc, NULL, &vk.visibility_buffer.classify_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_visbuf_classify_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.visibility_buffer.classify_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL,
		&vk.visibility_buffer.classify_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.material_classify_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.visibility_buffer.classify_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL,
		&vk.visibility_buffer.classify_pipeline ) );
	SET_OBJECT_NAME( vk.visibility_buffer.classify_pipeline, "pipeline - material classify",
		VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[0].descriptorCount = 2;
	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	pool_sizes[1].descriptorCount = 1;

	Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
	pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_ci.maxSets = 1;
	pool_ci.poolSizeCount = 2;
	pool_ci.pPoolSizes = pool_sizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.visibility_buffer.classify_pool ) );

	Com_Memset( &alloc, 0, sizeof( alloc ) );
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = vk.visibility_buffer.classify_pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &vk.visibility_buffer.classify_layout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.visibility_buffer.classify_descriptor ) );

	vk.visibility_buffer.classify_pipeline_ready = qtrue;
	ri.Printf( PRINT_ALL, "[VK][visbuf] material classify compute pipeline ready\n" );
}

static VkImageView vk_visbuf_depth_sample_view( void )
{
	if ( vk.depth_image_view_sample != VK_NULL_HANDLE ) {
		return vk.depth_image_view_sample;
	}
	return vk.depth_image_view;
}

static void vk_visbuf_update_fill_descriptors( void )
{
	VkDescriptorImageInfo depth_info;
	VkDescriptorImageInfo vis_info;
	VkDescriptorImageInfo bary_info;
	VkWriteDescriptorSet writes[3];
	Vk_Sampler_Def depth_sd;

	if ( vk.visibility_buffer.descriptor == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &depth_sd, 0, sizeof( depth_sd ) );
	depth_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	Com_Memset( &depth_info, 0, sizeof( depth_info ) );
	depth_info.sampler = vk_find_sampler( &depth_sd );
	depth_info.imageView = vk_visbuf_depth_sample_view();
	depth_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	Com_Memset( &vis_info, 0, sizeof( vis_info ) );
	vis_info.imageView = vk.visibility_buffer_ids_view;
	vis_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( &bary_info, 0, sizeof( bary_info ) );
	bary_info.imageView = vk.visibility_buffer_bary_view;
	bary_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.visibility_buffer.descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &depth_info;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vk.visibility_buffer.descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[1].pImageInfo = &vis_info;
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = vk.visibility_buffer.descriptor;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[2].pImageInfo = &bary_info;
	qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );
}

static void vk_visbuf_update_classify_descriptors( void )
{
	VkDescriptorImageInfo depth_info;
	VkDescriptorImageInfo mat_info;
	VkDescriptorImageInfo class_info;
	VkWriteDescriptorSet writes[3];
	Vk_Sampler_Def sd;

	if ( vk.visibility_buffer.classify_descriptor == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	Com_Memset( &depth_info, 0, sizeof( depth_info ) );
	depth_info.sampler = vk_find_sampler( &sd );
	depth_info.imageView = vk_visbuf_depth_sample_view();
	depth_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	Com_Memset( &mat_info, 0, sizeof( mat_info ) );
	mat_info.sampler = vk_find_sampler( &sd );
	mat_info.imageView = vk.deferred_gbuffer_material_view != VK_NULL_HANDLE ?
		vk.deferred_gbuffer_material_view : vk.color_image_view;
	mat_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &class_info, 0, sizeof( class_info ) );
	class_info.imageView = vk.visibility_buffer_class_view;
	class_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.visibility_buffer.classify_descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &depth_info;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vk.visibility_buffer.classify_descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &mat_info;
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = vk.visibility_buffer.classify_descriptor;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[2].pImageInfo = &class_info;
	qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );
}

void vk_visibility_buffer_capture_after_geometry( void )
{
	VkImageAspectFlags depth_aspect;
	uint32_t width, height;
	vk_visbuf_fill_push_t fill_push;
	vk_visbuf_classify_push_t class_push;
	uint32_t gx, gy;
	qboolean resume_main;

	if ( !vk_visibility_buffer_fill_wanted() ) {
		return;
	}
	vk_visibility_buffer_ensure_runtime();
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.visibility_buffer_ids == VK_NULL_HANDLE || vk.visibility_buffer_bary == VK_NULL_HANDLE ||
		vk.depth_image == VK_NULL_HANDLE ) {
		return;
	}

	resume_main = ( vk.inRenderPass && vk.renderPassIndex == RENDER_PASS_MAIN ) ? qtrue : qfalse;
	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}
	vk_visbuf_validate_compute_break( "visibility_buffer_capture_after_geometry", resume_main );

	vk_visbuf_create_fill_pipeline();
	if ( !vk.visibility_buffer.pipeline_ready || vk.visibility_buffer.pipeline == VK_NULL_HANDLE ) {
		if ( resume_main ) {
			vk_resume_current_render_pass();
		}
		return;
	}

	if ( !vk.visibility_buffer.fill_logged ) {
		ri.Printf( PRINT_ALL,
			"[VK][visbuf] r_visibilityBufferFill=1 (depth-derived draw/prim id + bary proxy)\n" );
		vk.visibility_buffer.fill_logged = qtrue;
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
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	record_image_layout_transition( vk.cmd->command_buffer, vk.visibility_buffer_ids,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );
	record_image_layout_transition( vk.cmd->command_buffer, vk.visibility_buffer_bary,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );

	vk_visbuf_update_fill_descriptors();

	Com_Memset( &fill_push, 0, sizeof( fill_push ) );
	fill_push.extent[0] = width;
	fill_push.extent[1] = height;
	fill_push.tileSize = 16u;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.visibility_buffer.pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.visibility_buffer.pipeline_layout, 0, 1, &vk.visibility_buffer.descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.visibility_buffer.pipeline_layout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( fill_push ), &fill_push );

	gx = ( width + 7u ) / 8u;
	gy = ( height + 7u ) / 8u;
	qvkCmdDispatch( vk.cmd->command_buffer, gx, gy, 1 );

	record_image_layout_transition( vk.cmd->command_buffer, vk.visibility_buffer_ids,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	record_image_layout_transition( vk.cmd->command_buffer, vk.visibility_buffer_bary,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

	if ( vk_material_classify_wanted() && vk.visibility_buffer_class != VK_NULL_HANDLE ) {
		vk_visbuf_create_classify_pipeline();
		if ( vk.visibility_buffer.classify_pipeline_ready &&
			vk.visibility_buffer.classify_pipeline != VK_NULL_HANDLE ) {
			if ( !vk.visibility_buffer.classify_logged ) {
				ri.Printf( PRINT_ALL,
					"[VK][visbuf] r_materialClassify=1 (class map from G-buffer material + depth)\n" );
				vk.visibility_buffer.classify_logged = qtrue;
			}

			record_image_layout_transition( vk.cmd->command_buffer, vk.visibility_buffer_class,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );

			vk_visbuf_update_classify_descriptors();

			Com_Memset( &class_push, 0, sizeof( class_push ) );
			class_push.extent[0] = width;
			class_push.extent[1] = height;
			class_push.hasMaterial = ( vk.deferred_gbuffer_material_view != VK_NULL_HANDLE &&
				vk_deferred_gbuffer_fill_wanted() ) ? 1u : 0u;

			qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
				vk.visibility_buffer.classify_pipeline );
			qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
				vk.visibility_buffer.classify_pipeline_layout, 0, 1,
				&vk.visibility_buffer.classify_descriptor, 0, NULL );
			qvkCmdPushConstants( vk.cmd->command_buffer, vk.visibility_buffer.classify_pipeline_layout,
				VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( class_push ), &class_push );
			qvkCmdDispatch( vk.cmd->command_buffer, gx, gy, 1 );

			record_image_layout_transition( vk.cmd->command_buffer, vk.visibility_buffer_class,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
		}
	}

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );

	if ( resume_main ) {
		vk_resume_current_render_pass();
	}
}

static void vk_visbuf_create_debug_gfx_pipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[4];
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
	VkPipelineDynamicStateCreateInfo dynamic_state;
	VkDynamicState dynamic_states[2];
	VkGraphicsPipelineCreateInfo pipe_ci;
	VkDescriptorPoolSize pool_sizes[2];
	VkDescriptorPoolCreateInfo pool_ci;
	VkDescriptorSetAllocateInfo alloc;
	int i;

	if ( vk.visibility_buffer.debug_gfx_ready ) {
		return;
	}
	if ( vk.modules.visibility_buffer_debug_fs == VK_NULL_HANDLE || vk.modules.gamma_vs == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.render_pass.post_bloom == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	for ( i = 0; i < 4; i++ ) {
		bindings[i].binding = (uint32_t)i;
		bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[i].descriptorCount = 1;
		bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	}

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 4;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL,
		&vk.visibility_buffer.debug_gfx_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_visbuf_debug_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.visibility_buffer.debug_gfx_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL,
		&vk.visibility_buffer.debug_gfx_pipeline_layout ) );

	Com_Memset( stages, 0, sizeof( stages ) );
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vk.modules.gamma_vs;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = vk.modules.visibility_buffer_debug_fs;
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

	dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
	Com_Memset( &dynamic_state, 0, sizeof( dynamic_state ) );
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = 2;
	dynamic_state.pDynamicStates = dynamic_states;

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
	pipe_ci.pDynamicState = &dynamic_state;
	pipe_ci.layout = vk.visibility_buffer.debug_gfx_pipeline_layout;
	pipe_ci.renderPass = vk.render_pass.post_bloom;
	pipe_ci.subpass = 0;
	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL,
		&vk.visibility_buffer.debug_gfx_pipeline ) );
	SET_OBJECT_NAME( vk.visibility_buffer.debug_gfx_pipeline, "pipeline - visibility buffer debug",
		VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[0].descriptorCount = 4;
	Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
	pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_ci.maxSets = 1;
	pool_ci.poolSizeCount = 1;
	pool_ci.pPoolSizes = pool_sizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.visibility_buffer.debug_gfx_pool ) );

	Com_Memset( &alloc, 0, sizeof( alloc ) );
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = vk.visibility_buffer.debug_gfx_pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &vk.visibility_buffer.debug_gfx_layout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.visibility_buffer.debug_gfx_descriptor ) );

	vk.visibility_buffer.debug_gfx_ready = qtrue;
}

static void vk_visbuf_update_debug_descriptors( void )
{
	VkDescriptorImageInfo infos[4];
	VkWriteDescriptorSet writes[4];
	Vk_Sampler_Def sd;
	int i;
	VkImageView albedo_view;

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	albedo_view = vk.deferred_gbuffer_albedo_view != VK_NULL_HANDLE ?
		vk.deferred_gbuffer_albedo_view : vk.color_image_view;

	Com_Memset( infos, 0, sizeof( infos ) );
	infos[0].sampler = vk_find_sampler( &sd );
	infos[0].imageView = vk.visibility_buffer_ids_view;
	infos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	infos[1].sampler = vk_find_sampler( &sd );
	infos[1].imageView = vk.visibility_buffer_bary_view;
	infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	infos[2].sampler = vk_find_sampler( &sd );
	infos[2].imageView = vk.visibility_buffer_class_view != VK_NULL_HANDLE ?
		vk.visibility_buffer_class_view : vk.visibility_buffer_ids_view;
	infos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	infos[3].sampler = vk_find_sampler( &sd );
	infos[3].imageView = albedo_view != VK_NULL_HANDLE ? albedo_view : vk.visibility_buffer_bary_view;
	infos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	for ( i = 0; i < 4; i++ ) {
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = vk.visibility_buffer.debug_gfx_descriptor;
		writes[i].dstBinding = (uint32_t)i;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[i].pImageInfo = &infos[i];
	}
	qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );
}

qboolean vk_visibility_buffer_draw_debug( void )
{
	vk_visbuf_debug_push_t push;
	int mode;
	uint32_t width, height;
	static qboolean s_debug_logged;

	if ( !vk_visibility_buffer_fill_wanted() ) {
		return qfalse;
	}
	vk_visibility_buffer_ensure_runtime();
	if ( !r_visibilityBufferDebug || r_visibilityBufferDebug->integer <= 0 ) {
		return qfalse;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !backEnd.doneSurfaces || !vk.fboActive ) {
		return qfalse;
	}

	mode = r_visibilityBufferDebug->integer;
	if ( mode < 1 ) {
		mode = 1;
	}
	if ( mode > 5 ) {
		mode = 5;
	}

	vk_visbuf_create_debug_gfx_pipeline();
	if ( !vk.visibility_buffer.debug_gfx_ready ||
		vk.visibility_buffer.debug_gfx_pipeline == VK_NULL_HANDLE ) {
		return qfalse;
	}

	if ( !s_debug_logged ) {
		ri.Printf( PRINT_ALL,
			"[VK][visbuf] r_visibilityBufferDebug=%d (1=drawId 2=primId 3=bary 4=class 5=lateShade)\n", mode );
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

	vk_visbuf_update_debug_descriptors();
	vk_begin_post_bloom_render_pass();
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.visibility_buffer.debug_gfx_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.visibility_buffer.debug_gfx_pipeline_layout, 0, 1,
		&vk.visibility_buffer.debug_gfx_descriptor, 0, NULL );

	Com_Memset( &push, 0, sizeof( push ) );
	push.mode = mode;
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.visibility_buffer.debug_gfx_pipeline_layout,
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );

	vk_set_fullscreen_viewport_scissor( width, height );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	if ( vk.color_image_view != VK_NULL_HANDLE ) {
		vk_barrier_post_fog_source_for_sampling( vk.color_image_view, "visibility buffer debug" );
	}
	return qtrue;
}

void vk_visibility_buffer_status_f( void )
{
	ri.Printf( PRINT_ALL, "==== visibility buffer (2027 Phase 1 / 1.5) ====\n" );
	ri.Printf( PRINT_ALL, "cvar      : r_visibilityBuffer=%d fill=%d debug=%d classify=%d deferredClassify=%d\n",
		r_visibilityBuffer ? r_visibilityBuffer->integer : 0,
		r_visibilityBufferFill ? r_visibilityBufferFill->integer : 0,
		r_visibilityBufferDebug ? r_visibilityBufferDebug->integer : 0,
		r_materialClassify ? r_materialClassify->integer : 0,
		r_deferredMaterialClassify ? r_deferredMaterialClassify->integer : 0 );
	ri.Printf( PRINT_ALL, "active    : allocated=%s fillWanted=%s classifyWanted=%s mode=%d\n",
		vk.visibilityBufferAllocated ? "yes" : "no",
		vk_visibility_buffer_fill_wanted() ? "yes" : "no",
		vk_material_classify_wanted() ? "yes" : "no",
		r_renderMode ? r_renderMode->integer : -1 );
	ri.Printf( PRINT_ALL, "encoding  : depth_proxy (Morton+depth bucket; true gl_PrimitiveID MRT follow-up)\n" );
	ri.Printf( PRINT_ALL, "consumers : ids/bary=debug(+lateShade5) class=deferred_when_classify deferredClassify=%s\n",
		( r_deferredMaterialClassify && r_deferredMaterialClassify->integer ) ? "on" : "off" );
	ri.Printf( PRINT_ALL, "images    : ids=%s bary=%s class=%s\n",
		vk.visibility_buffer_ids ? "yes" : "no",
		vk.visibility_buffer_bary ? "yes" : "no",
		vk.visibility_buffer_class ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "pipelines : fill=%s classify=%s debug=%s\n",
		vk.visibility_buffer.pipeline_ready ? "yes" : "no",
		vk.visibility_buffer.classify_pipeline_ready ? "yes" : "no",
		vk.visibility_buffer.debug_gfx_ready ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "docs      : docs/RENDERER_2027.md\n" );
}
