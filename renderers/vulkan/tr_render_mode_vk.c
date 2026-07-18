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
		if ( r_deferredGBuffer && r_deferredGBuffer->integer &&
			r_deferredLighting && r_deferredLighting->integer ) {
			R_LatchCvarInt( r_forwardPlus, "r_forwardPlus", 1 );
			if ( r_forwardPlusShade && r_forwardPlusShade->value > 0.0f ) {
				R_LatchCvarFloat( r_forwardPlusShade, "r_forwardPlusShade", 0.0f );
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
		R_LatchCvarInt( r_forwardPlus, "r_forwardPlus", 1 );
		if ( r_forwardPlusShade && r_forwardPlusShade->value <= 0.0f ) {
			R_LatchCvarFloat( r_forwardPlusShade, "r_forwardPlusShade", 1.0f );
		}
		if ( mode != s_last_logged_mode ) {
			ri.Printf( PRINT_ALL,
				"[VK] r_renderMode 2: Forward+ (r_forwardPlus=1, r_forwardPlusShade=1, GPU cap %u; "
				"classic projector/dlightBits still %d)\n",
				(unsigned)VK_FP_MAX_GPU_LIGHTS, MAX_DLIGHTS );
			s_last_logged_mode = mode;
		}
		break;
	case 3:
		/* Unified Clustered Renderer: deferred opaque + Forward+ transparent, shared tiles. */
		R_LatchCvarInt( r_forwardPlus, "r_forwardPlus", 1 );
		R_LatchCvarInt( r_deferredGBuffer, "r_deferredGBuffer", 1 );
		R_LatchCvarInt( r_deferredGBufferFill, "r_deferredGBufferFill", 1 );
		R_LatchCvarInt( r_deferredLighting, "r_deferredLighting", 1 );
		R_LatchCvarInt( r_deferredUnlitBase, "r_deferredUnlitBase", 1 );
		if ( r_forwardPlusShade && r_forwardPlusShade->value <= 0.0f ) {
			R_LatchCvarFloat( r_forwardPlusShade, "r_forwardPlusShade", 1.0f );
		}
		if ( mode != s_last_logged_mode ) {
			ri.Printf( PRINT_ALL,
				"[VK] Unified Clustered Renderer (r_renderMode 3): hybrid deferred opaque + "
				"Forward+ transparent; shared tile lists (GPU cap %u)\n",
				(unsigned)VK_FP_MAX_GPU_LIGHTS );
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
