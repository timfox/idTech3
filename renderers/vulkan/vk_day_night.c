/*
===========================================================================
Real-time day/night world lighting.
===========================================================================
*/

#include "tr_local.h"
#include "vk_day_night.h"
#include "vk_weather.h"

#ifdef USE_VULKAN

#include <math.h>

static cvar_t *r_dayNight;
static cvar_t *r_dayNightUseRealTime;
static cvar_t *r_dayNightTime;
static cvar_t *r_dayNightCycleMinutes;
static cvar_t *r_dayNightLatitude;
static cvar_t *r_dayNightNorthYaw;
static cvar_t *r_dayNightSunScale;
static cvar_t *r_dayNightMoonScale;
static cvar_t *r_dayNightAmbientScale;
static cvar_t *r_dayNightShadowFade;
static cvar_t *r_dayNightMoonShadow;
static cvar_t *r_dayNightDebug;

static vec3_t s_baseSunLight = { 1.0f, 1.0f, 1.0f };
static vec3_t s_baseSunDir = { 0.45f, 0.3f, 0.9f };
static char s_mapName[MAX_QPATH];
static float s_timeOfDay = 12.0f;
static float s_dayFactor = 1.0f;
static float s_sunElevation = 1.0f;
static vec3_t s_skyAmbient = { 1.0f, 1.0f, 1.0f };
static qboolean s_haveWorld;
static qboolean s_cmds;

static float DN_Clamp01( float v )
{
	return Com_Clamp( 0.0f, 1.0f, v );
}

static float DN_SmoothStep( float edge0, float edge1, float x )
{
	float t;

	if ( edge0 == edge1 ) {
		return x >= edge1 ? 1.0f : 0.0f;
	}
	t = DN_Clamp01( ( x - edge0 ) / ( edge1 - edge0 ) );
	return t * t * ( 3.0f - 2.0f * t );
}

static float DN_TimeFromClock( void )
{
	qtime_t qt;

	Com_Memset( &qt, 0, sizeof( qt ) );
	ri.Com_RealTime( &qt );
	return (float)qt.tm_hour + (float)qt.tm_min / 60.0f + (float)qt.tm_sec / 3600.0f;
}

static float DN_TimeOfDay( void )
{
	float t;

	if ( r_dayNightUseRealTime && r_dayNightUseRealTime->integer ) {
		t = DN_TimeFromClock();
	} else if ( r_dayNightCycleMinutes && r_dayNightCycleMinutes->value > 0.0f ) {
		float cycleMs = r_dayNightCycleMinutes->value * 60000.0f;
		t = fmodf( (float)ri.Milliseconds(), cycleMs ) / cycleMs * 24.0f;
	} else {
		t = r_dayNightTime ? r_dayNightTime->value : 12.0f;
	}

	while ( t < 0.0f ) {
		t += 24.0f;
	}
	while ( t >= 24.0f ) {
		t -= 24.0f;
	}
	return t;
}

static void DN_CaptureAuthoredSun( void )
{
	VectorCopy( tr.sunDirection, s_baseSunDir );
	if ( VectorLength( s_baseSunDir ) < 0.001f ) {
		s_baseSunDir[0] = 0.45f;
		s_baseSunDir[1] = 0.3f;
		s_baseSunDir[2] = 0.9f;
	}
	VectorNormalize( s_baseSunDir );

	VectorCopy( tr.sunLight, s_baseSunLight );
	if ( VectorLength( s_baseSunLight ) < 0.001f ) {
		s_baseSunLight[0] = 1.0f;
		s_baseSunLight[1] = 1.0f;
		s_baseSunLight[2] = 1.0f;
	}
}

static void DN_Evaluate( float tod, vec3_t outDir, vec3_t outLight, float *outElevation, float *outDay )
{
	const float pi = (float)M_PI;
	float latitude = r_dayNightLatitude ? r_dayNightLatitude->value : 35.0f;
	float yawOffset = r_dayNightNorthYaw ? r_dayNightNorthYaw->value * pi / 180.0f : 0.0f;
	float sunScale = r_dayNightSunScale ? r_dayNightSunScale->value : 1.0f;
	float moonScale = r_dayNightMoonScale ? r_dayNightMoonScale->value : 0.035f;
	float ambientScale = r_dayNightAmbientScale ? r_dayNightAmbientScale->value : 0.18f;
	float phase = ( tod - 6.0f ) / 24.0f * 2.0f * pi;
	float elev = sinf( phase );
	float day = DN_SmoothStep( -0.08f, 0.16f, elev );
	float twilight = DN_SmoothStep( -0.22f, 0.04f, elev ) * ( 1.0f - DN_SmoothStep( 0.10f, 0.34f, elev ) );
	float az = phase + yawOffset;
	float latTilt = cosf( latitude * pi / 180.0f );
	float warmth = 1.0f - DN_SmoothStep( 0.12f, 0.70f, elev );
	vec3_t dayTint = { 1.0f, 0.97f, 0.90f };
	vec3_t twilightTint = { 1.0f, 0.54f, 0.30f };
	vec3_t nightTint = { 0.12f, 0.18f, 0.34f };
	vec3_t tint;
	vec3_t skyDay = { 0.80f, 0.92f, 1.0f };
	vec3_t skyTwilight = { 1.0f, 0.50f, 0.28f };
	vec3_t skyNight = { 0.05f, 0.07f, 0.14f };
	float skyPower;
	float intensity;
	int i;

	outDir[0] = cosf( az ) * latTilt;
	outDir[1] = sinf( az ) * latTilt;
	outDir[2] = elev;
	if ( VectorLength( outDir ) < 0.001f ) {
		VectorCopy( s_baseSunDir, outDir );
	} else {
		VectorNormalize( outDir );
	}

	intensity = sunScale * ( 0.08f + 0.92f * DN_SmoothStep( -0.02f, 0.34f, elev ) );
	intensity = intensity * day + moonScale * ( 1.0f - day );
	intensity += ambientScale * twilight * 0.08f;

	for ( i = 0; i < 3; ++i ) {
		float warmTint = dayTint[i] * ( 1.0f - warmth ) + twilightTint[i] * warmth;
		tint[i] = warmTint * ( day + twilight * 0.35f ) + nightTint[i] * ( 1.0f - day );
		outLight[i] = s_baseSunLight[i] * tint[i] * intensity;
	}

	skyPower = ( 0.04f + 0.96f * DN_SmoothStep( -0.18f, 0.38f, elev ) ) *
		vk_weather_direct_sun_factor();
	for ( i = 0; i < 3; ++i ) {
		float skyTint = skyDay[i] * day + skyTwilight[i] * twilight * 0.55f + skyNight[i] * ( 1.0f - day );
		s_skyAmbient[i] = Com_Clamp( 0.0f, 1.5f, skyTint * skyPower );
	}

	if ( outElevation ) {
		*outElevation = elev;
	}
	if ( outDay ) {
		*outDay = day;
	}
}

static void DN_RestoreAuthoredSun( void )
{
	if ( !s_haveWorld ) {
		return;
	}
	VectorCopy( s_baseSunDir, tr.sunDirection );
	VectorCopy( s_baseSunLight, tr.sunLight );
}

qboolean vk_day_night_active( void )
{
	return ( r_dayNight && r_dayNight->integer && s_haveWorld && tr.world ) ? qtrue : qfalse;
}

float vk_day_night_time_of_day( void )
{
	return s_timeOfDay;
}

float vk_day_night_day_factor( void )
{
	return vk_day_night_active() ? s_dayFactor : 1.0f;
}

float vk_day_night_sun_elevation( void )
{
	return vk_day_night_active() ? s_sunElevation : 1.0f;
}

float vk_day_night_shadow_factor( void )
{
	float fade;
	float moon;

	if ( !vk_day_night_active() ) {
		return 1.0f;
	}

	fade = r_dayNightShadowFade ? DN_Clamp01( r_dayNightShadowFade->value ) : 1.0f;
	moon = r_dayNightMoonShadow ? DN_Clamp01( r_dayNightMoonShadow->value ) : 0.18f;
	return Com_Clamp( 0.0f, 1.0f,
		( s_dayFactor * fade + ( 1.0f - s_dayFactor ) * moon ) *
		vk_weather_shadow_factor() );
}

void vk_day_night_sky_ambient( vec3_t outAmbient )
{
	if ( !outAmbient ) {
		return;
	}
	if ( !vk_day_night_active() ) {
		outAmbient[0] = 1.0f;
		outAmbient[1] = 1.0f;
		outAmbient[2] = 1.0f;
		return;
	}
	VectorCopy( s_skyAmbient, outAmbient );
}

void vk_day_night_register_cvars( void )
{
	if ( r_dayNight ) {
		return;
	}

	r_dayNight = ri.Cvar_Get( "r_dayNight", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dayNight, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_dayNight,
		"Enable renderer-owned day/night world lighting. Updates tr.sunDirection/tr.sunLight from real time or an accelerated cycle." );
	ri.Cvar_SetGroup( r_dayNight, CVG_RENDERER );

	r_dayNightUseRealTime = ri.Cvar_Get( "r_dayNightUseRealTime", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dayNightUseRealTime, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_dayNightUseRealTime, "Use local wall-clock time for r_dayNight. Set 0 for manual/accelerated testing." );

	r_dayNightTime = ri.Cvar_Get( "r_dayNightTime", "12", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dayNightTime, "0", "24", CV_FLOAT );
	ri.Cvar_SetDescription( r_dayNightTime, "Manual time of day in hours when r_dayNightUseRealTime 0 and r_dayNightCycleMinutes 0." );

	r_dayNightCycleMinutes = ri.Cvar_Get( "r_dayNightCycleMinutes", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dayNightCycleMinutes, "0", "240", CV_FLOAT );
	ri.Cvar_SetDescription( r_dayNightCycleMinutes, "Accelerated full 24-hour cycle length in minutes when non-zero and r_dayNightUseRealTime 0." );

	r_dayNightLatitude = ri.Cvar_Get( "r_dayNightLatitude", "35", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dayNightLatitude, "-80", "80", CV_FLOAT );
	ri.Cvar_SetDescription( r_dayNightLatitude, "Approximate world latitude used to tilt the sun path." );

	r_dayNightNorthYaw = ri.Cvar_Get( "r_dayNightNorthYaw", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dayNightNorthYaw, "-360", "360", CV_FLOAT );
	ri.Cvar_SetDescription( r_dayNightNorthYaw, "Yaw offset in degrees for the day/night sun path." );

	r_dayNightSunScale = ri.Cvar_Get( "r_dayNightSunScale", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dayNightSunScale, "0", "8", CV_FLOAT );
	ri.Cvar_SetDescription( r_dayNightSunScale, "Multiplier for daytime authored sun radiance." );

	r_dayNightMoonScale = ri.Cvar_Get( "r_dayNightMoonScale", "0.035", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dayNightMoonScale, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_dayNightMoonScale, "Fraction of authored sun radiance used as moon/night directional light." );

	r_dayNightAmbientScale = ri.Cvar_Get( "r_dayNightAmbientScale", "0.18", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dayNightAmbientScale, "0", "2", CV_FLOAT );
	ri.Cvar_SetDescription( r_dayNightAmbientScale, "Twilight fill added around sunrise and sunset." );

	r_dayNightShadowFade = ri.Cvar_Get( "r_dayNightShadowFade", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dayNightShadowFade, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_dayNightShadowFade, "Scales daytime sun-shadow strength when day/night lighting is active." );

	r_dayNightMoonShadow = ri.Cvar_Get( "r_dayNightMoonShadow", "0.18", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dayNightMoonShadow, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_dayNightMoonShadow, "Night directional shadow floor for moonlit day/night lighting. Set 0 to skip deep-night sun CSM." );

	r_dayNightDebug = ri.Cvar_Get( "r_dayNightDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dayNightDebug, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_dayNightDebug, "Print day/night world-lighting state changes." );
}

static void vk_day_night_status_f( void )
{
	ri.Printf( PRINT_ALL,
		"daynight_status: active=%d map=%s time=%.2f realTime=%d cycleMin=%.2f day=%.2f elev=%.2f shadow=%.2f\n"
		"  sunDir=(%.3f %.3f %.3f) sunLight=(%.3f %.3f %.3f)\n"
		"  skyAmbient=(%.3f %.3f %.3f)\n"
		"  authoredDir=(%.3f %.3f %.3f) authoredLight=(%.3f %.3f %.3f)\n",
		vk_day_night_active() ? 1 : 0,
		s_mapName[0] ? s_mapName : "<none>",
		s_timeOfDay,
		r_dayNightUseRealTime ? r_dayNightUseRealTime->integer : 1,
		r_dayNightCycleMinutes ? r_dayNightCycleMinutes->value : 0.0f,
		s_dayFactor,
		s_sunElevation,
		vk_day_night_shadow_factor(),
		tr.sunDirection[0], tr.sunDirection[1], tr.sunDirection[2],
		tr.sunLight[0], tr.sunLight[1], tr.sunLight[2],
		s_skyAmbient[0], s_skyAmbient[1], s_skyAmbient[2],
		s_baseSunDir[0], s_baseSunDir[1], s_baseSunDir[2],
		s_baseSunLight[0], s_baseSunLight[1], s_baseSunLight[2] );
}

void vk_day_night_init( void )
{
	vk_day_night_register_cvars();
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "daynight_status", vk_day_night_status_f );
		s_cmds = qtrue;
	}
}

void vk_day_night_on_world_load( const char *mapName )
{
	DN_CaptureAuthoredSun();
	Q_strncpyz( s_mapName, mapName ? mapName : "", sizeof( s_mapName ) );
	s_haveWorld = qtrue;
	s_timeOfDay = DN_TimeOfDay();
	if ( r_dayNightDebug && r_dayNightDebug->integer ) {
		ri.Printf( PRINT_ALL,
			"[VK][DayNight] captured authored sun for %s dir=(%.2f %.2f %.2f) light=(%.2f %.2f %.2f)\n",
			s_mapName[0] ? s_mapName : "<unknown>",
			s_baseSunDir[0], s_baseSunDir[1], s_baseSunDir[2],
			s_baseSunLight[0], s_baseSunLight[1], s_baseSunLight[2] );
	}
}

void vk_day_night_begin_frame( void )
{
	vec3_t dir;
	vec3_t light;

	if ( !s_haveWorld || !tr.world ) {
		return;
	}
	if ( !vk_day_night_active() ) {
		DN_RestoreAuthoredSun();
		return;
	}

	s_timeOfDay = DN_TimeOfDay();
	DN_Evaluate( s_timeOfDay, dir, light, &s_sunElevation, &s_dayFactor );
	VectorCopy( dir, tr.sunDirection );
	VectorScale( light, vk_weather_direct_sun_factor() * vk_weather_lightning_factor(), light );
	VectorCopy( light, tr.sunLight );

	if ( r_dayNightDebug && r_dayNightDebug->integer > 1 ) {
		ri.Printf( PRINT_ALL,
			"[VK][DayNight] t=%.2f day=%.2f elev=%.2f dir=(%.2f %.2f %.2f) light=(%.2f %.2f %.2f)\n",
			s_timeOfDay, s_dayFactor, s_sunElevation,
			tr.sunDirection[0], tr.sunDirection[1], tr.sunDirection[2],
			tr.sunLight[0], tr.sunLight[1], tr.sunLight[2] );
	}
}

#endif /* USE_VULKAN */
