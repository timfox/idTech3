/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Apply latched r_renderMode to Vulkan lighting path (forward / deferred / Forward+).
===========================================================================
*/

#include "tr_local.h"
#include "vk_forward_plus.h"

void R_ApplyRenderModeLatch( void )
{
	int mode;
	static int s_last_logged_mode = -1;

	if ( !r_renderMode ) {
		return;
	}

	mode = r_renderMode->integer;
	if ( mode < 0 || mode > 2 ) {
		mode = 0;
	}

	switch ( mode ) {
	case 1:
		if ( r_deferredGBuffer && r_deferredGBuffer->integer &&
			r_deferredLighting && r_deferredLighting->integer ) {
			if ( r_forwardPlus && !r_forwardPlus->integer ) {
				ri.Cvar_Set( "r_forwardPlus", "1" );
				r_forwardPlus->integer = 1;
				r_forwardPlus->modified = qtrue;
			}
			if ( r_forwardPlusShade && r_forwardPlusShade->value > 0.0f ) {
				ri.Cvar_Set( "r_forwardPlusShade", "0" );
				r_forwardPlusShade->value = 0.0f;
				r_forwardPlusShade->modified = qtrue;
			}
			if ( mode != s_last_logged_mode ) {
				ri.Printf( PRINT_ALL,
					"[VK] r_renderMode 1 + r_deferredLighting 1: deferred diffuse (r_forwardPlus=1, "
					"r_forwardPlusShade=0, r_deferredUnlitBase additive; G-buffer fill required)\n" );
				s_last_logged_mode = mode;
			}
		} else if ( mode != s_last_logged_mode ) {
			ri.Printf( PRINT_ALL,
				"[VK] r_renderMode 1 (deferred G-buffer scaffold). "
				"r_deferredGBuffer 1 + r_deferredGBufferFill 1 capture RTs; "
				"r_deferredLighting 1 enables experimental diffuse. vid_restart after latch.\n" );
			s_last_logged_mode = mode;
		}
		break;
	case 2:
		if ( r_forwardPlus && !r_forwardPlus->integer ) {
			ri.Cvar_Set( "r_forwardPlus", "1" );
			r_forwardPlus->integer = 1;
			r_forwardPlus->modified = qtrue;
		}
		if ( r_forwardPlusShade && r_forwardPlusShade->value <= 0.0f ) {
			ri.Cvar_Set( "r_forwardPlusShade", "1" );
			r_forwardPlusShade->value = 1.0f;
			r_forwardPlusShade->modified = qtrue;
		}
		if ( mode != s_last_logged_mode ) {
			ri.Printf( PRINT_ALL,
				"[VK] r_renderMode 2: Forward+ (r_forwardPlus=1, r_forwardPlusShade=1, GPU cap %u; "
				"classic projector/dlightBits still %d)\n",
				(unsigned)VK_FP_MAX_GPU_LIGHTS, MAX_DLIGHTS );
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
