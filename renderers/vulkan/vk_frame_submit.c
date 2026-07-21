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
#include "vk_present_recon.h"
#include "vk_gpu_scene.h"
#include "vk_ht_throughput.h"
#include "vk_ht_animation.h"
#include "vk_weather.h"
#include "vk_volumetric_clouds.h"
#include "vk_surface_evolution.h"
#include "vk_vshadow.h"
#include "vk_cinematic_camera.h"
#include "vk_exposure_histogram.h"
#include "vk_reference_lab.h"
#include "vk_frequency_aware.h"
#include "vk_spatial_aa.h"
#include "vk_scene_platform.h"
#include "vk_hiz.h"
#include "vk_render_pass.h"
#include "vk_resource_destroy.h"
#include "vk_scene_pass.h"
#include "vk_staging.h"
#include "vk_swapchain.h"
#include "vk_temporal.h"
#include "vk_pass_registry.h"
#include "vk_util.h"
#include "vk_volumetric_internal.h"
#include "vk_sim_render_debug.h"
#include "vk_rtx.h"
#include "vk_pathtrace.h"
#include "vk_hybrid1.h"
#include "vk_raygun.h"
#include "vk_surfel_gi.h"
#include "vk_rcgi.h"
#include "vk_ambient_visibility.h"
#include "vk_raster_gi.h"
#include "vk_gpu_particles.h"
#include "vk_deferred_decals.h"
#include "vk_distortion.h"
#include "vk_selective_sun_shadow.h"
#include "vk_selective_reflection.h"
#include "vk_vrcs.h"
#include "vk_grtx.h"
#include "vk_vuda.h"

#ifdef __ANDROID__
#include "platform/android/android_surface_glue.h"
#endif

#ifndef UINT64_MAX
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL
#endif

static qboolean vk_swapchain_extent_mismatch_detected( VkExtent2D *out_extent )
{
	VkExtent2D extent;

	if ( vk.device_lost || vk_surface == VK_NULL_HANDLE || !vk.swapchain_extent_valid ) {
		return qfalse;
	}

	if ( !vk_query_surface_extent( vk.physical_device, vk_surface, &extent ) ) {
		return qfalse;
	}

	if ( out_extent ) {
		*out_extent = extent;
	}

	return extent.width != vk.swapchain_extent.width || extent.height != vk.swapchain_extent.height;
}

static qboolean vk_restart_swapchain_if_extent_mismatch( const char *funcname, const char *stage )
{
	VkExtent2D new_extent;

	if ( !vk_swapchain_extent_mismatch_detected( &new_extent ) ) {
		return qfalse;
	}

	ri.Printf( PRINT_WARNING,
		"[VK] %s: surface extent changed during %s (%ux%u -> %ux%u), rebuilding swapchain before stale attachments black-screen\n",
		funcname ? funcname : "vk_frame_submit",
		stage ? stage : "frame",
		(unsigned int)vk.swapchain_extent.width,
		(unsigned int)vk.swapchain_extent.height,
		(unsigned int)new_extent.width,
		(unsigned int)new_extent.height );
	vk_log_swapchain_recreation( VK_SUBOPTIMAL_KHR, &vk.swapchain_extent, &new_extent );
	vk_restart_swapchain( funcname ? funcname : __func__, VK_SUBOPTIMAL_KHR );
	return qtrue;
}

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
#ifdef USE_VK_PBR
	qboolean needWorldPipeRebuild = qfalse;
#endif

	if ( vk.device_lost ) {
		return;
	}
	if ( vk.frame_count++ )
		return;

	vk_present_recon_begin_frame();

	/* Minimize skips the entire backend; on restore clear stale acquire flags and
	 * sticky-reset temporal so the first visible frame does not present garbage.
	 * Alt-tab / FOCUS_GAINED is owned by the client via re.NotifyWindowRestored. */
	{
		static qboolean s_wasMinimized;
		const qboolean minimized = ri.CL_IsMinimized() ? qtrue : qfalse;

		if ( s_wasMinimized && !minimized ) {
			vk_presentation_note_window_restored( "minimize_to_active" );
		}
		s_wasMinimized = minimized;
	}

	if ( !ri.CL_IsMinimized() && vk_restart_swapchain_if_extent_mismatch( __func__, "begin_frame" ) ) {
		return;
	}

	vk.inRenderPass = qfalse;

#ifdef USE_VK_PBR
	/* r_forwardPlusShade / r_deferredUnlitBase / r_deferredLighting change PBR fragment specialization
	 * on world draw pipelines. Do not use vk_destroy_pipelines() here: it also tears down gamma/bloom/smaa
	 * and other post paths unrelated to Forward+ (black screen if not rebuilt the same frame).
	 * World VkPipelines are shared across both command-buffer slots: wait for the whole queue before
	 * destroy so the other slot cannot still be executing draws (GPU hazard). */
	if ( r_forwardPlusShade && r_forwardPlusShade->modified ) {
		r_forwardPlusShade->modified = qfalse;
		needWorldPipeRebuild = qtrue;
	}
	if ( r_deferredUnlitBase && r_deferredUnlitBase->modified ) {
		r_deferredUnlitBase->modified = qfalse;
		needWorldPipeRebuild = qtrue;
	}
	if ( r_deferredLighting && r_deferredLighting->modified ) {
		r_deferredLighting->modified = qfalse;
		needWorldPipeRebuild = qtrue;
	}
	if ( needWorldPipeRebuild ) {
		if ( vk.device && !vk.device_lost && vk.pipelines_count > (uint32_t)vk.pipelines_world_base ) {
			ri.Printf( PRINT_ALL, "[VK][PBR] Fragment specialization changed; invalidating world graphics pipelines\n" );
			vk_wait_idle();
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
	vk.uiOverlayContentValid = qfalse;

	vk.cmd = &vk.tess[ vk.cmd_index ];

	vk_shs_frame_begin();
	vk_shr_frame_begin();
#ifdef USE_VULKAN_RTX
	vk_rtx_frame_begin();
	vk_grtx_frame_begin();
	vk_pathtrace_frame_begin();
	vk_hybrid1_frame_begin();
	vk_raygun_frame_begin();
	vk_surfel_gi_frame_begin();
	vk_rcgi_frame_begin();
#endif
	vk_vrcs_frame_begin();
	vk_ambient_visibility_frame_begin();
	vk_raster_gi_frame_begin();
	vk_gpu_particles_frame_begin();
	vk_deferred_decals_frame_begin();
	vk_distortion_frame_begin();
	vk_vuda_frame_begin();

	if ( vk.cmd->waitForFence ) {
		vk.cmd->waitForFence = qfalse;
		res = qvkWaitForFences( vk.device, 1, &vk.cmd->rendering_finished_fence, VK_FALSE, 1e10 );
		if ( res != VK_SUCCESS ) {
			if ( res == VK_ERROR_DEVICE_LOST ) {
				vk_fatal_device_lost( "vkWaitForFences", res );
			}
			else {
				ri.Error( ERR_FATAL, "Vulkan: %s returned %s", "vkWaitForFences", vk_result_string( res ) );
			}
		}
		VK_CHECK( qvkResetFences( vk.device, 1, &vk.cmd->rendering_finished_fence ) );
		if ( vk.volumetric_query_pool != VK_NULL_HANDLE &&
			vk_volumetric_perf_wanted() ) {
			vk_update_volumetric_perf_queries();
		}
		if ( vk.weapon_temporal_query_pool != VK_NULL_HANDLE ) {
			uint64_t queryValues[VK_WEAPON_TEMPORAL_QUERY_SLOTS];
			const uint32_t queryBase = vk.cmd_index * VK_WEAPON_TEMPORAL_QUERY_SLOTS;
			if ( qvkGetQueryPoolResults( vk.device, vk.weapon_temporal_query_pool,
				queryBase, VK_WEAPON_TEMPORAL_QUERY_SLOTS, sizeof( queryValues ),
				queryValues, sizeof( uint64_t ), VK_QUERY_RESULT_64_BIT ) == VK_SUCCESS &&
				queryValues[1] >= queryValues[0] ) {
				vk.temporal.weaponResolveGpuMs =
					(float)( queryValues[1] - queryValues[0] ) *
					vk.weapon_temporal_timestamp_period_ns * 1.0e-6f;
			}
		}
		VK_SimRenderDebugFrameEnd();
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
		if ( vk.cmd->swapchain_image_index >= vk.swapchain_image_count ) {
			if ( retry == qfalse ) {
				retry = qtrue;
				ri.Printf( PRINT_WARNING, "vkAcquireNextImageKHR returned out-of-range image index %u (count=%u); restarting swapchain...\n",
					(unsigned int)vk.cmd->swapchain_image_index, (unsigned int)vk.swapchain_image_count );
				vk_restart_swapchain( __func__, VK_SUBOPTIMAL_KHR );
				goto _retry;
			}
			ri.Error( ERR_FATAL, "vkAcquireNextImageKHR returned out-of-range image index %u (count=%u)",
				(unsigned int)vk.cmd->swapchain_image_index, (unsigned int)vk.swapchain_image_count );
		}
		vk.cmd->swapchain_image_acquired = qtrue;
	}

	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	VK_CHECK( qvkBeginCommandBuffer( vk.cmd->command_buffer, &begin_info ) );
	vk_reset_scene_src_rect_tracking();
	vk_reset_post_fog_frame_state();
	vk_validate_post_fog_runtime_sources( "begin_frame" );

	if ( vk.volumetric_query_pool != VK_NULL_HANDLE && !qvkResetQueryPoolEXT && qvkCmdResetQueryPool ) {
		const uint32_t qbase = vk.cmd_index * VK_VOLUMETRIC_QUERY_SLOTS;
		qvkCmdResetQueryPool( vk.cmd->command_buffer, vk.volumetric_query_pool, qbase, VK_VOLUMETRY_QUERY_USED );
	}
	if ( vk.weapon_temporal_query_pool != VK_NULL_HANDLE ) {
		const uint32_t queryBase = vk.cmd_index * VK_WEAPON_TEMPORAL_QUERY_SLOTS;
		if ( qvkResetQueryPoolEXT ) {
			qvkResetQueryPoolEXT( vk.device, vk.weapon_temporal_query_pool,
				queryBase, VK_WEAPON_TEMPORAL_QUERY_SLOTS );
		} else if ( qvkCmdResetQueryPool ) {
			qvkCmdResetQueryPool( vk.cmd->command_buffer, vk.weapon_temporal_query_pool,
				queryBase, VK_WEAPON_TEMPORAL_QUERY_SLOTS );
		}
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

	/* Vegetation wind compute runs from RB_EndSurface before draw (see tr_shade.c). */

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
	if ( r_taaMotionVectors && r_taaMotionVectors->modified ) {
		r_taaMotionVectors->modified = qfalse;
		reset_taa = qtrue;
	}
	if ( r_weaponTemporalMode && r_weaponTemporalMode->modified ) {
		r_weaponTemporalMode->modified = qfalse;
		reset_taa = qtrue;
	}
	if ( r_weaponTemporalHistoryWeight && r_weaponTemporalHistoryWeight->modified ) {
		r_weaponTemporalHistoryWeight->modified = qfalse;
		vk_reset_weapon_history();
	}
	if ( r_weaponTemporalVarianceGamma && r_weaponTemporalVarianceGamma->modified ) {
		r_weaponTemporalVarianceGamma->modified = qfalse;
		vk_reset_weapon_history();
	}
	if ( r_weaponTemporalDepthThreshold && r_weaponTemporalDepthThreshold->modified ) {
		r_weaponTemporalDepthThreshold->modified = qfalse;
		vk_reset_weapon_history();
	}
	if ( r_weaponTemporalReactiveScale && r_weaponTemporalReactiveScale->modified ) {
		r_weaponTemporalReactiveScale->modified = qfalse;
		vk_reset_weapon_history();
	}
	if ( reset_taa ) {
		vk_reset_taa_history();
	}

	vk_temporal_begin_frame();
	vk_spatial_aa_begin_frame();
	vk_frequency_aware_begin_frame();
	vk_scene_platform_begin_frame();
	vk_ht_throughput_begin_frame();
	vk_ht_animation_begin_frame();
	vk_spine_frame_begin();
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
		vk_spine_frame_end();
		return;
	}

	vk_prepare_frame_temporal_state();

	if ( vk.fboActive )
	{
		vk.cmd->last_pipeline = VK_NULL_HANDLE;

		if ( PostFX_SSR_IsEnabled() )
		{
			vk_spine_pass_begin( VK_SPINE_PASS_SSR );
			vk_ssr_pass();
			vk_spine_pass_end( VK_SPINE_PASS_SSR );
		}

		if ( r_bloom->integer && !vk_temporal_defer_bloom_for_weapon() )
		{
			vk_spine_pass_begin( VK_SPINE_PASS_BLOOM );
			vk_bloom();
			vk_spine_pass_end( VK_SPINE_PASS_BLOOM );
		}

		if ( vk.lensFlareActive )
		{
			vk_lens_flare();
		}

		if ( r_ssao && r_ssao->integer && !backEnd.doneSSAO )
		{
			vk_ssao_pass();
		}

		if ( !ri.CL_IsMinimized() )
		{
			VkImageView post_fog_src;
			VkImageView luminance_src;

			vk_end_frame_prepare_post_process( &post_fog_src, &luminance_src );
			vk_spine_pass_begin( VK_SPINE_PASS_TEMPORAL_RECON );
			vk_end_frame_record_taa_pass( &post_fog_src, &luminance_src );
			vk_spine_pass_end( VK_SPINE_PASS_TEMPORAL_RECON );
			if ( vk_temporal_defer_bloom_for_weapon() && !backEnd.doneBloom ) {
				vk_spine_pass_begin( VK_SPINE_PASS_BLOOM );
				vk_bloom();
				vk_spine_pass_end( VK_SPINE_PASS_BLOOM );
				post_fog_src = vk_get_post_fog_source();
				luminance_src = post_fog_src;
			}
			vk_spine_pass_begin( VK_SPINE_PASS_EYE_ADAPTATION );
			vk_end_frame_record_luminance_pass( luminance_src );
			vk_spine_pass_end( VK_SPINE_PASS_EYE_ADAPTATION );
			vk_spine_pass_begin( VK_SPINE_PASS_PRESENTATION );
			vk_end_frame_record_gamma_pass( post_fog_src );
			vk_spine_pass_end( VK_SPINE_PASS_PRESENTATION );
		}

		/* After gamma/overlay have written the swapchain (LDR presentable). */
		vk_end_frame_record_capture_if_needed();
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
				vk_fatal_device_lost( "vkQueueSubmit", sub_res );
			}
			ri.Error( ERR_FATAL, "Vulkan: qvkQueueSubmit returned %s", vk_result_string( sub_res ) );
		}
	}
	vk.cmd->waitForFence = qtrue;
	vk_present_recon_note_gpu_submit();
	if ( vk.temporal.sharedCameraCut ) {
		vk_volumetric_clouds_on_camera_cut();
		vk_hiz_on_camera_cut();
		vk_vshadow_on_camera_cut();
		vk_exposure_histogram_on_camera_cut();
	}
	vk_weather_update();
	vk_surface_evolution_update();
	vk_vshadow_begin_frame();
	vk_cinematic_camera_begin_frame();
	vk_reference_lab_begin_frame();
	vk_frequency_aware_begin_frame();
	vk_volumetric_clouds_begin_frame();
	vk_gpu_scene_end_frame();
	vk_ht_throughput_end_frame();
	vk_ht_animation_end_frame();
	vk_temporal_commit_frame_state();
	vk_spine_frame_end();
	vk_vuda_after_queue_submit();

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

	if ( vk_restart_swapchain_if_extent_mismatch( __func__, "present" ) ) {
		vk.cmd->swapchain_image_acquired = qfalse;
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
	vk_present_recon_note_present();
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
			vk.device_lost = qtrue;
			vk_report_device_lost_context( "vkQueuePresentKHR" );
			ri.Printf( PRINT_WARNING, "vkQueuePresentKHR: device lost (teardown will skip destroy spam)\n" );
			break;
		default:
			ri.Error( ERR_FATAL, "vkQueuePresentKHR returned %s", vk_result_string( res ) );
	}

	vk.cmd_index++;
	vk.cmd_index %= NUM_COMMAND_BUFFERS;
	vk.cmd = &vk.tess[ vk.cmd_index ];
}
