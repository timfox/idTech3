/*
===========================================================================
Cinematic Engine Platform 1.0 — Photometric lighting contract (Environment Slice).
Canonical unit conversion for deferred / Forward+ / volumetrics / GI / tools.
LTC LUT availability is reported; full area-light shading remains quality opt-in.
===========================================================================
*/

#pragma once

#ifdef USE_VULKAN

typedef enum {
	VK_PHOTO_UNIT_LEGACY = 0,   /* classic Q3 dlight color*radius intensity */
	VK_PHOTO_UNIT_CANDELA,      /* luminous intensity (point/spot) */
	VK_PHOTO_UNIT_LUMEN,        /* luminous flux */
	VK_PHOTO_UNIT_LUX,          /* illuminance (directional) */
	VK_PHOTO_UNIT_NIT,          /* luminance (area emitters) */
	VK_PHOTO_UNIT_COUNT
} vkPhotometricUnit_t;

typedef struct vkPhotometricState_s {
	qboolean active;
	qboolean applyToPack;
	qboolean ltcTablesPresent;
	qboolean ltcUploaded;       /* Slice 1: tables linked; GPU upload deferred */
	float    legacyScale;       /* candela → legacy multiplier */
	float    defaultKelvin;
	uint32_t conversions;
} vkPhotometricState_t;

void vk_photometric_register_cvars( void );
void vk_photometric_init( void );
void vk_photometric_shutdown( void );

qboolean vk_photometric_active( void );
const vkPhotometricState_t *vk_photometric_state( void );

/* Kelvin → linear sRGB (approx CIE daylight locus). */
void vk_photometric_kelvin_to_rgb( float kelvin, vec3_t outRgb );

/* Convert physical quantity to legacy dlight color scale factor (multiplies RGB). */
float vk_photometric_to_legacy_scale( vkPhotometricUnit_t unit, float value, float radiusMeters );

/* Optional pack-time scale when r_photometricLights is on (defaults preserve legacy). */
float vk_photometric_pack_intensity_scale( float legacyColorMax, float radius );

void vk_photometric_status_f( void );

const char *vk_photometric_unit_name( vkPhotometricUnit_t u );

#endif /* USE_VULKAN */
