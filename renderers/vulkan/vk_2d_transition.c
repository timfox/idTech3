#include "tr_local.h"
#include "vk.h"
#include "vk_post_fog.h"
#include "vk_scene_pass.h"
#include "vk_temporal.h"
#include "vk_volumetric_pass.h"

static qboolean vk_can_use_2d_overlay_path( void )
{
	if ( !vk.fboActive ) {
		return qfalse;
	}
	if ( !vk.cmd || vk.cmd->swapchain_image_index >= MAX_SWAPCHAIN_IMAGES ) {
		return qfalse;
	}
	if ( vk.render_pass.ui_overlay == VK_NULL_HANDLE ||
		vk.ui_overlay_image_view == VK_NULL_HANDLE ||
		vk.framebuffers.ui_overlay[ vk.cmd->swapchain_image_index ] == VK_NULL_HANDLE ) {
		return qfalse;
	}

	return qtrue;
}

static void vk_begin_2d_overlay_or_fallback( qboolean preferLoad )
{
	if ( vk_can_use_2d_overlay_path() ) {
		if ( preferLoad && vk.uiOverlayContentValid ) {
			vk_begin_ui_overlay_render_pass_load();
		} else {
			vk_begin_ui_overlay_render_pass();
		}
		return;
	}

	vk.uiOverlayActive = qfalse;
	vk_begin_post_bloom_render_pass();
}

void vk_prepare_2d( void )
{
	vk_prepare_frame_temporal_state();

	/* Already in UI overlay and in-pass: nothing to do. */
	if ( vk.inRenderPass && vk.renderPassIndex == RENDER_PASS_UI_OVERLAY && vk.uiOverlayActive ) {
		return;
	}

	/* Mid-frame bloom ended overlay: resume with load to preserve HUD. */
	if ( !vk.inRenderPass && vk.uiOverlayContentValid && vk_can_use_2d_overlay_path() &&
		( vk.renderPassIndex == RENDER_PASS_UI_OVERLAY ||
			vk.renderPassIndex == RENDER_PASS_POST_BLOOM ||
			vk.renderPassIndex == RENDER_PASS_MAIN ) ) {
		vk_pass_diag_stage( "prepare_2d_overlay_resume" );
		vk_begin_ui_overlay_render_pass_load();
		return;
	}

	/* Cinematic/menu-only: no world, no RC_DRAW_SURFS, so no render pass was ever started.
	 * Start a fresh main pass here so 2D can draw on a cleared color target instead of
	 * inheriting stale swapchain/FBO contents from a previous frame.
	 * Use doneWorldScene (not tr.refdef.rdflags): HUD/weapon scenes set RDF_NOWORLDMODEL
	 * after the world view and must not clear the HDR color target. */
	if ( ( !tr.world || !backEnd.doneWorldScene ) && !vk.inRenderPass ) {
		if ( vk.cmd && vk.cmd->command_buffer != VK_NULL_HANDLE &&
			vk.cmd->swapchain_image_index < MAX_SWAPCHAIN_IMAGES &&
			vk.render_pass.main != VK_NULL_HANDLE &&
			vk.framebuffers.main[ vk.cmd->swapchain_image_index ] != VK_NULL_HANDLE &&
			vk.fboActive ) {
			vk_reset_volumetric_history();
			backEnd.doneFog = qtrue;
			vk_reset_scene_src_rect_tracking();
			vk_begin_main_render_pass();
			vk_end_render_pass();
			vk_set_scene_post_fog_source( vk.color_image_view );
			vk_log_post_fog_rebind( "prepare_2d no-world initial scene source", vk.color_image_view );
			vk_update_post_fog_descriptors( vk.color_image_view );
			vk_begin_2d_overlay_or_fallback( qfalse );
		}
		return;
	}

	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk.inRenderPass ) {
		return;
	}
	if ( vk.cmd->swapchain_image_index >= MAX_SWAPCHAIN_IMAGES ) {
		return;
	}
	if ( vk.render_pass.post_bloom == VK_NULL_HANDLE ||
		vk.framebuffers.main[ vk.cmd->swapchain_image_index ] == VK_NULL_HANDLE ) {
		return;
	}

	// Only split the main scene pass.
	if ( vk.renderPassIndex != RENDER_PASS_MAIN && vk.renderPassIndex != RENDER_PASS_POST_BLOOM ) {
		return;
	}

	/* Menu/cinematic only — not HUD after a world view (see doneWorldScene). */
	if ( !tr.world || !backEnd.doneWorldScene ) {
		vk_reset_volumetric_history();
		backEnd.doneFog = qtrue;
		vk_set_scene_post_fog_source( vk.color_image_view );
		vk_end_render_pass();
		vk_log_post_fog_rebind( "prepare_2d no-world scene source", vk.color_image_view );
		vk_update_post_fog_descriptors( vk.color_image_view );
		vk_begin_2d_overlay_or_fallback( vk.uiOverlayContentValid );
		return;
	}

	{
		int tier = 0;
		qboolean runFogPass = qfalse;
		cvar_t *tierCvar = ri.Cvar_Get( "r_volumetricFogTier", "0", 0 );
		if ( tierCvar ) {
			tier = tierCvar->integer;
		}
		runFogPass = ( vk.fboActive && !backEnd.doneFog && r_volumetricFog && r_volumetricFog->integer &&
			tier < 2 && tier != 4 );

		vk_end_render_pass();
		if ( runFogPass ) {
			vk_volumetric_fog_pass();
			vk_log_post_fog_rebind( "prepare_2d split (scene+2D on color_image)", vk.color_image_view );
			vk_update_post_fog_descriptors( vk.color_image_view );
		} else {
			vk_reset_volumetric_history();
			backEnd.doneFog = qtrue;
			vk_set_scene_post_fog_source( vk.color_image_view );
			vk_log_post_fog_rebind( "prepare_2d split (scene source)", vk.color_image_view );
			vk_update_post_fog_descriptors( vk.color_image_view );
		}
	}

	/* First split from MAIN clears overlay; re-entry from POST_BLOOM after bloom loads. */
	vk_begin_2d_overlay_or_fallback( vk.renderPassIndex == RENDER_PASS_POST_BLOOM && vk.uiOverlayContentValid );
}
