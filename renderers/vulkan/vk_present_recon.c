/*
===========================================================================
Present-Time Adaptive Reconstruction (Raster Ultra 1.5)

Current-frame-first AA/reconstruction with optional temporal evidence.
No frame generation. No intentional presentation delay. Weapon/UI outside
world history. Latency counters report presentation_source = current sim frame.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_aa_policy.h"
#include "vk_present_recon.h"
#include "vk_temporal.h"

cvar_t *r_presentAdaptiveBudget;
cvar_t *r_presentAdaptiveSpatial;
cvar_t *r_presentAdaptiveHistoryCap;
cvar_t *r_debugAdaptiveSampleMask;

static int s_frames_in_flight_est;
static int s_recon_frames;
static uint64_t s_cpu_begin_ms;
static uint64_t s_last_submit_ms;
static uint64_t s_last_present_ms;
static qboolean s_logged_startup;

static uint64_t vk_present_recon_now_ms( void )
{
	return (uint64_t)ri.Milliseconds();
}

qboolean vk_present_recon_active( void )
{
	if ( !vk.fboActive ) {
		return qfalse;
	}
	if ( r_aaMode && r_aaMode->integer == 3 ) {
		return qtrue;
	}
	if ( r_presentAdaptiveRecon && r_presentAdaptiveRecon->integer &&
		vk_temporal_reconstruction_wanted() ) {
		return qtrue;
	}
	return qfalse;
}

qboolean vk_present_recon_wants_adaptive( void )
{
	return vk_present_recon_active();
}

void vk_present_recon_register_cvars( void )
{
	r_presentAdaptiveBudget = ri.Cvar_Get( "r_presentAdaptiveBudget", "0.15", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_presentAdaptiveBudget, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_presentAdaptiveBudget,
		"Fraction of difficult pixels eligible for extra current-frame spatial samples "
		"(Present-Time Adaptive Reconstruction). 0 = confidence reject only; "
		"default 0.15 keeps cost bounded (no full-frame supersample)." );
	ri.Cvar_SetGroup( r_presentAdaptiveBudget, CVG_RENDERER );

	r_presentAdaptiveSpatial = ri.Cvar_Get( "r_presentAdaptiveSpatial", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_presentAdaptiveSpatial, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_presentAdaptiveSpatial,
		"When adaptive recon rejects history, blend an edge-aware current-frame neighborhood "
		"(SMAA-class spatial fallback) instead of raw 1x. Does not run full-frame SMAA over TAA." );
	ri.Cvar_SetGroup( r_presentAdaptiveSpatial, CVG_RENDERER );

	r_presentAdaptiveHistoryCap = ri.Cvar_Get( "r_presentAdaptiveHistoryCap", "0.42", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_presentAdaptiveHistoryCap, "0", "0.85", CV_FLOAT );
	ri.Cvar_SetDescription( r_presentAdaptiveHistoryCap,
		"Hard max history weight for Present-Time Adaptive Reconstruction (mode 3). "
		"Lower than Temporal Reconstruction (mode 4) to kill trails; current frame remains authoritative." );
	ri.Cvar_SetGroup( r_presentAdaptiveHistoryCap, CVG_RENDERER );

	r_debugAdaptiveSampleMask = ri.Cvar_Get( "r_debugAdaptiveSampleMask", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_debugAdaptiveSampleMask, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_debugAdaptiveSampleMask,
		"Debug: show adaptive current-frame sample mask (also r_debugHistoryRejection 9)." );
	ri.Cvar_SetGroup( r_debugAdaptiveSampleMask, CVG_RENDERER );
}

void vk_present_recon_init( void )
{
	s_frames_in_flight_est = 2;
	s_recon_frames = 0;
	s_cpu_begin_ms = 0;
	s_last_submit_ms = 0;
	s_last_present_ms = 0;
	s_logged_startup = qfalse;

	if ( !s_logged_startup ) {
		ri.Printf( PRINT_ALL,
			"[VK][PresentRecon] Raster Ultra 1.5 ready: "
			"frame_generation=off interpolated_frames=0 "
			"presentation_source=current_simulation_frame "
			"(r_aaMode 3 or r_presentAdaptiveRecon 1)\n" );
		s_logged_startup = qtrue;
	}
}

void vk_present_recon_shutdown( void )
{
	s_recon_frames = 0;
}

void vk_present_recon_begin_frame( void )
{
	s_cpu_begin_ms = vk_present_recon_now_ms();
	if ( vk_present_recon_active() ) {
		s_recon_frames++;
	}
}

void vk_present_recon_note_gpu_submit( void )
{
	s_last_submit_ms = vk_present_recon_now_ms();
}

void vk_present_recon_note_present( void )
{
	s_last_present_ms = vk_present_recon_now_ms();
}

/*
===============
vk_present_recon_status_f

Latency / ownership report for Present-Time Adaptive Reconstruction.
Does not claim exact input-to-photon latency without platform support.
===============
*/
void vk_present_recon_status_f( void )
{
	const qboolean active = vk_present_recon_active();
	const qboolean adaptive = vk_present_recon_wants_adaptive();
	const int aa = r_aaMode ? r_aaMode->integer : -1;
	cvar_t *swapInterval = ri.Cvar_Get( "r_swapInterval", "0", 0 );

	ri.Printf( PRINT_ALL, "======== Present-Time Adaptive Reconstruction ========\n" );
	ri.Printf( PRINT_ALL, "active              : %s (adaptive=%s aaMode=%d presentAdaptiveRecon=%d)\n",
		active ? "yes" : "no",
		adaptive ? "yes" : "no",
		aa,
		r_presentAdaptiveRecon ? r_presentAdaptiveRecon->integer : 0 );
	ri.Printf( PRINT_ALL, "frame_generation    : off\n" );
	ri.Printf( PRINT_ALL, "interpolated_frames : 0\n" );
	ri.Printf( PRINT_ALL, "presentation_source : current_simulation_frame\n" );
	ri.Printf( PRINT_ALL, "intentional_delay   : none (no +1 frame presentation hold)\n" );
	ri.Printf( PRINT_ALL, "weapon_policy       : outside world history (r_temporalWeaponAfterTaa=%d)\n",
		r_temporalWeaponAfterTaa ? r_temporalWeaponAfterTaa->integer : 0 );
	ri.Printf( PRINT_ALL, "history_cap         : %.2f (budget=%.2f spatial=%d)\n",
		r_presentAdaptiveHistoryCap ? r_presentAdaptiveHistoryCap->value : 0.42f,
		r_presentAdaptiveBudget ? r_presentAdaptiveBudget->value : 0.15f,
		r_presentAdaptiveSpatial ? r_presentAdaptiveSpatial->integer : 0 );
	ri.Printf( PRINT_ALL, "present_hint        : r_swapInterval=%d swapchain_images=%u\n",
		swapInterval ? swapInterval->integer : 0,
		vk.swapchain_image_count );
	ri.Printf( PRINT_ALL, "frames_in_flight    : ~%d (estimate; not input-to-photon)\n",
		s_frames_in_flight_est > 0 ? s_frames_in_flight_est : (int)vk.swapchain_image_count );
	ri.Printf( PRINT_ALL, "cpu_begin_ms        : %llu\n", (unsigned long long)s_cpu_begin_ms );
	ri.Printf( PRINT_ALL, "last_submit_ms      : %llu\n", (unsigned long long)s_last_submit_ms );
	ri.Printf( PRINT_ALL, "last_present_ms     : %llu\n", (unsigned long long)s_last_present_ms );
	ri.Printf( PRINT_ALL, "recon_frames        : %d\n", s_recon_frames );
	ri.Printf( PRINT_ALL, "smaa_role           : certified zero-history (aaMode 2); "
		"adaptive uses spatial fallback on reject, not full-frame post-TAA SMAA\n" );
	ri.Printf( PRINT_ALL, "======================================================\n" );
}

/*
===============
vk_motion_vector_cert_status_f

Motion-vector certification dump (units, sign, convention, gaps).
===============
*/
void vk_motion_vector_cert_status_f( void )
{
	ri.Printf( PRINT_ALL, "======== Motion Vector Certification ========\n" );
	ri.Printf( PRINT_ALL, "format              : R16G16_SFLOAT (main-pass attachment)\n" );
	ri.Printf( PRINT_ALL, "units               : normalized UV delta (not pixels)\n" );
	ri.Printf( PRINT_ALL, "encoding            : out_motion = currUV - prevUV\n" );
	ri.Printf( PRINT_ALL, "sign                : +X right, +Y down in UV; historyUV = sampleUV - motion\n" );
	ri.Printf( PRINT_ALL, "convention          : current-to-previous (forward velocity in UV)\n" );
	ri.Printf( PRINT_ALL, "jitter              : postfx.lutParams.zw subtracted from sample UV when temporal upscale jitter active\n" );
	ri.Printf( PRINT_ALL, "resolution          : written at render resolution; TAA samples same space (upscale recon scales UVs)\n" );
	ri.Printf( PRINT_ALL, "invalid             : NaN/Inf → matrix reprojection fallback; OOB history UV → current only\n" );
	ri.Printf( PRINT_ALL, "--- coverage ---\n" );
	ri.Printf( PRINT_ALL, "camera              : CERT (prevViewProj + entity/world MVP)\n" );
	ri.Printf( PRINT_ALL, "rigid models        : CERT (entity model matrix prev/curr)\n" );
	ri.Printf( PRINT_ALL, "movers              : CERT when entity transform tracked\n" );
	ri.Printf( PRINT_ALL, "skinned             : PARTIAL (GPU deform / CPU-skin prev via r_temporalCpuSkinPrev)\n" );
	ri.Printf( PRINT_ALL, "vertex animation    : PARTIAL (frame/oldframe; soft-unreliable if mismatch)\n" );
	ri.Printf( PRINT_ALL, "particles           : WEAK / reactive reject (prefer current)\n" );
	ri.Printf( PRINT_ALL, "water / refractive  : WEAK (distortion + reactive; do not trust for history)\n" );
	ri.Printf( PRINT_ALL, "scrolling / anim UV : UNRELIABLE for surface flow (geo MV only)\n" );
	ri.Printf( PRINT_ALL, "foliage / alpha-test: PARTIAL; high variance → adaptive spatial\n" );
	ri.Printf( PRINT_ALL, "decals              : PARTIAL (follow parent geo when stamped)\n" );
	ri.Printf( PRINT_ALL, "weapon              : OUTSIDE world history (deferred after recon)\n" );
	ri.Printf( PRINT_ALL, "portals / mirrors   : RESET / separate view family (no main-world history reuse)\n" );
	ri.Printf( PRINT_ALL, "UI                  : no world MV; no world jitter; no recon\n" );
	ri.Printf( PRINT_ALL, "debug               : r_debugMotionVectors 1; r_debugHistoryRejection 1/6\n" );
	ri.Printf( PRINT_ALL, "policy              : unavailable/unreliable MV → reject or soft-cap history\n" );
	ri.Printf( PRINT_ALL, "==============================================\n" );
}
