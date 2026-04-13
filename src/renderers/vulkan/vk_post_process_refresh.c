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

void vk_update_post_process_pipelines( void )
{
	if ( vk.fboActive ) {
		vk_create_post_process_pipeline( 0, 0, 0 );
		if ( vk.ui_overlay_image != VK_NULL_HANDLE ) {
			vk_create_post_process_pipeline( 22, glConfig.vidWidth, glConfig.vidHeight );
		}
		if ( vk.capture.image ) {
			vk_create_post_process_pipeline( 3, gls.captureWidth, gls.captureHeight );
		}
		if ( vk.smaaActive ) {
			vk_create_post_process_pipeline( 10, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 11, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 12, glConfig.vidWidth, glConfig.vidHeight );
		}
		vk_create_post_process_pipeline( 23, glConfig.vidWidth, glConfig.vidHeight );
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

			vk_create_post_process_pipeline( 2, glConfig.vidWidth, glConfig.vidHeight );
		}

		if ( r_ssao && r_ssao->integer ) {
			vk_create_post_process_pipeline( 5, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 21, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 6, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 7, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 8, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_post_process_pipeline( 9, glConfig.vidWidth, glConfig.vidHeight );
		}
		if ( r_oit && r_oit->integer ) {
			vk_create_post_process_pipeline( 20, glConfig.vidWidth, glConfig.vidHeight );
			vk_create_oit_accum_pipeline();
		}
		if ( PostFX_SSR_IsEnabled() ) {
			vk_create_post_process_pipeline( 13, glConfig.vidWidth, glConfig.vidHeight );
		}
		vk_create_atmosphere_pipeline();
	}
}
