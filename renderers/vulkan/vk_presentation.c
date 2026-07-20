/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Swapchain teardown/recreate and swapchain restart helper.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_attachments.h"
#include "vk_descriptor_sets.h"
#include "vk_device.h"
#include "vk_instance.h"
#include "vk_framebuffers.h"
#include "vk_render_pass.h"
#include "vk_resource_destroy.h"
#include "vk_post_fog.h"
#include "vk_scene_pass.h"
#include "vk_forward_plus.h"
#include "vk_deferred_gbuffer.h"
#include "vk_visibility_buffer.h"
#include "vk_ambient_visibility.h"
#include "vk_raster_gi.h"
#include "vk_gpu_particles.h"
#include "vk_deferred_decals.h"
#include "vk_distortion.h"
#include "vk_swapchain.h"
#include "vk_sync.h"
#include "vk_temporal.h"
#include "vk_util.h"
#ifdef USE_IMGUI
#include "inspector/vk_imgui.h"
#endif

static void vk_reset_presentation_runtime_state( void )
{
	uint32_t i;

	vk.inRenderPass = qfalse;
	vk.uiOverlayActive = qfalse;
	vk.uiOverlayContentValid = qfalse;
	vk.renderPassIndex = RENDER_PASS_MAIN;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		vk.tess[i].swapchain_image_acquired = qfalse;
		vk.tess[i].swapchain_image_index = 0;
	}
}

void vk_teardown_presentation_targets( void )
{
	uint32_t i;

	if ( vk.device == VK_NULL_HANDLE || qvkQueuePresentKHR == NULL ) {
		return;
	}

	vk_wait_idle();

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.tess[i].command_buffer != VK_NULL_HANDLE ) {
			qvkResetCommandBuffer( vk.tess[i].command_buffer, 0 );
		}
	}

	/*
	 * Sticky temporal invalidation before tearing down history owners (TAA / AV /
	 * volumetrics). Restore also requests this; requesting here covers partial
	 * restore failures after attachments are already gone.
	 */
	vk_temporal_request_sticky_reset( VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE );

	/* Drop descriptor/pipeline bindings that hold G-buffer / AV image views
	 * before destroying the underlying attachments (resize / swapchain restart). */
	vk_deferred_gbuffer_invalidate_runtime();
	vk_visibility_buffer_shutdown();
	vk_ambient_visibility_shutdown();
	vk_raster_gi_shutdown();
	vk_gpu_particles_shutdown();
	vk_deferred_decals_shutdown();
	vk_distortion_shutdown();
	vk_deferred_gbuffer_note_recreate( "presentation_teardown" );

	vk_destroy_pipelines( qfalse );
	vk_destroy_framebuffers();
	vk_destroy_render_passes();
	vk_destroy_attachments();
	vk_destroy_swapchain();
	vk_destroy_sync_primitives();
#ifdef VK_CUBEMAP
	vk_destroy_cubemap_prefilter();
#endif

	vk_reset_presentation_runtime_state();
	vk_pass_diag_reset();
}

void vk_restore_presentation_targets( void )
{
	if ( vk.device == VK_NULL_HANDLE || vk_surface == VK_NULL_HANDLE ) {
		return;
	}

	vk_select_surface_format( vk.physical_device, vk_surface );
	vk_setup_surface_formats( vk.physical_device );

	vk_create_sync_primitives();
	vk_create_swapchain( vk.physical_device, vk.device, vk_surface, vk.present_format, &vk.swapchain, qfalse );
	if ( vk.swapchain == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING,
			"[VK][presentation] restore aborted: swapchain recreate failed (temporal sticky already set)\n" );
		vk_reset_presentation_runtime_state();
		vk_pass_diag_reset();
		return;
	}
	vk_create_attachments();
	if ( vk.color_image == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING,
			"[VK][presentation] restore incomplete: color attachment missing after recreate\n" );
	}
	vk_create_render_passes();
	vk_create_framebuffers();

#ifdef VK_PBR_BRDFLUT
	vk_create_brdflut_pipeline();
#endif
#ifdef VK_CUBEMAP
	vk_create_cubemap_prefilter();
#endif
	vk_update_attachment_descriptors();
	vk_update_volumetric_descriptors();
	vk_reset_scene_src_rect_tracking();
	vk_reset_post_fog_frame_state();
	vk_reset_presentation_runtime_state();
	vk_pass_diag_reset();
	vk.renderWidth = vk.mainColorWidth > 0u ? vk.mainColorWidth :
		( vk.swapchain_extent_valid ? vk.swapchain_extent.width : ( glConfig.vidWidth > 0 ? (uint32_t)glConfig.vidWidth : 1u ) );
	vk.renderHeight = vk.mainColorHeight > 0u ? vk.mainColorHeight :
		( vk.swapchain_extent_valid ? vk.swapchain_extent.height : ( glConfig.vidHeight > 0 ? (uint32_t)glConfig.vidHeight : 1u ) );
	vk.renderScaleX = 1.0f;
	vk.renderScaleY = 1.0f;

	vk_update_post_process_pipelines();
	vk_validate_pbr_ibl_resources();
	vk_forward_plus_ensure_runtime();
	vk_deferred_gbuffer_note_recreate( "presentation_restore" );
	vk_deferred_gbuffer_ensure_runtime();
	vk_visibility_buffer_ensure_runtime();
	/* AV re-inits lazily on frame_begin once G-buffer resources are live. */
	vk_ambient_visibility_init();
	vk_ambient_visibility_reset_history();
	vk_raster_gi_init();
	vk_gpu_particles_init();
	vk_deferred_decals_init();
	vk_distortion_init();

#ifdef USE_IMGUI
	VkImgui_SwapchainRestarted();
#endif

#ifdef VK_PBR_BRDFLUT
	vk_create_brfdlut();
#endif

	vk_temporal_request_sticky_reset( VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE );
}

void vk_presentation_note_window_restored( const char *reason )
{
	if ( vk.device == VK_NULL_HANDLE || vk.device_lost ) {
		return;
	}

	vk_reset_presentation_runtime_state();
	vk_pass_diag_reset();
	vk_temporal_request_sticky_reset( VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE );

	ri.Printf( PRINT_DEVELOPER,
		"[VK][presentation] window restored (%s): cleared acquire flags + sticky temporal reset\n",
		reason ? reason : "unknown" );
}

void vk_restart_swapchain( const char *funcname, VkResult res )
{
	(void)res;

	vk.swapchain_restart_count++;
	vk.swapchain_last_restart_result = (int)res;
	vk.swapchain_last_restart_ms = ri.Milliseconds();

#ifdef _DEBUG
	ri.Printf( PRINT_WARNING, "%s(%s): restarting swapchain...\n", funcname, vk_result_string( res ) );
#else
	ri.Printf( PRINT_WARNING, "%s(): restarting swapchain...\n", funcname );
#endif

	vk_teardown_presentation_targets();
	vk_restore_presentation_targets();
}
