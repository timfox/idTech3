/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Apply latched r_renderMode to Vulkan lighting path
(forward / deferred / Forward+ / Unified Clustered).
===========================================================================
*/

#include "tr_local.h"
#include "vk_forward_plus.h"

static void R_LatchCvarInt( cvar_t *cv, const char *name, int value )
{
	char buf[16];

	if ( !cv || cv->integer == value ) {
		return;
	}
	Com_sprintf( buf, sizeof( buf ), "%d", value );
	ri.Cvar_Set( name, buf );
	cv->integer = value;
	cv->modified = qtrue;
}

static void R_LatchCvarFloat( cvar_t *cv, const char *name, float value )
{
	char buf[32];

	if ( !cv || cv->value == value ) {
		return;
	}
	Com_sprintf( buf, sizeof( buf ), "%g", value );
	ri.Cvar_Set( name, buf );
	cv->value = value;
	cv->modified = qtrue;
}

void R_ApplyRenderModeLatch( void )
{
	int mode;
	static int s_last_logged_mode = -1;

	if ( !r_renderMode ) {
		return;
	}

	mode = r_renderMode->integer;
	if ( mode < 0 || mode > 3 ) {
		mode = 0;
	}

	switch ( mode ) {
	case 1:
		R_LatchCvarInt( r_deferredGBuffer, "r_deferredGBuffer", 1 );
		R_LatchCvarInt( r_deferredGBufferFill, "r_deferredGBufferFill", 1 );
		R_LatchCvarInt( r_forwardPlus, "r_forwardPlus", 1 );
		R_LatchCvarInt( r_deferredUnlitBase, "r_deferredUnlitBase", 1 );
		/* Transparent Forward+ shade; opaque uses deferred handoff (same split as mode 3). */
		if ( r_forwardPlusShade && r_forwardPlusShade->value <= 0.0f ) {
			R_LatchCvarFloat( r_forwardPlusShade, "r_forwardPlusShade", 1.0f );
		}
		if ( r_forwardPlusOverflowShade && r_forwardPlusOverflowShade->value <= 0.0f &&
			!R_ClassicLightingActive() ) {
			R_LatchCvarFloat( r_forwardPlusOverflowShade, "r_forwardPlusOverflowShade", 1.0f );
		}
		if ( r_deferredGBuffer && r_deferredGBuffer->integer &&
			r_deferredLighting && r_deferredLighting->integer ) {
			if ( mode != s_last_logged_mode ) {
				ri.Printf( PRINT_ALL,
					"[VK] r_renderMode 1 + r_deferredLighting 1: deferred opaque + Forward+ transparent "
					"(r_forwardPlusShade=1 on filter 2; r_deferredUnlitBase; same contract as mode 3)\n" );
				s_last_logged_mode = mode;
			}
		} else if ( mode != s_last_logged_mode ) {
			ri.Printf( PRINT_ALL,
				"[VK] r_renderMode 1 (deferred G-buffer). "
				"r_deferredLighting 1 enables opaque deferred + transparent Forward+. vid_restart after latch.\n" );
			s_last_logged_mode = mode;
		}
		break;
	case 2:
		R_LatchCvarInt( r_forwardPlus, "r_forwardPlus", 1 );
		R_LatchCvarInt( r_forwardPlusDepthCull, "r_forwardPlusDepthCull", 1 );
		/* Sidecar G-buffer is optional for mode 2 ("may use"). Forcing fill=1
		 * crashes NVIDIA after gbuffer capture when UI overlay resumes
		 * (vk_begin_ui_overlay_render_pass_load → SIGSEGV in glcore). */
		R_LatchCvarInt( r_deferredLighting, "r_deferredLighting", 0 );
		if ( r_forwardPlusShade && r_forwardPlusShade->value <= 0.0f ) {
			R_LatchCvarFloat( r_forwardPlusShade, "r_forwardPlusShade", 1.0f );
		}
		/* Lights 32-63 are packed on GPU; enable Forward+ overflow shade for parity with deferred. */
		if ( r_forwardPlusOverflowShade && r_forwardPlusOverflowShade->value <= 0.0f &&
			!R_ClassicLightingActive() ) {
			R_LatchCvarFloat( r_forwardPlusOverflowShade, "r_forwardPlusOverflowShade", 1.0f );
		}
		if ( mode != s_last_logged_mode ) {
			ri.Printf( PRINT_ALL,
				"[VK] r_renderMode 2: Forward+ primary (r_forwardPlus=1, r_forwardPlusShade=1, "
				"r_forwardPlusDepthCull=1, sidecar G-buffer opt-in via r_deferredGBuffer, "
				"deferred lighting off; GPU cap %u; classic projector/dlightBits still %d)\n",
				(unsigned)VK_FP_MAX_GPU_LIGHTS, MAX_DLIGHTS );
			s_last_logged_mode = mode;
		}
		break;
	case 3:
		/* Unified Clustered Renderer: deferred opaque + Forward+ transparent, shared light grid. */
		R_LatchCvarInt( r_forwardPlus, "r_forwardPlus", 1 );
		R_LatchCvarInt( r_forwardPlusDepthCull, "r_forwardPlusDepthCull", 1 );
		R_LatchCvarInt( r_deferredGBuffer, "r_deferredGBuffer", 1 );
		R_LatchCvarInt( r_deferredGBufferFill, "r_deferredGBufferFill", 1 );
		R_LatchCvarInt( r_deferredLighting, "r_deferredLighting", 1 );
		R_LatchCvarInt( r_deferredUnlitBase, "r_deferredUnlitBase", 1 );
		if ( r_forwardPlusShade && r_forwardPlusShade->value <= 0.0f ) {
			R_LatchCvarFloat( r_forwardPlusShade, "r_forwardPlusShade", 1.0f );
		}
		if ( r_forwardPlusOverflowShade && r_forwardPlusOverflowShade->value <= 0.0f &&
			!R_ClassicLightingActive() ) {
			R_LatchCvarFloat( r_forwardPlusOverflowShade, "r_forwardPlusOverflowShade", 1.0f );
		}
		/* Z-slices: configs set r_forwardPlusZSlices 8; keep 1 as explicit 2D diagnostic fallback. */
		if ( mode != s_last_logged_mode ) {
			ri.Printf( PRINT_ALL,
				"[VK] Unified Clustered Renderer (r_renderMode 3): hybrid deferred opaque + "
				"Forward+ transparent; shared light grid (GPU cap %u, Z-slices %d)\n",
				(unsigned)VK_FP_MAX_GPU_LIGHTS,
				r_forwardPlusZSlices ? r_forwardPlusZSlices->integer : 1 );
			s_last_logged_mode = mode;
		}
		break;
	default:
		if ( mode != s_last_logged_mode ) {
			s_last_logged_mode = mode;
		}
		break;
	}
}
