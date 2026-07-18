#include "tr_local.h"
#include "vk.h"
#include "vk_image_layout.h"
#include "vk_post_fog.h"
#include "vk_temporal.h"
#include "vk_volumetric_pass.h"
#include "vk_post_aa.h"

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
