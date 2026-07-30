/*
===========================================================================
Raster Ultra 1.7 — weather controller (data-driven presets + transitions).
===========================================================================
*/

#include "tr_local.h"
#include "vk_weather.h"
#include "vk_volumetric_clouds.h"
#include "vk_sky_owner.h"
#include <math.h>

static cvar_t *r_weather;
static cvar_t *r_weatherPreset;
static cvar_t *r_weatherTransition;
static cvar_t *r_weatherIndoor;
static cvar_t *r_weatherSunDim;
static cvar_t *r_weatherShadowDim;
static cvar_t *r_weatherDynamic;
static cvar_t *r_weatherDynamicMinTime;
static cvar_t *r_weatherDynamicMaxTime;
static cvar_t *r_weatherVolatility;
static cvar_t *r_weatherSeed;
static cvar_t *r_weatherLightning;
static cvar_t *r_weatherLightningScale;
static cvar_t *r_weatherDebug;

static vkWeatherState_t s_state;
static vkWeatherState_t s_target;
static float s_blend; /* 0 = current, 1 = fully at target */
static int s_lastPreset = -1;
static int s_seed = 0x577EA7;
static float s_dynamicTimer;
static float s_dynamicNextTime = 60.0f;
static float s_lightningTimer;
static float s_lightningPeak;
static qboolean s_cmds;

static void Weather_ApplyPreset( vkWeatherPreset_t p, vkWeatherState_t *out )
{
	Com_Memset( out, 0, sizeof( *out ) );
	out->preset = p;
	out->sunVisibility = 1.0f;
	out->transition = 1.0f;
	out->indoorSuppress = qtrue;

	switch ( p ) {
	case VK_WEATHER_CLOUDY:
		out->coverage = 0.45f;
		out->fogDensityScale = 1.05f;
		out->aerosol = 1.1f;
		out->sunVisibility = 0.85f;
		out->wind = 0.25f;
		break;
	case VK_WEATHER_OVERCAST:
		out->coverage = 0.85f;
		out->fogDensityScale = 1.25f;
		out->aerosol = 1.35f;
		out->sunVisibility = 0.45f;
		out->wind = 0.35f;
		break;
	case VK_WEATHER_RAIN:
		out->coverage = 0.75f;
		out->precipitation = 0.65f;
		out->fogDensityScale = 1.4f;
		out->aerosol = 1.2f;
		out->wetnessRate = 0.55f;
		out->puddleRate = 0.35f;
		out->sunVisibility = 0.40f;
		out->wind = 0.45f;
		break;
	case VK_WEATHER_STORM:
		out->coverage = 0.95f;
		out->precipitation = 1.0f;
		out->fogDensityScale = 1.8f;
		out->aerosol = 1.6f;
		out->wetnessRate = 0.9f;
		out->puddleRate = 0.7f;
		out->sunVisibility = 0.15f;
		out->wind = 0.85f;
		out->lightningProb = 0.08f;
		break;
	case VK_WEATHER_SNOW:
		out->coverage = 0.70f;
		out->precipitation = 0.55f;
		out->fogDensityScale = 1.3f;
		out->aerosol = 1.15f;
		out->sunVisibility = 0.55f;
		out->wind = 0.30f;
		out->wetnessRate = 0.15f;
		break;
	case VK_WEATHER_DUST:
		out->coverage = 0.25f;
		out->precipitation = 0.0f;
		out->fogDensityScale = 1.6f;
		out->aerosol = 2.0f;
		out->sunVisibility = 0.35f;
		out->wind = 0.70f;
		break;
	case VK_WEATHER_FOG:
		out->coverage = 0.35f;
		out->fogDensityScale = 2.2f;
		out->aerosol = 1.4f;
		out->sunVisibility = 0.25f;
		out->wind = 0.10f;
		break;
	case VK_WEATHER_CLEAR:
	default:
		out->coverage = 0.08f;
		out->fogDensityScale = 1.0f;
		out->aerosol = 1.0f;
		out->sunVisibility = 1.0f;
		out->wind = 0.10f;
		break;
	}
}

static float Weather_Lerp( float a, float b, float t )
{
	return a + ( b - a ) * t;
}

static float Weather_FrameDt( void )
{
	float dt = 1.0f / 60.0f;
	static double s_prev;

	if ( backEnd.refdef.floatTime > 0.0 ) {
		if ( s_prev > 0.0 ) {
			dt = (float)( backEnd.refdef.floatTime - s_prev );
			if ( dt < 0.0f || dt > 0.25f ) {
				dt = 1.0f / 60.0f;
			}
		}
		s_prev = backEnd.refdef.floatTime;
	}
	return dt;
}

static float Weather_Rand01( void )
{
	return Q_random( &s_seed );
}

static void Weather_ResetDynamicTimer( void )
{
	float minTime = r_weatherDynamicMinTime ? r_weatherDynamicMinTime->value : 45.0f;
	float maxTime = r_weatherDynamicMaxTime ? r_weatherDynamicMaxTime->value : 180.0f;

	if ( maxTime < minTime ) {
		maxTime = minTime;
	}
	s_dynamicTimer = 0.0f;
	s_dynamicNextTime = minTime + ( maxTime - minTime ) * Weather_Rand01();
}

static int Weather_ChooseDynamicPreset( int current )
{
	float volatility = r_weatherVolatility ? Com_Clamp( 0.0f, 1.0f, r_weatherVolatility->value ) : 0.35f;
	float r = Weather_Rand01();

	switch ( current ) {
	case VK_WEATHER_CLEAR:
		if ( r < 0.55f - 0.25f * volatility ) {
			return VK_WEATHER_CLOUDY;
		}
		return ( r < 0.85f ) ? VK_WEATHER_FOG : VK_WEATHER_DUST;
	case VK_WEATHER_CLOUDY:
		if ( r < 0.30f ) {
			return VK_WEATHER_CLEAR;
		}
		if ( r < 0.60f + 0.20f * volatility ) {
			return VK_WEATHER_OVERCAST;
		}
		return VK_WEATHER_RAIN;
	case VK_WEATHER_OVERCAST:
		if ( r < 0.25f ) {
			return VK_WEATHER_CLOUDY;
		}
		if ( r < 0.65f ) {
			return VK_WEATHER_RAIN;
		}
		return ( r < 0.80f + 0.15f * volatility ) ? VK_WEATHER_STORM : VK_WEATHER_FOG;
	case VK_WEATHER_RAIN:
		if ( r < 0.25f ) {
			return VK_WEATHER_OVERCAST;
		}
		if ( r < 0.45f + 0.35f * volatility ) {
			return VK_WEATHER_STORM;
		}
		return VK_WEATHER_CLOUDY;
	case VK_WEATHER_STORM:
		return ( r < 0.55f ) ? VK_WEATHER_RAIN : VK_WEATHER_OVERCAST;
	case VK_WEATHER_SNOW:
		return ( r < 0.55f ) ? VK_WEATHER_OVERCAST : VK_WEATHER_FOG;
	case VK_WEATHER_DUST:
		return ( r < 0.50f ) ? VK_WEATHER_CLEAR : VK_WEATHER_CLOUDY;
	case VK_WEATHER_FOG:
	default:
		return ( r < 0.45f ) ? VK_WEATHER_CLEAR : VK_WEATHER_CLOUDY;
	}
}

void vk_weather_register_cvars( void )
{
	if ( r_weather ) {
		return;
	}
	r_weather = ri.Cvar_Get( "r_weather", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_weather, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_weather,
		"Raster Ultra 1.7 weather controller (latched).\n"
		" 0 off (default — classic maps unchanged)\n"
		" 1 enable data-driven weather state / transitions" );
	ri.Cvar_SetGroup( r_weather, CVG_RENDERER );

	r_weatherPreset = ri.Cvar_Get( "r_weatherPreset", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weatherPreset, "0", "7", CV_INTEGER );
	ri.Cvar_SetDescription( r_weatherPreset,
		"Weather preset: 0 clear, 1 cloudy, 2 overcast, 3 rain, 4 storm, 5 snow, 6 dust, 7 fog" );

	r_weatherTransition = ri.Cvar_Get( "r_weatherTransition", "8", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weatherTransition, "0.5", "120", CV_FLOAT );
	ri.Cvar_SetDescription( r_weatherTransition, "Seconds to blend between weather presets." );

	r_weatherIndoor = ri.Cvar_Get( "r_weatherIndoor", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weatherIndoor, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_weatherIndoor,
		"Force indoor classification for weather (suppress precip/cloud shadows). "
		"Auto portal/leaf classification can replace this later." );

	r_weatherSunDim = ri.Cvar_Get( "r_weatherSunDim", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weatherSunDim, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_weatherSunDim,
		"How strongly weather sunVisibility dims canonical world sun radiance." );

	r_weatherShadowDim = ri.Cvar_Get( "r_weatherShadowDim", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weatherShadowDim, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_weatherShadowDim,
		"How strongly weather sunVisibility dims directional sun shadow strength." );

	r_weatherDynamic = ri.Cvar_Get( "r_weatherDynamic", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weatherDynamic, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_weatherDynamic, "Automatically evolve weather presets over time." );

	r_weatherDynamicMinTime = ri.Cvar_Get( "r_weatherDynamicMinTime", "45", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weatherDynamicMinTime, "5", "3600", CV_FLOAT );
	ri.Cvar_SetDescription( r_weatherDynamicMinTime, "Minimum seconds before dynamic weather may transition." );

	r_weatherDynamicMaxTime = ri.Cvar_Get( "r_weatherDynamicMaxTime", "180", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weatherDynamicMaxTime, "5", "7200", CV_FLOAT );
	ri.Cvar_SetDescription( r_weatherDynamicMaxTime, "Maximum seconds before dynamic weather transitions." );

	r_weatherVolatility = ri.Cvar_Get( "r_weatherVolatility", "0.35", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weatherVolatility, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_weatherVolatility, "Bias dynamic weather toward stronger rain/storm transitions." );

	r_weatherSeed = ri.Cvar_Get( "r_weatherSeed", "577", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weatherSeed, "1", "2147483647", CV_INTEGER );
	ri.Cvar_SetDescription( r_weatherSeed, "Deterministic seed for dynamic weather transitions." );

	r_weatherLightning = ri.Cvar_Get( "r_weatherLightning", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weatherLightning, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_weatherLightning, "Enable short lightning flashes for storm/rain dynamic weather." );

	r_weatherLightningScale = ri.Cvar_Get( "r_weatherLightningScale", "1.25", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weatherLightningScale, "0", "8", CV_FLOAT );
	ri.Cvar_SetDescription( r_weatherLightningScale, "Peak canonical sun-light multiplier added during lightning flashes." );

	r_weatherDebug = ri.Cvar_Get( "r_weatherDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_weatherDebug, "0", "2", CV_INTEGER );
}

void vk_weather_init( void )
{
	vk_weather_register_cvars();
	Weather_ApplyPreset( VK_WEATHER_CLEAR, &s_state );
	Weather_ApplyPreset( VK_WEATHER_CLEAR, &s_target );
	s_blend = 1.0f;
	s_lastPreset = 0;
	s_seed = r_weatherSeed ? r_weatherSeed->integer : 0x577EA7;
	Weather_ResetDynamicTimer();
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "weather_status", vk_weather_status_f );
		s_cmds = qtrue;
	}
	if ( r_weather && r_weather->integer ) {
		ri.Printf( PRINT_ALL, "[VK][Weather] Raster Ultra 1.7 weather controller active\n" );
	}
}

void vk_weather_shutdown( void )
{
	Com_Memset( &s_state, 0, sizeof( s_state ) );
	s_lightningTimer = 0.0f;
	s_lightningPeak = 0.0f;
}

qboolean vk_weather_active( void )
{
	return ( r_weather && r_weather->integer ) ? qtrue : qfalse;
}

const vkWeatherState_t *vk_weather_state( void )
{
	return &s_state;
}

qboolean vk_weather_is_outdoor_view( void )
{
	if ( r_weatherIndoor && r_weatherIndoor->integer ) {
		return qfalse;
	}
	/* Portal views: treat as outdoor for now unless tagged indoor. */
	if ( backEnd.viewParms.portalView != PV_NONE ) {
		return qtrue;
	}
	return qtrue;
}

void vk_weather_update( void )
{
	int preset;
	float dt;
	float speed;
	vkWeatherState_t blended;

	if ( !vk_weather_active() ) {
		Weather_ApplyPreset( VK_WEATHER_CLEAR, &s_state );
		return;
	}

	dt = Weather_FrameDt();
	preset = r_weatherPreset ? r_weatherPreset->integer : 0;
	if ( preset < 0 || preset >= VK_WEATHER_PRESET_COUNT ) {
		preset = 0;
	}
	if ( r_weatherDynamic && r_weatherDynamic->integer ) {
		s_dynamicTimer += dt;
		if ( s_blend >= 1.0f && s_dynamicTimer >= s_dynamicNextTime ) {
			preset = Weather_ChooseDynamicPreset( s_lastPreset );
			if ( r_weatherPreset ) {
				ri.Cvar_Set( "r_weatherPreset", va( "%d", preset ) );
			}
			Weather_ResetDynamicTimer();
		}
	}
	if ( preset != s_lastPreset ) {
		Weather_ApplyPreset( (vkWeatherPreset_t)preset, &s_target );
		s_blend = 0.0f;
		s_lastPreset = preset;
		vk_volumetric_clouds_on_weather_change();
		if ( r_weatherDebug && r_weatherDebug->integer ) {
			ri.Printf( PRINT_ALL, "[VK][Weather] transition → preset %d\n", preset );
		}
	}

	speed = r_weatherTransition && r_weatherTransition->value > 0.0f ?
		( 1.0f / r_weatherTransition->value ) : 0.125f;
	s_blend = Com_Clamp( 0.0f, 1.0f, s_blend + dt * speed );

	blended = s_state;
	blended.preset = s_target.preset;
	blended.coverage = Weather_Lerp( s_state.coverage, s_target.coverage, s_blend );
	blended.precipitation = Weather_Lerp( s_state.precipitation, s_target.precipitation, s_blend );
	blended.wind = Weather_Lerp( s_state.wind, s_target.wind, s_blend );
	blended.fogDensityScale = Weather_Lerp( s_state.fogDensityScale, s_target.fogDensityScale, s_blend );
	blended.aerosol = Weather_Lerp( s_state.aerosol, s_target.aerosol, s_blend );
	blended.wetnessRate = Weather_Lerp( s_state.wetnessRate, s_target.wetnessRate, s_blend );
	blended.puddleRate = Weather_Lerp( s_state.puddleRate, s_target.puddleRate, s_blend );
	blended.sunVisibility = Weather_Lerp( s_state.sunVisibility, s_target.sunVisibility, s_blend );
	blended.lightningProb = Weather_Lerp( s_state.lightningProb, s_target.lightningProb, s_blend );
	blended.transition = s_blend;
	blended.indoorSuppress = s_target.indoorSuppress;
	s_state = blended;

	if ( s_blend >= 1.0f ) {
		s_state = s_target;
		s_state.transition = 1.0f;
	}

	/* Indoor: kill precip / reduce coverage for cloud shadows. */
	if ( !vk_weather_is_outdoor_view() && s_state.indoorSuppress ) {
		s_state.precipitation = 0.0f;
		s_state.coverage *= 0.15f;
		s_state.wetnessRate = 0.0f;
	}

	if ( s_lightningTimer > 0.0f ) {
		s_lightningTimer = MAX( 0.0f, s_lightningTimer - dt );
	} else {
		s_lightningPeak = 0.0f;
		if ( r_weatherLightning && r_weatherLightning->integer && vk_weather_is_outdoor_view() &&
			s_state.lightningProb > 0.0f && Weather_Rand01() < s_state.lightningProb * dt ) {
			s_lightningTimer = 0.20f + 0.18f * Weather_Rand01();
			s_lightningPeak = ( r_weatherLightningScale ? r_weatherLightningScale->value : 1.25f ) *
				( 0.65f + 0.70f * Weather_Rand01() );
			if ( r_weatherDebug && r_weatherDebug->integer > 1 ) {
				ri.Printf( PRINT_ALL, "[VK][Weather] lightning flash peak=%.2f duration=%.2f\n",
					s_lightningPeak, s_lightningTimer );
			}
		}
	}
}

float vk_weather_sun_visibility( void )
{
	if ( !vk_weather_active() ) {
		return 1.0f;
	}
	return Com_Clamp( 0.0f, 1.0f, s_state.sunVisibility );
}

float vk_weather_direct_sun_factor( void )
{
	float vis;
	float dim;

	if ( !vk_weather_active() ) {
		return 1.0f;
	}
	vis = vk_weather_sun_visibility();
	dim = r_weatherSunDim ? Com_Clamp( 0.0f, 1.0f, r_weatherSunDim->value ) : 1.0f;
	return Com_Clamp( 0.0f, 1.0f, 1.0f - ( 1.0f - vis ) * dim );
}

float vk_weather_shadow_factor( void )
{
	float vis;
	float dim;

	if ( !vk_weather_active() ) {
		return 1.0f;
	}
	vis = vk_weather_sun_visibility();
	dim = r_weatherShadowDim ? Com_Clamp( 0.0f, 1.0f, r_weatherShadowDim->value ) : 1.0f;
	return Com_Clamp( 0.0f, 1.0f, 1.0f - ( 1.0f - vis ) * dim );
}

float vk_weather_lightning_factor( void )
{
	float t;

	if ( !vk_weather_active() || s_lightningTimer <= 0.0f || s_lightningPeak <= 0.0f ) {
		return 1.0f;
	}
	t = Com_Clamp( 0.0f, 1.0f, s_lightningTimer / 0.38f );
	return 1.0f + s_lightningPeak * t * t;
}

float vk_weather_fog_density_scale( void )
{
	if ( !vk_weather_active() ) {
		return 1.0f;
	}
	return ( s_state.fogDensityScale > 0.05f ) ? s_state.fogDensityScale : 0.05f;
}

float vk_weather_cloud_coverage( void )
{
	if ( !vk_weather_active() ) {
		return 0.0f;
	}
	return Com_Clamp( 0.0f, 1.0f, s_state.coverage );
}

float vk_weather_precipitation( void )
{
	if ( !vk_weather_active() || !vk_weather_is_outdoor_view() ) {
		return 0.0f;
	}
	return Com_Clamp( 0.0f, 1.0f, s_state.precipitation );
}

float vk_weather_wetness_rate( void )
{
	if ( !vk_weather_active() || !vk_weather_is_outdoor_view() ) {
		return 0.0f;
	}
	return Com_Clamp( 0.0f, 1.0f, s_state.wetnessRate );
}

void vk_weather_status_f( void )
{
	static const char *names[] = {
		"clear", "cloudy", "overcast", "rain", "storm", "snow", "dust", "fog"
	};
	ri.Printf( PRINT_ALL, "======== Weather (Raster Ultra 1.7) ========\n" );
	ri.Printf( PRINT_ALL, "active         : %s\n", vk_weather_active() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "preset         : %s (%d) blend=%.2f\n",
		names[s_state.preset], (int)s_state.preset, s_blend );
	ri.Printf( PRINT_ALL, "coverage       : %.2f\n", s_state.coverage );
	ri.Printf( PRINT_ALL, "precipitation  : %.2f\n", s_state.precipitation );
	ri.Printf( PRINT_ALL, "fogDensityScale: %.2f\n", s_state.fogDensityScale );
	ri.Printf( PRINT_ALL, "aerosol        : %.2f\n", s_state.aerosol );
	ri.Printf( PRINT_ALL, "sunVisibility  : %.2f\n", s_state.sunVisibility );
	ri.Printf( PRINT_ALL, "sun/shadow     : %.2f / %.2f\n",
		vk_weather_direct_sun_factor(), vk_weather_shadow_factor() );
	ri.Printf( PRINT_ALL, "dynamic        : %s timer=%.1f/%.1f volatility=%.2f\n",
		( r_weatherDynamic && r_weatherDynamic->integer ) ? "yes" : "no",
		s_dynamicTimer, s_dynamicNextTime,
		r_weatherVolatility ? r_weatherVolatility->value : 0.0f );
	ri.Printf( PRINT_ALL, "lightning      : factor=%.2f timer=%.2f peak=%.2f\n",
		vk_weather_lightning_factor(), s_lightningTimer, s_lightningPeak );
	ri.Printf( PRINT_ALL, "wetnessRate    : %.2f puddle=%.2f\n",
		s_state.wetnessRate, s_state.puddleRate );
	ri.Printf( PRINT_ALL, "wind           : %.2f lightningProb=%.3f\n",
		s_state.wind, s_state.lightningProb );
	ri.Printf( PRINT_ALL, "outdoor_view   : %s\n",
		vk_weather_is_outdoor_view() ? "yes" : "no (indoor suppress)" );
	ri.Printf( PRINT_ALL, "sky_owner      : %s\n", vk_sky_owner_name( vk_sky_owner() ) );
	ri.Printf( PRINT_ALL, "============================================\n" );
}
