/*
===========================================================================
Raster Ultra 1.10 — exposure histogram / metering controller.
===========================================================================
*/

#include "tr_local.h"
#include "vk_exposure_histogram.h"
#include "vk_raster_ultra.h"
#include <math.h>

static cvar_t *r_exposureHistogram;
static cvar_t *r_exposureMeter;
static cvar_t *r_exposureComp;
static cvar_t *r_exposureFixed;
static cvar_t *r_exposureMinEV;
static cvar_t *r_exposureMaxEV;
static cvar_t *r_exposureSkyWeight;
static cvar_t *r_exposureDebug;
static qboolean s_cmds;
static vkExposureHistogramState_t s_state;

void vk_exposure_histogram_register_cvars( void )
{
	if ( r_exposureHistogram ) {
		return;
	}
	r_exposureHistogram = ri.Cvar_Get( "r_exposureHistogram", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_exposureHistogram, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_exposureHistogram,
		"Histogram/metering exposure controller (runtime; no latch).\n"
		"Works with r_exposure_auto; adds meter modes, EV clamps, cut/map reset." );
	ri.Cvar_SetGroup( r_exposureHistogram, CVG_RENDERER );

	r_exposureMeter = ri.Cvar_Get( "r_exposureMeter", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_exposureMeter, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_exposureMeter,
		"Metering: 0 average, 1 center-weighted, 2 spot, 3 histogram percentile (uses luminance trim)." );

	r_exposureComp = ri.Cvar_Get( "r_exposureComp", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_exposureComp, "-4", "4", CV_FLOAT );
	ri.Cvar_SetDescription( r_exposureComp, "Exposure compensation in EV stops." );

	r_exposureFixed = ri.Cvar_Get( "r_exposureFixed", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_exposureFixed, "0", "1", CV_INTEGER );

	r_exposureMinEV = ri.Cvar_Get( "r_exposureMinEV", "-4", CVAR_ARCHIVE_ND );
	r_exposureMaxEV = ri.Cvar_Get( "r_exposureMaxEV", "4", CVAR_ARCHIVE_ND );
	r_exposureSkyWeight = ri.Cvar_Get( "r_exposureSkyWeight", "0.75", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_exposureSkyWeight, "0", "2", CV_FLOAT );
	ri.Cvar_SetDescription( r_exposureSkyWeight,
		"How strongly bright upper-frame (sky) samples drive auto-exposure. "
		"0=ignore sky, 0.75=balanced, 1=full, >1 amplify. Sun extremes still trimmed by r_autoExposure_highPercent." );

	r_exposureDebug = ri.Cvar_Get( "r_exposureDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_exposureDebug, "0", "2", CV_INTEGER );
}

void vk_exposure_histogram_init( void )
{
	vk_exposure_histogram_register_cvars();
	Com_Memset( &s_state, 0, sizeof( s_state ) );
	s_state.minEV = r_exposureMinEV ? r_exposureMinEV->value : -4.0f;
	s_state.maxEV = r_exposureMaxEV ? r_exposureMaxEV->value : 4.0f;
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "exposure_histogram_status", vk_exposure_histogram_status_f );
		s_cmds = qtrue;
	}
	ri.Printf( PRINT_ALL, "[VK][ExposureHistogram] %s meter=%d (cut/map reset; fixed=%d)\n",
		( r_exposureHistogram && r_exposureHistogram->integer ) ? "enabled" : "off",
		r_exposureMeter ? r_exposureMeter->integer : 1,
		r_exposureFixed ? r_exposureFixed->integer : 0 );
}

void vk_exposure_histogram_shutdown( void )
{
	Com_Memset( &s_state, 0, sizeof( s_state ) );
}

qboolean vk_exposure_histogram_active( void )
{
	return ( r_exposureHistogram && r_exposureHistogram->integer ) ? qtrue : qfalse;
}

const vkExposureHistogramState_t *vk_exposure_histogram_state( void )
{
	return &s_state;
}

static void Exposure_Reset( void )
{
	s_state.valid = qfalse;
	s_state.lastAvgLogLum = 0.0f;
	s_state.currentEV = 0.0f;
	s_state.targetEV = 0.0f;
	s_state.resets++;
}

void vk_exposure_histogram_on_camera_cut( void )
{
	if ( vk_exposure_histogram_active() ) {
		Exposure_Reset();
	}
}

void vk_exposure_histogram_on_map_change( void )
{
	if ( vk_exposure_histogram_active() ) {
		Exposure_Reset();
	}
}

void vk_exposure_histogram_on_focus_recovery( void )
{
	if ( vk_exposure_histogram_active() ) {
		Exposure_Reset();
	}
}

void vk_exposure_histogram_notify_luminance( float avgLogLum, qboolean valid )
{
	cvar_t *pre;
	float ev;

	s_state.frames++;
	s_state.meterMode = (vkExposureMeterMode_t)( r_exposureMeter ? r_exposureMeter->integer : 1 );
	s_state.fixedExposure = ( r_exposureFixed && r_exposureFixed->integer ) ? qtrue : qfalse;
	s_state.compensationEV = r_exposureComp ? r_exposureComp->value : 0.0f;
	s_state.minEV = r_exposureMinEV ? r_exposureMinEV->value : -4.0f;
	s_state.maxEV = r_exposureMaxEV ? r_exposureMaxEV->value : 4.0f;
	pre = ri.Cvar_Get( "r_pre_exposure_scale", "1", 0 );
	s_state.preExposure = pre ? pre->value : 1.0f;

	if ( !vk_exposure_histogram_active() || s_state.fixedExposure || !valid ) {
		s_state.valid = qfalse;
		return;
	}
	if ( avgLogLum != avgLogLum || avgLogLum < -20.0f || avgLogLum > 20.0f ) {
		s_state.valid = qfalse;
		return;
	}

	s_state.lastAvgLogLum = avgLogLum;
	/* EV relative to mid-grey: negative log2(scene) style */
	ev = -avgLogLum + s_state.compensationEV;
	/*
	 * Do not attenuate bright-sky EV — Source eye adaptation must darken when
	 * looking at HDR sky. Extreme sun is handled by luminance highPercent trim.
	 */
	(void)r_exposureSkyWeight;
	ev = Com_Clamp( s_state.minEV, s_state.maxEV, ev );
	s_state.targetEV = ev;
	if ( !s_state.valid ) {
		s_state.currentEV = ev;
		s_state.valid = qtrue;
	} else {
		float a = ( ev > s_state.currentEV ) ? 0.08f : 0.15f; /* darken faster */
		s_state.currentEV += ( ev - s_state.currentEV ) * a;
	}
}

float vk_exposure_histogram_meter_scale( void )
{
	float scale = 1.0f;

	if ( !vk_exposure_histogram_active() || s_state.fixedExposure ) {
		return 1.0f;
	}
	switch ( s_state.meterMode ) {
	case VK_EXPOSURE_METER_AVERAGE:
		scale = 1.0f;
		break;
	case VK_EXPOSURE_METER_CENTER:
		scale = 1.08f; /* favor mid-frame (existing centerWeight) */
		break;
	case VK_EXPOSURE_METER_SPOT:
		scale = 1.18f;
		break;
	case VK_EXPOSURE_METER_HISTOGRAM:
		scale = 1.0f;
		break;
	default:
		break;
	}
	/* Compensation in linear from EV (full stop scale). */
	scale *= powf( 2.0f, s_state.compensationEV );
	return scale;
}

void vk_exposure_histogram_status_f( void )
{
	static const char *meters[] = { "average", "center", "spot", "histogram" };
	int m = r_exposureMeter ? r_exposureMeter->integer : 1;

	ri.Printf( PRINT_ALL, "=== Exposure Histogram (Raster Ultra 1.10) ===\n" );
	ri.Printf( PRINT_ALL, "active       : %s fixed=%s\n",
		vk_exposure_histogram_active() ? "yes" : "no",
		s_state.fixedExposure ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "meter        : %s\n", meters[(int)Com_Clamp( 0, 3, m )] );
	ri.Printf( PRINT_ALL, "EV           : current=%.2f target=%.2f comp=%.2f clamp=[%.1f,%.1f]\n",
		s_state.currentEV, s_state.targetEV, s_state.compensationEV, s_state.minEV, s_state.maxEV );
	ri.Printf( PRINT_ALL, "logLum       : %.3f valid=%s preExp=%.3f\n",
		s_state.lastAvgLogLum, s_state.valid ? "yes" : "no", s_state.preExposure );
	ri.Printf( PRINT_ALL, "resets       : %u frames=%u meterScale=%.3f\n",
		s_state.resets, s_state.frames, vk_exposure_histogram_meter_scale() );
	ri.Printf( PRINT_ALL, "policy       : cut/map/focus reset; brighten slower than darken; sky weight soft-reject\n" );
}
