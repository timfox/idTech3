/*
===========================================================================
Cinematic Engine Platform 1.0 — Photometric lighting contract.
===========================================================================
*/

#include "tr_local.h"
#include "vk_photometric.h"
#include "vk_ltc.h"
#include "ltc_tables.h"

static cvar_t *r_photometricLights;
static cvar_t *r_photometricLegacyScale;
static cvar_t *r_photometricKelvin;
static qboolean s_cmds;
static vkPhotometricState_t s_photo;

void vk_photometric_register_cvars( void )
{
	if ( r_photometricLights ) {
		return;
	}

	r_photometricLights = ri.Cvar_Get( "r_photometricLights", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_photometricLights, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_photometricLights,
		"Photometric lighting contract (Cinematic Platform 1.0).\n"
		"Defines candela/lumen/lux/nit ↔ legacy conversion. Opt-in pack scaling." );
	ri.Cvar_SetGroup( r_photometricLights, CVG_RENDERER );

	r_photometricLegacyScale = ri.Cvar_Get( "r_photometricLegacyScale", "0.001", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_photometricLegacyScale, "0.000001", "10", CV_FLOAT );
	ri.Cvar_SetDescription( r_photometricLegacyScale,
		"Candela → legacy dlight multiplier (default 0.001 keeps classic maps unchanged when off)." );

	r_photometricKelvin = ri.Cvar_Get( "r_photometricKelvin", "6500", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_photometricKelvin, "1000", "20000", CV_FLOAT );
}

void vk_photometric_init( void )
{
	vk_photometric_register_cvars();
	Com_Memset( &s_photo, 0, sizeof( s_photo ) );
	s_photo.ltcTablesPresent = qtrue;
	s_photo.ltcUploaded = vk_ltc_uploaded();
	s_photo.legacyScale = r_photometricLegacyScale ? r_photometricLegacyScale->value : 0.001f;
	s_photo.defaultKelvin = r_photometricKelvin ? r_photometricKelvin->value : 6500.0f;

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "photometric_status", vk_photometric_status_f );
		s_cmds = qtrue;
	}

	if ( r_photometricLights && r_photometricLights->integer ) {
		ri.Printf( PRINT_ALL,
			"[VK] Photometric lights: active (legacyScale=%.4f kelvin=%.0f LTC_tables=%s)\n",
			s_photo.legacyScale, s_photo.defaultKelvin,
			s_photo.ltcTablesPresent ? "present" : "missing" );
		(void)s_ltcMatCanonical[0]; /* ensure LTC table is linked */
	}
}

void vk_photometric_shutdown( void )
{
	if ( s_cmds ) {
		ri.Cmd_RemoveCommand( "photometric_status" );
		s_cmds = qfalse;
	}
	Com_Memset( &s_photo, 0, sizeof( s_photo ) );
}

qboolean vk_photometric_active( void )
{
	return ( r_photometricLights && r_photometricLights->integer ) ? qtrue : qfalse;
}

const vkPhotometricState_t *vk_photometric_state( void )
{
	s_photo.active = vk_photometric_active();
	s_photo.applyToPack = s_photo.active;
	s_photo.legacyScale = r_photometricLegacyScale ? r_photometricLegacyScale->value : 0.001f;
	s_photo.defaultKelvin = r_photometricKelvin ? r_photometricKelvin->value : 6500.0f;
	s_photo.ltcUploaded = vk_ltc_uploaded();
	return &s_photo;
}

void vk_photometric_kelvin_to_rgb( float kelvin, vec3_t outRgb )
{
	/* Tanner Helland / approximated blackbody → sRGB (clamped). */
	float t, r, g, b;

	if ( !outRgb ) {
		return;
	}
	t = Com_Clamp( 1000.0f, 40000.0f, kelvin ) / 100.0f;
	if ( t <= 66.0f ) {
		r = 1.0f;
		g = Com_Clamp( 0.0f, 1.0f, ( 0.3900815788f * logf( t ) - 0.6318414438f ) );
	} else {
		r = Com_Clamp( 0.0f, 1.0f, ( 1.2929362f * powf( t - 60.0f, -0.1332047592f ) ) );
		g = Com_Clamp( 0.0f, 1.0f, ( 1.12989086f * powf( t - 60.0f, -0.0755148492f ) ) );
	}
	if ( t >= 66.0f ) {
		b = 1.0f;
	} else if ( t <= 19.0f ) {
		b = 0.0f;
	} else {
		b = Com_Clamp( 0.0f, 1.0f, ( 0.543206789f * logf( t - 10.0f ) - 1.19625408f ) );
	}
	outRgb[0] = r;
	outRgb[1] = g;
	outRgb[2] = b;
	s_photo.conversions++;
}

float vk_photometric_to_legacy_scale( vkPhotometricUnit_t unit, float value, float radiusMeters )
{
	float scale = r_photometricLegacyScale ? r_photometricLegacyScale->value : 0.001f;
	float r = radiusMeters > 0.01f ? radiusMeters : 1.0f;

	s_photo.conversions++;
	switch ( unit ) {
	case VK_PHOTO_UNIT_CANDELA:
		return value * scale;
	case VK_PHOTO_UNIT_LUMEN:
		/* Isotropic point: I = Φ / (4π) */
		return ( value / ( 4.0f * (float)M_PI ) ) * scale;
	case VK_PHOTO_UNIT_LUX:
		/* Rough: E ≈ I / r² → I ≈ E * r² */
		return ( value * r * r ) * scale;
	case VK_PHOTO_UNIT_NIT:
		/* Area luminance → provisional scale; LTC path will own true radiometry. */
		return value * scale * 0.1f;
	case VK_PHOTO_UNIT_LEGACY:
	default:
		return value;
	}
}

float vk_photometric_pack_intensity_scale( float legacyColorMax, float radius )
{
	(void)legacyColorMax;
	(void)radius;
	if ( !vk_photometric_active() ) {
		return 1.0f;
	}
	/* When active without per-light photometric metadata, preserve legacy 1:1. */
	return 1.0f;
}

const char *vk_photometric_unit_name( vkPhotometricUnit_t u )
{
	static const char *names[] = {
		"legacy", "candela", "lumen", "lux", "nit"
	};
	if ( u < 0 || u >= VK_PHOTO_UNIT_COUNT ) {
		return "invalid";
	}
	return names[u];
}

void vk_photometric_status_f( void )
{
	vec3_t rgb;
	const vkPhotometricState_t *st = vk_photometric_state();

	vk_photometric_kelvin_to_rgb( st->defaultKelvin, rgb );
	ri.Printf( PRINT_ALL, "=== Photometric Lights (Cinematic Platform 1.0) ===\n" );
	ri.Printf( PRINT_ALL, "active         : %s applyToPack=%s\n",
		st->active ? "yes" : "no", st->applyToPack ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "legacyScale    : %.6f (candela→legacy)\n", st->legacyScale );
	ri.Printf( PRINT_ALL, "defaultKelvin  : %.0f → rgb(%.3f %.3f %.3f)\n",
		st->defaultKelvin, rgb[0], rgb[1], rgb[2] );
	ri.Printf( PRINT_ALL, "LTC            : tables=%s uploaded=%s (Forward+/deferred rect area lights)\n",
		st->ltcTablesPresent ? "yes" : "no", st->ltcUploaded ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "conversions    : %u\n", st->conversions );
	ri.Printf( PRINT_ALL, "contract       : deferred/Forward+/volumetrics/GI/tools share these units\n" );
	ri.Printf( PRINT_ALL, "IES/gobos      : not in Environment Slice (quality opt-in later)\n" );
}
