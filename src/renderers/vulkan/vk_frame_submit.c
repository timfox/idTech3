/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Per-frame command recording, queue submit, and present.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#include "tr_local.h"
#include "vk_forward_plus.h"
#include "vk.h"
#include "vk_descriptors.h"
#include "vk_draw_state.h"
#include "vk_frame_end.h"
#include "vk_geometry.h"
#include "vk_instance.h"
#include "vk_occlusion.h"
#include "vk_post_fog.h"
#include "vk_postfx.h"
#include "vk_postfx_params.h"
#include "vk_render_pass.h"
#include "vk_resource_destroy.h"
#include "vk_scene_pass.h"
#include "vk_staging.h"
#include "vk_swapchain.h"
#include "vk_temporal.h"
#include "vk_util.h"
#include "vk_volumetric_internal.h"
#include "vk_rtx.h"

#ifdef __ANDROID__
#include "../../platform/android/android_surface_glue.h"
#endif

#ifndef UINT64_MAX
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL
#endif

static void vk_begin_screenmap_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.screenmap;

	if ( frameBuffer == VK_NULL_HANDLE || vk.render_pass.screenmap == VK_NULL_HANDLE )
		return;

	vk.renderPassIndex = RENDER_PASS_SCREENMAP;

	vk.renderWidth = vk.screenMapWidth;
	vk.renderHeight = vk.screenMapHeight;

	vk.renderScaleX = (float)vk.renderWidth / (float)glConfig.vidWidth;
	vk.renderScaleY = (float)vk.renderHeight / (float)glConfig.vidHeight;

	vk_begin_render_pass_tracked( vk.render_pass.screenmap, frameBuffer, qtrue, vk.renderWidth, vk.renderHeight );
}

void vk_end_render_pass( void )
{
	vk_end_render_pass_tracked();
}

static qboolean vk_find_screenmap_drawsurfs( void )
{
	const void *curCmd = &backEndData->commands.cmds;
	const drawBufferCommand_t *db_cmd;
	const drawSurfsCommand_t *ds_cmd;

	for ( ;; ) {
		curCmd = PADP( curCmd, sizeof(void *) );
		switch ( *(const int *)curCmd ) {
			case RC_DRAW_BUFFER:
				db_cmd = (const drawBufferCommand_t *)curCmd;
				curCmd = (const void *)(db_cmd + 1);
				break;
			case RC_DRAW_SURFS:
				ds_cmd = (const drawSurfsCommand_t *)curCmd;
				return ds_cmd->refdef.needScreenMap;
			default:
				return qfalse;
		}
	}
}

void vk_begin_frame( void )
{
	VkCommandBufferBeginInfo begin_info;
	VkResult res;
	qboolean needPost = qfalse;

	if ( vk.device_lost ) {
		return;
	}
	if ( vk.frame_count++ )
		return;

	vk.inRenderPass = qfalse;

#ifdef USE_VK_PBR
	/* r_forwardPlusShade only changes PBR fragment specialization on world draw pipelines.
	 * Do not use vk_destroy_pipelines() here: it also tears down gamma/bloom/smaa and other
	 * post paths unrelated to Forward+ (black screen if not rebuilt the same frame). */
	if ( r_forwardPlusShade && r_forwardPlusShade->modified ) {
		r_forwardPlusShade->modified = qfalse;
		if ( vk.device && !vk.device_lost && vk.pipelines_count > (uint32_t)vk.pipelines_world_base ) {
			ri.Printf( PRINT_ALL, "[VK][Forward+] r_forwardPlusShade changed; invalidating world graphics pipelines for new fragment specialization\n" );
			vk_destroy_world_graphics_pipelines();
		}
	}
#endif

	if ( PostFX_NeedsPipelineUpdate() ) {
		needPost = qtrue;
	}
	if ( needPost && vk.fboActive ) {
		vk_update_post_process_pipelines();
	}

	vk_begin_motion_frame();
	vk_prime_gpu_morph_weights_current();
	vk.sun_shadow_valid = qfalse;
	vk.temporal.preparedThisFrame = qfalse;
	vk.uiOverlayActive = qfalse;

	vk.cmd = &vk.tess[ vk.cmd_index ];

#ifdef USE_VULKAN_RTX
	vk_rtx_frame_begin();
#endif

	if ( vk.cmd->waitForFence ) {
		vk.cmd->waitForFence = qfalse;
		res = qvkWaitForFences( vk.device, 1, &vk.cmd->rendering_finished_fence, VK_FALSE, 1e10 );
		if ( res != VK_SUCCESS ) {
			if ( res == VK_ERROR_DEVICE_LOST ) {
				vk.device_lost = qtrue;
				ri.Error( ERR_FATAL, "Vulkan: %s returned %s (GPU lost)", "vkWaitForFences", vk_result_string( res ) );
			}
			else {
				ri.Error( ERR_FATAL, "Vulkan: %s returned %s", "vkWaitForFences", vk_result_string( res ) );
			}
		}
		VK_CHECK( qvkResetFences( vk.device, 1, &vk.cmd->rendering_finished_fence ) );
		if ( vk.volumetric_query_pool != VK_NULL_HANDLE &&
			r_volumetricFogPerfTimers && r_volumetricFogPerfTimers->integer ) {
			vk_update_volumetric_perf_queries();
		}
		if ( r_occlusionCulling && r_occlusionCulling->integer ) {
			vk_occlusion_readback();
		}
	}

	if ( !ri.CL_IsMinimized() && !vk.cmd->swapchain_image_acquired ) {
		qboolean retry = qfalse;
_retry:
		res = qvkAcquireNextImageKHR( vk.device, vk.swapchain, 1 * 1000000000ULL, vk.cmd->image_acquired, VK_NULL_HANDLE, &vk.cmd->swapchain_image_index );
		if ( res < 0 ) {
			if ( res == VK_ERROR_OUT_OF_DATE_KHR && retry == qfalse ) {
				retry = qtrue;
				vk_restart_swapchain( __func__, res );
				goto _retry;
			} else {
				ri.Error( ERR_FATAL, "vkAcquireNextImageKHR returned %s", vk_result_string( res ) );
			}
		}
		vk.cmd->swapchain_image_acquired = qtrue;
	}

	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	VK_CHECK( qvkBeginCommandBuffer( vk.cmd->command_buffer, &begin_info ) );
	vk_reset_post_fog_frame_state();

	if ( vk.volumetric_query_pool != VK_NULL_HANDLE && !qvkResetQueryPoolEXT && qvkCmdResetQueryPool ) {
		const uint32_t qbase = vk.cmd_index * VK_VOLUMETRIC_QUERY_SLOTS;
		qvkCmdResetQueryPool( vk.cmd->command_buffer, vk.volumetric_query_pool, qbase, VK_VOLUMETRY_QUERY_USED );
	}

	if ( vk.colorWriteMaskDynamic && qvkCmdSetColorWriteMaskEXT )
		vk_set_color_write_mask( qtrue, qtrue, qtrue, qtrue );

	if ( vk.cmd->vertex_buffer_offset > vk.stats.vertex_buffer_max ) {
		vk.stats.vertex_buffer_max = vk.cmd->vertex_buffer_offset;
	}

	if ( vk.stats.push_size > vk.stats.push_size_max ) {
		vk.stats.push_size_max = vk.stats.push_size;
	}

	vk.cmd->last_pipeline = VK_NULL_HANDLE;

	backEnd.screenMapDone = qfalse;

	/* Vegetation wind compute is dispatched from RB_EndSurface after SURF_VEGETATION
	 * batches upload staging (see tr_shade.c); dispatch here at frame start would run
	 * before tessellation and see vertexCount==0. */

	if ( vk_find_screenmap_drawsurfs() ) {
		vk_begin_screenmap_render_pass();
	} else {
		vk_begin_main_render_pass();
	}

	vk.cmd->uniform_read_offset = 0;
	vk.cmd->vertex_buffer_offset = 0;
	Com_Memset( vk.cmd->vertex_buffer_ptr, 0, 64 );
	vk.cmd->vertex_buffer_offset = PAD( 64, 32 );
	vk_reset_iqm_storage_offsets();
	Com_Memset( vk.cmd->buf_offset, 0, sizeof( vk.cmd->buf_offset ) );
	Com_Memset( vk.cmd->vbo_offset, 0, sizeof( vk.cmd->vbo_offset ) );
	vk.cmd->curr_index_buffer = VK_NULL_HANDLE;
	vk.cmd->curr_index_offset = 0;
	vk.cmd->num_indexes = 0;

	Com_Memset( &vk.cmd->descriptor_set, 0, sizeof( vk.cmd->descriptor_set ) );
	vk.cmd->descriptor_set.start = ~0U;
#ifdef USE_VK_PBR
	if ( vk.maxBoundDescriptorSets >= VK_DESC_COUNT ) {
		VkDescriptorSet fp_set = vk_forward_plus_get_graphics_descriptor_set();
		if ( fp_set != VK_NULL_HANDLE ) {
			vk.cmd->descriptor_set.current[VK_DESC_FORWARD_PLUS] = fp_set;
		}
	}
#endif

	Com_Memset( &vk.cmd->scissor_rect, 0, sizeof( vk.cmd->scissor_rect ) );

	vk.stats.push_size = 0;
}

void vk_prepare_frame_temporal_state( void )
{
	qboolean reset_taa = qfalse;

	if ( vk.temporal.preparedThisFrame ) {
		return;
	}

	if ( r_taa && r_taa->modified ) {
		r_taa->modified = qfalse;
		reset_taa = qtrue;
	}
	if ( r_taa_feedbackStationary && r_taa_feedbackStationary->modified ) {
		r_taa_feedbackStationary->modified = qfalse;
		reset_taa = qtrue;
	}
	if ( r_taa_feedbackMotion && r_taa_feedbackMotion->modified ) {
		r_taa_feedbackMotion->modified = qfalse;
		reset_taa = qtrue;
	}
	if ( r_taa_sharpen && r_taa_sharpen->modified ) {
		r_taa_sharpen->modified = qfalse;
		reset_taa = qtrue;
	}
	if ( reset_taa ) {
		vk_reset_taa_history();
	}

	vk_temporal_begin_frame();
	vk_update_postfx_params( vk.cmd_index );
	vk.temporal.preparedThisFrame = qtrue;
}

static void vk_resize_geometry_buffer( void )
{
	int i;

	vk_end_render_pass();

	VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );

	qvkResetCommandBuffer( vk.cmd->command_buffer, 0 );

	vk_wait_idle();

	vk_release_geometry_buffers();

	vk_create_geometry_buffers( vk.geometry_buffer_size_new );
	vk.geometry_buffer_size_new = 0;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
		vk_update_uniform_descriptor( vk.tess[ i ].uniform_descriptor, vk.tess[ i ].vertex_buffer );

	ri.Printf( PRINT_DEVELOPER, "...geometry buffer resized to %iK\n", (int)( vk.geometry_buffer_size / 1024 ) );
}

void vk_end_frame( void )
{
	const VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submit_info;

	if ( vk.frame_count == 0 )
		return;

	vk.frame_count = 0;

	if ( vk.geometry_buffer_size_new )
	{
		vk_resize_geometry_buffer();
		return;
	}

	vk_prepare_frame_temporal_state();

	if ( vk.fboActive )
	{
		vk.cmd->last_pipeline = VK_NULL_HANDLE;

		if ( PostFX_SSR_IsEnabled() )
		{
			vk_ssr_pass();
		}

		if ( r_bloom->integer )
		{
			vk_bloom();
		}

		if ( r_ssao && r_ssao->integer && !backEnd.doneSSAO )
		{
			vk_ssao_pass();
		}

		vk_end_frame_record_capture_if_needed();

		if ( !ri.CL_IsMinimized() )
		{
			VkImageView post_fog_src;
			VkImageView luminance_src;

			vk_end_frame_prepare_post_process( &post_fog_src, &luminance_src );
			vk_end_frame_record_taa_pass( &post_fog_src, &luminance_src );
			vk_end_frame_record_luminance_pass( luminance_src );
			vk_end_frame_record_gamma_pass( post_fog_src );
		}
	}
	else
	{
		vk_end_render_pass();
	}

	VK_CHECK( qvkEndCommandBuffer( vk.cmd->command_buffer ) );

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk.cmd->command_buffer;
	if ( !ri.CL_IsMinimized() ) {
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &vk.cmd->image_acquired;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
	} else {
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = NULL;
	}

	{
		VkResult sub_res = qvkQueueSubmit( vk.queue, 1, &submit_info, vk.cmd->rendering_finished_fence );
		if ( sub_res != VK_SUCCESS ) {
			if ( sub_res == VK_ERROR_DEVICE_LOST ) {
				vk.device_lost = qtrue;
			}
			ri.Error( ERR_FATAL, "Vulkan: qvkQueueSubmit returned %s", vk_result_string( sub_res ) );
		}
	}
	vk.cmd->waitForFence = qtrue;
	vk_temporal_commit_frame_state();

	backEnd.pc.msec = ri.Milliseconds() - backEnd.pc.msec;

	vk.renderPassIndex = RENDER_PASS_MAIN;
}

void vk_present_frame( void )
{
	VkPresentInfoKHR present_info;
	VkResult res;
	VkExtent2D new_extent;
	qboolean new_extent_valid;

#ifdef __ANDROID__
	Android_SurfaceThread_ProcessPending();
#endif

	if ( ri.CL_IsMinimized() || !vk.cmd->swapchain_image_acquired ) {
		return;
	}

	if ( gls.windowWidth == 0 || gls.windowHeight == 0 ) {
		return;
	}

	if ( vk.swapchain_extent_valid && ( vk.swapchain_extent.width == 0 || vk.swapchain_extent.height == 0 ) ) {
		return;
	}

	if ( !vk.cmd->waitForFence ) {
		return;
	}

	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = NULL;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &vk.swapchain_rendering_finished[ vk.cmd->swapchain_image_index ];
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &vk.swapchain;
	present_info.pImageIndices = &vk.cmd->swapchain_image_index;
	present_info.pResults = NULL;

	vk.cmd->swapchain_image_acquired = qfalse;

	res = qvkQueuePresentKHR( vk.queue, &present_info );
	switch ( res ) {
		case VK_SUCCESS:
			break;
		case VK_SUBOPTIMAL_KHR:
			new_extent_valid = vk_query_surface_extent( vk.physical_device, vk_surface, &new_extent );
			vk_log_swapchain_recreation( res, &vk.swapchain_extent, new_extent_valid ? &new_extent : NULL );
			if ( new_extent_valid && ( !vk.swapchain_extent_valid ||
					new_extent.width != vk.swapchain_extent.width ||
					new_extent.height != vk.swapchain_extent.height ) ) {
				vk_restart_swapchain( __func__, res );
				return;
			}
			break;
		case VK_ERROR_OUT_OF_DATE_KHR:
			new_extent_valid = vk_query_surface_extent( vk.physical_device, vk_surface, &new_extent );
			vk_log_swapchain_recreation( res, &vk.swapchain_extent, new_extent_valid ? &new_extent : NULL );
			vk_restart_swapchain( __func__, res );
			return;
		case VK_ERROR_DEVICE_LOST:
			ri.Printf( PRINT_DEVELOPER, "vkQueuePresentKHR: device lost\n" );
			break;
		default:
			ri.Error( ERR_FATAL, "vkQueuePresentKHR returned %s", vk_result_string( res ) );
	}

	vk.cmd_index++;
	vk.cmd_index %= NUM_COMMAND_BUFFERS;
	vk.cmd = &vk.tess[ vk.cmd_index ];
}
