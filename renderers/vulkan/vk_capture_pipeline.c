/*
===========================================================================
Raster Ultra 1.10 — capture color-space policy.
===========================================================================
*/

#include "tr_local.h"
#include "vk_capture_pipeline.h"
#include "vk_raster_ultra.h"

static cvar_t *r_captureColorSpace;
static cvar_t *r_captureIncludeUI;
static cvar_t *r_captureDeterministic;
static cvar_t *r_captureBlockHdrToSdr;
static qboolean s_cmds;
static vkCapturePipelineState_t s_state;

void vk_capture_pipeline_register_cvars( void )
{
	if ( r_captureColorSpace ) {
		return;
	}
	r_captureColorSpace = ri.Cvar_Get( "r_captureColorSpace", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_captureColorSpace, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_captureColorSpace,
		"Raster Ultra 1.10 capture color space:\n"
		" 0 display (post-tonemap SDR — default screenshots)\n"
		" 1 pre-tonemap HDR intent (requires HDR export path)\n"
		" 2 scene-linear EXR intent\n"
		" 3 HDR display (PQ/scRGB)\n"
		"Modes 1–3 refuse silent 8-bit SDR encodes when r_captureBlockHdrToSdr 1." );
	ri.Cvar_SetGroup( r_captureColorSpace, CVG_RENDERER );

	r_captureIncludeUI = ri.Cvar_Get( "r_captureIncludeUI", "1", CVAR_ARCHIVE_ND );
	r_captureDeterministic = ri.Cvar_Get( "r_captureDeterministic", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_captureDeterministic,
		"Freeze exposure adaptation + film grain for repeatable captures." );
	r_captureBlockHdrToSdr = ri.Cvar_Get( "r_captureBlockHdrToSdr", "1", CVAR_ARCHIVE_ND );
}

void vk_capture_pipeline_init( void )
{
	vk_capture_pipeline_register_cvars();
	Com_Memset( &s_state, 0, sizeof( s_state ) );
	s_state.warnHdrToSdr = qtrue;
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "capture_pipeline_status", vk_capture_pipeline_status_f );
		s_cmds = qtrue;
	}
	ri.Printf( PRINT_ALL, "[VK][CapturePipeline] colorSpace=%d blockHdrToSdr=%d\n",
		r_captureColorSpace ? r_captureColorSpace->integer : 0,
		r_captureBlockHdrToSdr ? r_captureBlockHdrToSdr->integer : 1 );
}

void vk_capture_pipeline_shutdown( void )
{
}

qboolean vk_capture_pipeline_active( void )
{
	return qtrue; /* policy always available; overlay tunes modes */
}

const vkCapturePipelineState_t *vk_capture_pipeline_state( void )
{
	s_state.colorSpace = (vkCaptureColorSpace_t)( r_captureColorSpace ? r_captureColorSpace->integer : 0 );
	s_state.includeUI = ( !r_captureIncludeUI || r_captureIncludeUI->integer ) ? qtrue : qfalse;
	s_state.deterministicExposure = ( r_captureDeterministic && r_captureDeterministic->integer ) ? qtrue : qfalse;
	s_state.deterministicGrain = s_state.deterministicExposure;
	s_state.warnHdrToSdr = ( !r_captureBlockHdrToSdr || r_captureBlockHdrToSdr->integer ) ? qtrue : qfalse;
	return &s_state;
}

qboolean vk_capture_pipeline_allow_sdr_encode( void )
{
	const vkCapturePipelineState_t *st = vk_capture_pipeline_state();

	if ( st->colorSpace == VK_CAPTURE_DISPLAY ) {
		return qtrue;
	}
	if ( st->warnHdrToSdr ) {
		s_state.blockedSilentHdr++;
		ri.Printf( PRINT_WARNING,
			"[VK][CapturePipeline] refused silent HDR→8-bit SDR (r_captureColorSpace %d). "
			"Set r_captureColorSpace 0 for display SDR, or use HDR/EXR export.\n",
			(int)st->colorSpace );
		return qfalse;
	}
	return qtrue;
}

void vk_capture_pipeline_note_capture( void )
{
	s_state.captures++;
	if ( s_state.deterministicExposure ) {
		ri.Cvar_Set( "r_filmGrain", "0" );
		ri.Cvar_Set( "r_exposureFixed", "1" );
	}
}

void vk_capture_pipeline_status_f( void )
{
	static const char *spaces[] = { "display_sdr", "pre_tonemap_hdr", "scene_linear", "hdr_display" };
	const vkCapturePipelineState_t *st = vk_capture_pipeline_state();

	ri.Printf( PRINT_ALL, "=== Capture Pipeline (Raster Ultra 1.10) ===\n" );
	ri.Printf( PRINT_ALL, "colorSpace     : %s\n", spaces[(int)Com_Clamp( 0, 3, (int)st->colorSpace )] );
	ri.Printf( PRINT_ALL, "includeUI      : %s deterministic=%s\n",
		st->includeUI ? "yes" : "no", st->deterministicExposure ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "captures       : %u blockedSilentHdr=%u\n",
		st->captures, st->blockedSilentHdr );
	ri.Printf( PRINT_ALL, "note           : display path blits swapchain after tonemap+UI; "
		"HDR masters require non-8-bit export\n" );
}
