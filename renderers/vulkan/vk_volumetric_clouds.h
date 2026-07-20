#pragma once

#ifdef USE_VULKAN

/*
 * Raster Ultra 1.7 — volumetric clouds + dedicated history + cloud shadows.
 * Raster only (no RT). History is separate from world TAA.
 */

void vk_volumetric_clouds_register_cvars( void );
void vk_volumetric_clouds_init( void );
void vk_volumetric_clouds_shutdown( void );
void vk_volumetric_clouds_begin_frame( void );
void vk_volumetric_clouds_on_camera_cut( void );
void vk_volumetric_clouds_on_weather_change( void );

qboolean vk_volumetric_clouds_active( void );

/* Cloud shadow factor for sun lighting (1 = full sun, 0 = fully occluded). */
float vk_volumetric_clouds_sun_shadow_factor( void );

void vk_volumetric_clouds_status_f( void );

#endif /* USE_VULKAN */
