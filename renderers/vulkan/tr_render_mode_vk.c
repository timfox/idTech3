/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Apply latched r_renderMode to Vulkan lighting path
(forward / deferred / Forward+ / Unified Clustered / Selective Hybrid / PT Reference).

Spine 1.0 boot remains mode 2 (modern_vulkan_stable.cfg). Modes 4–5 are opt-in.
No frame generation. No intentional one-frame presentation latency.
===========================================================================
*/

#include "tr_local.h"
#include "tr_render_mode_vk.h"
#include "vk_forward_plus.h"

cvar_t *r_presentAdaptiveRecon;

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

static void R_LatchUnifiedClusteredBase( void )
{
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
}

qboolean R_RenderMode_IsCertifiedRaster( void )
{
	return ( r_renderMode && r_renderMode->integer == 2 ) ? qtrue : qfalse;
}

qboolean R_RenderMode_IsUnifiedClustered( void )
{
	const int mode = r_renderMode ? r_renderMode->integer : 0;
	return ( mode == 3 || mode == 4 ) ? qtrue : qfalse;
}

qboolean R_RenderMode_IsSelectiveHybrid( void )
{
	return ( r_renderMode && r_renderMode->integer == 4 ) ? qtrue : qfalse;
}

qboolean R_RenderMode_IsPathTracedReference( void )
{
	return ( r_renderMode && r_renderMode->integer == 5 ) ? qtrue : qfalse;
}

qboolean R_RenderMode_WantsGBuffer( void )
{
	const int mode = r_renderMode ? r_renderMode->integer : 0;
	return ( mode >= 1 && mode <= 5 ) ? qtrue : qfalse;
}

qboolean R_RenderMode_WantsDeferredLighting( void )
{
	const int mode = r_renderMode ? r_renderMode->integer : 0;
	/* Mode 5 keeps deferred scaffold for depth/G-buffer; PT replaces lighting when active. */
	return ( mode == 1 || mode == 3 || mode == 4 || mode == 5 ) ? qtrue : qfalse;
}

qboolean R_RenderMode_WantsOpaqueTransparentSplit( void )
{
	if ( R_RenderMode_IsUnifiedClustered() ) {
		return qtrue;
	}
	if ( r_renderMode && r_renderMode->integer == 1 ) {
		return qtrue;
	}
	/* Mode 5: still split world so depth/G-buffer stay valid for PT guidance. */
	if ( R_RenderMode_IsPathTracedReference() ) {
		return qtrue;
	}
	return qfalse;
}

const char *R_RenderMode_TierName( void )
{
	const int mode = r_renderMode ? r_renderMode->integer : 0;

	switch ( mode ) {
	case 2: return "tier_a_certified_raster";
	case 3: return "unified_clustered_raster";
	case 4: return "tier_b_selective_hybrid";
	case 5: return "tier_c_path_traced_reference";
	case 1: return "deferred_split";
	default: return "classic_forward";
	}
}

qboolean R_PresentAdaptiveRecon_Allowed( void )
{
	/*
	 * Present-time adaptive reconstruction may only adjust same-frame presentation
	 * quality (temporal recon / internal upscale). It must never:
	 *  - insert an interpolated frame between simulation frames
	 *  - add intentional one-frame presentation latency
	 *  - enable frame generation
	 */
	if ( !r_presentAdaptiveRecon || !r_presentAdaptiveRecon->integer ) {
		return qfalse;
	}
	return qtrue;
}

void R_ApplyRenderModeLatch( void )
{
	int mode;
	static int s_last_logged_mode = -1;

	if ( !r_presentAdaptiveRecon ) {
		r_presentAdaptiveRecon = ri.Cvar_Get( "r_presentAdaptiveRecon", "0", CVAR_ARCHIVE_ND );
		ri.Cvar_CheckRange( r_presentAdaptiveRecon, "0", "1", CV_INTEGER );
		ri.Cvar_SetDescription( r_presentAdaptiveRecon,
			"Present-time adaptive reconstruction (Spine 1.2).\n"
			" 0: off (default)\n"
			" 1: allow same-frame temporal recon / internal upscale paths only.\n"
			"Forbidden: frame generation, interpolated sim frames, intentional +1 frame latency." );
	}

	if ( !r_renderMode ) {
		return;
	}

	mode = r_renderMode->integer;
	if ( mode < 0 || mode > 5 ) {
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
				"[VK] r_renderMode 2: Forward+ primary / Tier A Certified Raster "
				"(r_forwardPlus=1, r_forwardPlusShade=1, r_forwardPlusDepthCull=1, "
				"sidecar G-buffer opt-in via r_deferredGBuffer, deferred lighting off; "
				"GPU cap %u; classic projector/dlightBits still %d)\n",
				(unsigned)VK_FP_MAX_GPU_LIGHTS, MAX_DLIGHTS );
			s_last_logged_mode = mode;
		}
		break;
	case 3:
		/* Unified Clustered Renderer: deferred opaque + Forward+ transparent, shared light grid. */
		R_LatchUnifiedClusteredBase();
		if ( mode != s_last_logged_mode ) {
			ri.Printf( PRINT_ALL,
				"[VK] Unified Clustered Renderer (r_renderMode 3): hybrid deferred opaque + "
				"Forward+ transparent; shared light grid (GPU cap %u, Z-slices %d). Opt-in — "
				"Spine 1.0 boot remains mode 2.\n",
				(unsigned)VK_FP_MAX_GPU_LIGHTS,
				r_forwardPlusZSlices ? r_forwardPlusZSlices->integer : 1 );
			s_last_logged_mode = mode;
		}
		break;
	case 4:
		/*
		 * Tier B Selective Hybrid: clustered raster visibility/materials remain primary.
		 * Individual RT signals (shadow/spec/AV/GI/transmission) are opt-in via Hybrid1
		 * overlays — do not auto-latch r_hybrid1 (needs RTX capability).
		 */
		R_LatchUnifiedClusteredBase();
		/* Prefer exclusive hybrid channels over full PT / demo overlays when mode 4. */
		if ( mode != s_last_logged_mode ) {
			ri.Printf( PRINT_ALL,
				"[VK] Spine 1.2 Tier B Selective Hybrid (r_renderMode 4): clustered raster primary; "
				"ray-traced signals are exclusive owners with raster fallbacks. "
				"Enable Hybrid1 via vulkan_overlay_selective_hybrid.cfg (requires RTX). "
				"Recovery: exec modern_vulkan.cfg\n" );
			s_last_logged_mode = mode;
		}
		break;
	case 5:
		/*
		 * Tier C Full Path-Traced Reference: PT is exclusive lighting owner when active.
		 * Keep G-buffer scaffold for depth/motion; do not auto-latch r_pathtrace
		 * (explicit overlay + latched RT cvars). Force full replace composite when PT on.
		 */
		R_LatchUnifiedClusteredBase();
		if ( r_pathtrace && r_pathtrace->integer > 0 ) {
			R_LatchCvarFloat( r_pathtrace_composite, "r_pathtrace_composite", 1.0f );
		}
		if ( mode != s_last_logged_mode ) {
			ri.Printf( PRINT_ALL,
				"[VK] Spine 1.2 Tier C Path-Traced Reference (r_renderMode 5): exclusive PT lighting "
				"when r_pathtrace 1 (not an additive overlay). Use vulkan_overlay_pt_reference.cfg. "
				"Not for normal gameplay. Recovery: exec modern_vulkan.cfg\n" );
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
