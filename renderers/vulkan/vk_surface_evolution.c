/*
===========================================================================
Raster Ultra 1.8 — surface evolution from weather + authoring cvars.
===========================================================================
*/

#include "tr_local.h"
#include "vk_surface_evolution.h"
#include "vk_weather.h"
#include "vk_raster_ultra.h"

static cvar_t *r_surfaceEvolution;
static cvar_t *r_surfaceWetness;
static cvar_t *r_surfaceSnow;
static cvar_t *r_surfaceDust;
static cvar_t *r_surfaceRust;
static cvar_t *r_surfaceSoot;
static cvar_t *r_surfaceMoss;
static cvar_t *r_surfaceDamage;
static cvar_t *r_surfaceDebug;
static qboolean s_cmds;
static vkSurfaceEvolution_t s_state;

void vk_surface_evolution_register_cvars( void )
{
	if ( r_surfaceEvolution ) {
		return;
	}
	r_surfaceEvolution = ri.Cvar_Get( "r_surfaceEvolution", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_surfaceEvolution, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_surfaceEvolution,
		"Raster Ultra 1.8 surface evolution (latched).\n"
		"Wetness/snow/dust/rust/soot/moss from weather + cvars. Feeds PBR UBO." );
	ri.Cvar_SetGroup( r_surfaceEvolution, CVG_RENDERER );

	r_surfaceWetness = ri.Cvar_Get( "r_surfaceWetness", "-1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_surfaceWetness, "Override wetness (-1 = weather-driven)." );
	r_surfaceSnow = ri.Cvar_Get( "r_surfaceSnow", "-1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_surfaceSnow, "Override snow accumulation (-1 = weather)." );
	r_surfaceDust = ri.Cvar_Get( "r_surfaceDust", "0", CVAR_ARCHIVE_ND );
	r_surfaceRust = ri.Cvar_Get( "r_surfaceRust", "0", CVAR_ARCHIVE_ND );
	r_surfaceSoot = ri.Cvar_Get( "r_surfaceSoot", "0", CVAR_ARCHIVE_ND );
	r_surfaceMoss = ri.Cvar_Get( "r_surfaceMoss", "0", CVAR_ARCHIVE_ND );
	r_surfaceDamage = ri.Cvar_Get( "r_surfaceDamage", "0", CVAR_ARCHIVE_ND );
	r_surfaceDebug = ri.Cvar_Get( "r_surfaceDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_surfaceDebug, "0", "20", CV_INTEGER );
	ri.Cvar_SetDescription( r_surfaceDebug,
		"Surface evolution debug view (0 off). See docs/RASTER_ULTRA_1.8.md." );
}

void vk_surface_evolution_init( void )
{
	vk_surface_evolution_register_cvars();
	Com_Memset( &s_state, 0, sizeof( s_state ) );
	s_state.outdoor = qtrue;
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "surface_evolution_status", vk_surface_evolution_status_f );
		ri.Cmd_AddCommand( "material_reload", vk_surface_evolution_status_f ); /* alias inspect */
		s_cmds = qtrue;
	}
	ri.Printf( PRINT_ALL, "[VK][SurfaceEvolution] %s (wetness/snow/dust/rust/soot/moss)\n",
		( r_surfaceEvolution && r_surfaceEvolution->integer ) ? "enabled" : "off" );
}

void vk_surface_evolution_shutdown( void )
{
	Com_Memset( &s_state, 0, sizeof( s_state ) );
}

qboolean vk_surface_evolution_active( void )
{
	return ( r_surfaceEvolution && r_surfaceEvolution->integer ) ? qtrue : qfalse;
}

const vkSurfaceEvolution_t *vk_surface_evolution_state( void )
{
	return &s_state;
}

void vk_surface_evolution_update( void )
{
	float weatherWet = 0.0f;
	float weatherPuddle = 0.0f;
	float weatherSnow = 0.0f;
	float weatherDust = 0.0f;
	const vkWeatherState_t *ws;

	if ( !vk_surface_evolution_active() ) {
		Com_Memset( &s_state, 0, sizeof( s_state ) );
		return;
	}

	s_state.outdoor = vk_weather_is_outdoor_view();

	if ( vk_weather_active() ) {
		ws = vk_weather_state();
		if ( ws && s_state.outdoor ) {
			weatherWet = ws->wetnessRate;
			weatherPuddle = ws->puddleRate;
			if ( ws->preset == VK_WEATHER_SNOW ) {
				weatherSnow = Com_Clamp( 0.0f, 1.0f, ws->precipitation );
			}
			if ( ws->preset == VK_WEATHER_DUST ) {
				weatherDust = Com_Clamp( 0.0f, 1.0f, ws->precipitation * 0.85f + ws->coverage * 0.15f );
			}
			if ( ws->preset == VK_WEATHER_RAIN || ws->preset == VK_WEATHER_STORM ) {
				weatherWet = Com_Clamp( 0.0f, 1.0f, weatherWet + ws->precipitation * 0.35f );
			}
		}
	}

	if ( r_surfaceWetness && r_surfaceWetness->value >= 0.0f ) {
		s_state.wetness = Com_Clamp( 0.0f, 1.0f, r_surfaceWetness->value );
	} else {
		s_state.wetness = Com_Clamp( 0.0f, 1.0f, weatherWet );
	}
	if ( r_surfaceSnow && r_surfaceSnow->value >= 0.0f ) {
		s_state.snow = Com_Clamp( 0.0f, 1.0f, r_surfaceSnow->value );
	} else {
		s_state.snow = weatherSnow;
	}

	s_state.dust = Com_Clamp( 0.0f, 1.0f,
		( r_surfaceDust && r_surfaceDust->value > 0.0f ) ? r_surfaceDust->value : weatherDust );
	s_state.rust = Com_Clamp( 0.0f, 1.0f, r_surfaceRust ? r_surfaceRust->value : 0.0f );
	s_state.soot = Com_Clamp( 0.0f, 1.0f, r_surfaceSoot ? r_surfaceSoot->value : 0.0f );
	s_state.moss = Com_Clamp( 0.0f, 1.0f, r_surfaceMoss ? r_surfaceMoss->value : 0.0f );
	s_state.damage = Com_Clamp( 0.0f, 1.0f, r_surfaceDamage ? r_surfaceDamage->value : 0.0f );
	s_state.puddle = Com_Clamp( 0.0f, 1.0f, weatherPuddle );

	/* Indoor: suppress outdoor deposition. */
	if ( !s_state.outdoor ) {
		s_state.snow *= 0.05f;
		s_state.dust *= 0.15f;
		s_state.wetness *= 0.25f;
		s_state.puddle = 0.0f;
	}
}

void vk_surface_evolution_fill_ubo( vec4_t out )
{
	float dustSoot;

	if ( !out ) {
		return;
	}
	if ( !vk_surface_evolution_active() ) {
		Vector4Set( out, 0.0f, 0.0f, 0.0f, 0.0f );
		return;
	}
	dustSoot = Com_Clamp( 0.0f, 1.0f, s_state.dust + s_state.soot * 0.5f );
	Vector4Set( out,
		s_state.wetness,
		s_state.snow,
		dustSoot,
		Com_Clamp( 0.0f, 1.0f, s_state.rust * 0.65f + s_state.moss * 0.35f + s_state.damage * 0.25f ) );
}

void vk_surface_evolution_status_f( void )
{
	ri.Printf( PRINT_ALL, "=== Surface Evolution (Raster Ultra 1.8) ===\n" );
	ri.Printf( PRINT_ALL, "active     : %s\n", vk_surface_evolution_active() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "outdoor    : %s\n", s_state.outdoor ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "wetness    : %.3f puddle=%.3f\n", s_state.wetness, s_state.puddle );
	ri.Printf( PRINT_ALL, "snow       : %.3f dust=%.3f\n", s_state.snow, s_state.dust );
	ri.Printf( PRINT_ALL, "rust       : %.3f soot=%.3f moss=%.3f damage=%.3f\n",
		s_state.rust, s_state.soot, s_state.moss, s_state.damage );
	ri.Printf( PRINT_ALL, "debug      : %d\n", r_surfaceDebug ? r_surfaceDebug->integer : 0 );
	ri.Printf( PRINT_ALL, "policy     : not perfect mirrors; rust reduces metallic; indoor suppress\n" );
}
