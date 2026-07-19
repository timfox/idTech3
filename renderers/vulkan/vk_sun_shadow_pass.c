/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Sun shadow map render pass (depth-only) for volumetric lighting.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_image_layout.h"
#include "vk_render_pass.h"
#include "vk_scene_pass.h"
#include "vk_pass_registry.h"

qboolean vk_begin_sun_shadow_render_pass( void )
{
	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

	if ( !vk.fboActive || vk.render_pass.sun_shadow == VK_NULL_HANDLE || vk.framebuffers.sun_shadow == VK_NULL_HANDLE ||
		vk.sun_shadow_image == VK_NULL_HANDLE || vk.sun_shadow_width == 0 || vk.sun_shadow_height == 0 )
	{
		return qfalse;
	}

	vk_spine_pass_begin( VK_SPINE_PASS_SUN_SHADOW );

	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	vk_end_render_pass();

	record_image_layout_transition( vk.cmd->command_buffer, vk.sun_shadow_image, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );
	vk_spine_note_layout( VK_SPINE_RES_SHADOW_SUN, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL );

	vk.renderPassIndex = RENDER_PASS_SUN_SHADOW;
	vk.renderWidth = vk.sun_shadow_width;
	vk.renderHeight = vk.sun_shadow_height;
	vk.renderScaleX = vk.renderScaleY = 1.0f;
	vk_begin_render_pass_tracked( vk.render_pass.sun_shadow, vk.framebuffers.sun_shadow, qtrue, vk.renderWidth, vk.renderHeight );
	vk_spine_note_clear( VK_SPINE_RES_SHADOW_SUN, VK_SPINE_PASS_SUN_SHADOW );

	if ( r_fogDebug && r_fogDebug->integer >= 1 && ( vk.volumetric_frame % 120u ) == 0u ) {
		ri.Printf( PRINT_ALL, "[VK][fog] sun shadow pass begin %ux%u image=0x%llx\n",
			vk.sun_shadow_width, vk.sun_shadow_height,
			(unsigned long long)(uintptr_t)vk.sun_shadow_image );
	}

	return qtrue;
}

void vk_end_sun_shadow_render_pass( void )
{
	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

	if ( !vk.fboActive || vk.sun_shadow_image == VK_NULL_HANDLE ) {
		return;
	}

	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	vk_end_render_pass();

	record_image_layout_transition( vk.cmd->command_buffer, vk.sun_shadow_image, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	vk_spine_note_layout( VK_SPINE_RES_SHADOW_SUN, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL );
	vk_spine_note_barrier( VK_SPINE_RES_SHADOW_SUN, VK_SPINE_PASS_SUN_SHADOW, "shadow_to_sample" );
	vk_spine_pass_end( VK_SPINE_PASS_SUN_SHADOW );

	if ( r_fogDebug && r_fogDebug->integer >= 1 && ( vk.volumetric_frame % 120u ) == 0u ) {
		ri.Printf( PRINT_ALL, "[VK][fog] sun shadow pass end image=0x%llx ATTACHMENT->READ_ONLY\n",
			(unsigned long long)(uintptr_t)vk.sun_shadow_image );
	}

	vk_begin_main_render_pass();
}
