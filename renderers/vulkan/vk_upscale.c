/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Upscale path: r_upscale 0=off, 1=r_renderScale spatial, 2=FSR2 (experimental spike).
FSR2 requires linked FidelityFX SDK — see docs/RENDERERS_FUTURE.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk_upscale.h"
#include "vk_temporal.h"

static cvar_t *r_upscale;

void R_Upscale_Init( void ) {
	r_upscale = ri.Cvar_Get( "r_upscale", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_upscale, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_upscale,
		"Upscaler: 0=off, 1=renderScale bilinear, 2=FSR2 experimental (motion RT + jitter)." );
	if ( r_upscale->integer == 2 ) {
		ri.Printf( PRINT_ALL,
			"[VK][upscale] r_upscale=2 FSR2 spike — using spatial fallback until SDK wired\n" );
	}
}

qboolean R_Upscale_UseFsr2( void ) {
	return ( r_upscale && r_upscale->integer == 2 ) ? qtrue : qfalse;
}

void R_Upscale_NoteJitter( float *jitterX, float *jitterY ) {
	if ( !R_Upscale_UseFsr2() ) {
		if ( jitterX ) {
			*jitterX = 0.0f;
		}
		if ( jitterY ) {
			*jitterY = 0.0f;
		}
		return;
	}
	/* Halton-style sub-pixel jitter placeholder for motion/TAA when FSR2 enabled */
	if ( jitterX ) {
		*jitterX = 0.25f;
	}
	if ( jitterY ) {
		*jitterY = 0.125f;
	}
}
