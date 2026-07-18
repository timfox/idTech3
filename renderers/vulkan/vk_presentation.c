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
#include "vk_swapchain.h"
#include "vk_sync.h"
#include "vk_temporal.h"
#include "vk_util.h"
#ifdef USE_IMGUI
#include "inspector/vk_imgui.h"
#endif

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

	vk_destroy_pipelines( qfalse );
	vk_destroy_framebuffers();
	vk_destroy_render_passes();
	vk_destroy_attachments();
	vk_destroy_swapchain();
	vk_destroy_sync_primitives();
#ifdef VK_CUBEMAP
	vk_destroy_cubemap_prefilter();
#endif
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
	vk_create_attachments();
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

	vk_update_post_process_pipelines();

#ifdef USE_IMGUI
	VkImgui_SwapchainRestarted();
#endif

#ifdef VK_PBR_BRDFLUT
	vk_create_brfdlut();
#endif

	vk_temporal_request_sticky_reset( VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE );
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
