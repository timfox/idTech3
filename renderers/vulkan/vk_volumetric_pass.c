#include "tr_local.h"
#include "vk.h"
#include "vk_image_layout.h"
#include "vk_post_fog.h"
#include "vk_temporal.h"
#include "vk_volumetric_pass.h"
#include "vk_post_aa.h"
#include "vk_render_pass.h"
#include "vk_scene_hdr_ownership.h"

void vk_volumetric_skip_cleanup( const char *reason, uint32_t restoreDepthSrcStages )
{
	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

	vk_reset_volumetric_history();
	/* Use doneWorldScene — HUD/weapon may set RDF_NOWORLDMODEL after the world view. */
	if ( tr.world && backEnd.doneWorldScene && vk_post_aa_output_active() ) {
		vk_post_scene_aa_apply();
		vk_log_post_fog_rebind( reason, vk_get_post_fog_source() );
	} else {
		vk_set_scene_post_fog_source( vk.color_image_view );
		vk_log_post_fog_rebind( reason, vk.color_image_view );
		vk_update_post_fog_descriptors( vk.color_image_view );
	}

	if ( tr.world && backEnd.doneWorldScene ) {
		if ( glConfig.stencilBits > 0 ) {
			depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			restoreDepthSrcStages,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );
	}

	backEnd.doneFog = qtrue;
}

void vk_volumetric_fog_before_oit( void )
{
	cvar_t *fogMode;
	int mode;
	static qboolean s_logged;
	static qboolean s_forcedModeLogged;

	if ( backEnd.doneFog ) {
		return;
	}
	fogMode = ri.Cvar_Get( "r_oitFogMode", "1", 0 );
	mode = fogMode ? fogMode->integer : 1;
	/*
	 * IQ P0-E: with production WBOIT, refuse legacy fogMode 0 (post-stack fog of
	 * resolved HDR). Force effective mode 1 for this frame's pre-OIT froxel.
	 */
	if ( r_oit && r_oit->integer >= 1 && mode < 1 ) {
		if ( !s_forcedModeLogged ) {
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][fog] r_oitFogMode 0 refused with r_oit>=1 (double-fog risk); "
				"using mode 1 for opaque pre-OIT volumetric\n" S_COLOR_WHITE );
			s_forcedModeLogged = qtrue;
		}
		mode = 1;
	}
	if ( mode < 1 ) {
		return; /* Legacy mode 0: keep frame-end volumetric over full HDR. */
	}
	if ( !r_oit || !r_oit->integer || !r_fbo || !r_fbo->integer ) {
		return;
	}
	if ( !tr.world || !backEnd.doneWorldScene ) {
		return;
	}

	if ( !s_logged ) {
		ri.Printf( PRINT_ALL,
			"[VK][fog] pre-OIT volumetric (r_oitFogMode effective=%d): opaque fogged before WBOIT resolve\n",
			mode );
		s_logged = qtrue;
	}
	/* Match frame-end: leave MAIN so color can leave COLOR_ATTACHMENT. */
	vk_end_render_pass();
	vk_volumetric_fog_pass();
	vk_scene_hdr_note_writer( SCENE_HDR_VOLUMETRIC, "volumetric_fog_pre_oit",
		SCENE_HDR_WRITE_COMPOSE );
}
