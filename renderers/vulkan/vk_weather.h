#pragma once


/*
 * Raster Ultra 1.7 — data-driven weather controller.
 * Drives precipitation intensity, fog/cloud coverage hooks, wetness rates.
 * Does not require RT. Classic maps stay dry unless opted in.
 */

typedef enum {
	VK_WEATHER_CLEAR = 0,
	VK_WEATHER_CLOUDY,
	VK_WEATHER_OVERCAST,
	VK_WEATHER_RAIN,
	VK_WEATHER_STORM,
	VK_WEATHER_SNOW,
	VK_WEATHER_DUST,
	VK_WEATHER_FOG,
	VK_WEATHER_PRESET_COUNT
} vkWeatherPreset_t;

typedef struct {
	vkWeatherPreset_t preset;
	float coverage;          /* 0..1 cloud coverage */
	float precipitation;     /* 0..1 rain/snow intensity */
	float wind;              /* 0..1 */
	float fogDensityScale;   /* multiplies froxel density */
	float aerosol;           /* mie / turbidity scale */
	float wetnessRate;       /* surface wetness accumulation request */
	float puddleRate;
	float sunVisibility;     /* 0..1 extinction of sun through weather */
	float lightningProb;
	float transition;        /* 0..1 blend into target */
	qboolean indoorSuppress; /* rain/clouds suppressed indoors */
} vkWeatherState_t;

void vk_weather_register_cvars( void );
void vk_weather_init( void );
void vk_weather_shutdown( void );
void vk_weather_update( void );

qboolean vk_weather_active( void );
const vkWeatherState_t *vk_weather_state( void );

/* Indoor/outdoor routing — when indoor, suppress outdoor precip / cloud shadows. */
qboolean vk_weather_is_outdoor_view( void );

float vk_weather_sun_visibility( void );
float vk_weather_direct_sun_factor( void );
float vk_weather_shadow_factor( void );
float vk_weather_lightning_factor( void );
float vk_weather_fog_density_scale( void );
float vk_weather_cloud_coverage( void );
float vk_weather_precipitation( void );
float vk_weather_wetness_rate( void );

void vk_weather_status_f( void );

