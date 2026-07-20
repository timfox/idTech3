/*
===========================================================================
Raster Ultra 1.7 — volumetric clouds + dedicated history + cloud shadows.

Raster-only. History ownership is separate from world TAA / present recon.
Cloud shadows modulate sun lighting only.
===========================================================================
*/

#include "tr_local.h"
#include "vk_volumetric_clouds.h"
#include "vk_weather.h"
#include "vk_sky_owner.h"
#include "vk_temporal.h"

static cvar_t *r_volumetricClouds;
static cvar_t *r_volumetricCloudsCoverage;
static cvar_t *r_volumetricCloudsAltitude;
static cvar_t *r_volumetricCloudsThickness;
static cvar_t *r_volumetricCloudsWind;
static cvar_t *r_cloudShadows;
static cvar_t *r_cloudShadowStrength;
static cvar_t *r_cloudHistory;
static cvar_t *r_cloudsDebug;

static uint32_t s_historyGeneration;
static uint32_t s_historyAge;
static qboolean s_historyValid;
static float s_lastSunDot;
static float s_windOffset;
static qboolean s_cmds;
static uint32_t s_frames;

void vk_volumetric_clouds_register_cvars( void )
{
	if ( r_volumetricClouds ) {
		return;
	}
	r_volumetricClouds = ri.Cvar_Get( "r_volumetricClouds", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_volumetricClouds, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricClouds,
		"Raster Ultra 1.7 volumetric clouds (latched, no RT).\n"
		" 0 off (default)\n"
		" 1 enable cloud layer + dedicated history + optional cloud shadows\n"
		"Not a scrolling 2D sky shell — density profile + weather coverage." );
	ri.Cvar_SetGroup( r_volumetricClouds, CVG_RENDERER );

	r_volumetricCloudsCoverage = ri.Cvar_Get( "r_volumetricCloudsCoverage", "-1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricCloudsCoverage, "-1", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricCloudsCoverage,
		"Cloud coverage override (-1 = use weather controller / default)." );

	r_volumetricCloudsAltitude = ri.Cvar_Get( "r_volumetricCloudsAltitude", "1200", CVAR_ARCHIVE_ND );
	r_volumetricCloudsThickness = ri.Cvar_Get( "r_volumetricCloudsThickness", "600", CVAR_ARCHIVE_ND );
	r_volumetricCloudsWind = ri.Cvar_Get( "r_volumetricCloudsWind", "1", CVAR_ARCHIVE_ND );

	r_cloudShadows = ri.Cvar_Get( "r_cloudShadows", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_cloudShadows, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_cloudShadows,
		"When volumetric clouds on: modulate sun lighting by cloud coverage (not all lights)." );

	r_cloudShadowStrength = ri.Cvar_Get( "r_cloudShadowStrength", "0.65", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_cloudShadowStrength, "0", "1", CV_FLOAT );

	r_cloudHistory = ri.Cvar_Get( "r_cloudHistory", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_cloudHistory, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_cloudHistory,
		"Dedicated cloud temporal history (separate from world TAA). Reject on cuts/weather/sun jumps." );

	r_cloudsDebug = ri.Cvar_Get( "r_cloudsDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_cloudsDebug, "0", "4", CV_INTEGER );
}

void vk_volumetric_clouds_init( void )
{
	vk_volumetric_clouds_register_cvars();
	s_historyGeneration = 1;
	s_historyAge = 0;
	s_historyValid = qfalse;
	s_lastSunDot = 1.0f;
	s_windOffset = 0.0f;
	s_frames = 0;
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "volumetric_clouds_status", vk_volumetric_clouds_status_f );
		s_cmds = qtrue;
	}
	if ( r_volumetricClouds && r_volumetricClouds->integer ) {
		ri.Printf( PRINT_ALL,
			"[VK][Clouds] Raster Ultra 1.7 volumetric clouds ready "
			"(dedicated history; cloud shadows modulate sun only; RT=off)\n" );
	}
}

void vk_volumetric_clouds_shutdown( void )
{
	s_historyValid = qfalse;
}

qboolean vk_volumetric_clouds_active( void )
{
	return ( r_volumetricClouds && r_volumetricClouds->integer ) ? qtrue : qfalse;
}

void vk_volumetric_clouds_on_camera_cut( void )
{
	s_historyValid = qfalse;
	s_historyAge = 0;
	s_historyGeneration++;
}

void vk_volumetric_clouds_on_weather_change( void )
{
	/* Prefer noise over persistent trails on rapid weather transitions. */
	s_historyValid = qfalse;
	s_historyAge = 0;
	s_historyGeneration++;
}

static float Clouds_Coverage( void )
{
	float c;

	if ( r_volumetricCloudsCoverage && r_volumetricCloudsCoverage->value >= 0.0f ) {
		c = r_volumetricCloudsCoverage->value;
	} else if ( vk_weather_active() ) {
		c = vk_weather_cloud_coverage();
	} else {
		c = 0.35f;
	}
	return Com_Clamp( 0.0f, 1.0f, c );
}

void vk_volumetric_clouds_begin_frame( void )
{
	float sunDot;
	vec3_t sun;

	if ( !vk_volumetric_clouds_active() ) {
		return;
	}

	s_frames++;
	if ( r_volumetricCloudsWind && r_volumetricCloudsWind->integer ) {
		s_windOffset += 0.0025f * ( vk_weather_active() ? ( 0.5f + vk_weather_state()->wind ) : 1.0f );
	}

	VectorCopy( tr.sunDirection, sun );
	if ( VectorLength( sun ) < 1e-4f ) {
		sun[0] = 0.3f; sun[1] = 0.8f; sun[2] = 0.5f;
		VectorNormalize( sun );
	}
	sunDot = DotProduct( sun, sun ); /* placeholder vs previous — use stored */
	{
		static vec3_t s_prevSun;
		sunDot = DotProduct( sun, s_prevSun );
		if ( s_prevSun[0] == 0.0f && s_prevSun[1] == 0.0f && s_prevSun[2] == 0.0f ) {
			sunDot = 1.0f;
		}
		VectorCopy( sun, s_prevSun );
	}
	s_lastSunDot = sunDot;

	if ( r_cloudHistory && r_cloudHistory->integer ) {
		if ( sunDot < 0.985f ) {
			/* Sun direction jumped — reject cloud history. */
			vk_volumetric_clouds_on_camera_cut();
		} else if ( s_historyValid ) {
			s_historyAge++;
		} else {
			s_historyValid = qtrue;
			s_historyAge = 1;
		}
	} else {
		s_historyValid = qfalse;
		s_historyAge = 0;
	}
}

float vk_volumetric_clouds_sun_shadow_factor( void )
{
	float coverage;
	float strength;
	float factor;

	if ( !vk_volumetric_clouds_active() ) {
		return 1.0f;
	}
	if ( !r_cloudShadows || !r_cloudShadows->integer ) {
		return 1.0f;
	}
	if ( vk_weather_active() && !vk_weather_is_outdoor_view() ) {
		return 1.0f; /* indoor exclusion */
	}

	coverage = Clouds_Coverage();
	strength = r_cloudShadowStrength ? r_cloudShadowStrength->value : 0.65f;
	/* Also fold weather sun visibility. */
	factor = 1.0f - coverage * Com_Clamp( 0.0f, 1.0f, strength );
	factor *= vk_weather_sun_visibility();
	return Com_Clamp( 0.05f, 1.0f, factor );
}

void vk_volumetric_clouds_status_f( void )
{
	ri.Printf( PRINT_ALL, "======== Volumetric Clouds (Raster Ultra 1.7) ========\n" );
	ri.Printf( PRINT_ALL, "active          : %s\n", vk_volumetric_clouds_active() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "coverage        : %.2f\n", Clouds_Coverage() );
	ri.Printf( PRINT_ALL, "altitude/thick  : %.0f / %.0f\n",
		r_volumetricCloudsAltitude ? r_volumetricCloudsAltitude->value : 0.0f,
		r_volumetricCloudsThickness ? r_volumetricCloudsThickness->value : 0.0f );
	ri.Printf( PRINT_ALL, "windOffset      : %.3f\n", s_windOffset );
	ri.Printf( PRINT_ALL, "cloudShadows    : %s strength=%.2f sunFactor=%.2f\n",
		( r_cloudShadows && r_cloudShadows->integer ) ? "on" : "off",
		r_cloudShadowStrength ? r_cloudShadowStrength->value : 0.0f,
		vk_volumetric_clouds_sun_shadow_factor() );
	ri.Printf( PRINT_ALL, "history         : valid=%s age=%u gen=%u (dedicated; not world TAA)\n",
		s_historyValid ? "yes" : "no", s_historyAge, s_historyGeneration );
	ri.Printf( PRINT_ALL, "sunDot(prev)    : %.4f\n", s_lastSunDot );
	ri.Printf( PRINT_ALL, "frames          : %u\n", s_frames );
	ri.Printf( PRINT_ALL, "policy          : no RT; prefer noise over trails; indoor excludes shadows\n" );
	ri.Printf( PRINT_ALL, "======================================================\n" );
}
