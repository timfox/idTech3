/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Rebuilds post-process compute/graphics pipelines when FBO settings change.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_atmosphere.h"
#include "vk_pipeline_helpers.h"
#include "vk_postfx.h"
#include "vk_scene_pass.h"
#include "vk_reactive_mask.h"
#include "vk_temporal_class.h"

void vk_update_post_process_pipelines( void )
{
	uint32_t postWidth = 0;
	uint32_t postHeight = 0;

	vk_get_active_render_extent( &postWidth, &postHeight );

	if ( vk.fboActive ) {
		vk_create_post_process_pipeline( 0, 0, 0 );
		if ( vk.ui_overlay_image != VK_NULL_HANDLE ) {
			vk_create_post_process_pipeline( 22, postWidth, postHeight );
		}
		if ( vk.capture.image ) {
			vk_create_post_process_pipeline( 3, gls.captureWidth, gls.captureHeight );
		}
		if ( vk.smaaActive ) {
			vk_create_post_process_pipeline( 10, postWidth, postHeight );
			vk_create_post_process_pipeline( 11, postWidth, postHeight );
			vk_create_post_process_pipeline( 12, postWidth, postHeight );
		}
		if ( vk.fxaaActive ) {
			vk_create_post_process_pipeline( 24, postWidth, postHeight );
		}
		/* Spatial adaptive SS reuses TAA history[0] as current-frame scratch (TAA off). */
		vk_create_post_process_pipeline( 26, postWidth, postHeight );
		vk_create_post_process_pipeline( 23, postWidth, postHeight );
		vk_create_post_process_pipeline( 27, postWidth, postHeight );
		vk_create_post_process_pipeline( 28, postWidth, postHeight );
		if ( r_bloom->integer ) {
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;
			uint32_t i;

			vk_create_post_process_pipeline( 1, width, height );

			for ( i = 0; i < ARRAY_LEN( vk.blur_pipeline ); i += 2 ) {
				width /= 2;
				height /= 2;
				vk_create_blur_pipeline( i + 0, width, height, qtrue );
				vk_create_blur_pipeline( i + 1, width, height, qfalse );
			}

			vk_create_post_process_pipeline( 2, postWidth, postHeight );
		}
		if ( vk.lensFlareActive ) {
			vk_create_post_process_pipeline( 25, postWidth, postHeight );
		}

		if ( r_ssao && r_ssao->integer ) {
			vk_create_post_process_pipeline( 5, postWidth, postHeight );
			vk_create_post_process_pipeline( 21, postWidth, postHeight );
			vk_create_post_process_pipeline( 6, postWidth, postHeight );
			vk_create_post_process_pipeline( 7, postWidth, postHeight );
			vk_create_post_process_pipeline( 8, postWidth, postHeight );
			vk_create_post_process_pipeline( 9, postWidth, postHeight );
		}
		if ( r_oit && r_oit->integer ) {
			vk_create_post_process_pipeline( 20, postWidth, postHeight );
			vk_create_oit_accum_pipeline();
			if ( r_oit->integer == 2 ) {
				vk_create_oit_moments_pipeline();
				vk_create_oit_accum_mboit_pipeline();
			}
		}
		vk_create_reactive_mask_pipeline();
		vk_create_temporal_class_pipeline();
		if ( PostFX_SSR_IsEnabled() ) {
			vk_create_post_process_pipeline( 13, postWidth, postHeight );
		}
		vk_create_atmosphere_pipeline();
		PostFX_NotifyPostPipelinesRebuilt();
	}
}
