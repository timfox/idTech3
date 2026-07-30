#pragma once

#ifdef USE_VULKAN

/*
 * Renderer-owned real-time day/night world lighting.
 *
 * The authored map/q3map_sun remains the noon baseline. When enabled, this
 * module updates tr.sunDirection/tr.sunLight once per frame so the existing
 * sky, atmosphere, fog, CSM, deferred, Forward+/PBR and RTX consumers all see
 * one coherent world sun.
 */

void vk_day_night_register_cvars( void );
void vk_day_night_init( void );
void vk_day_night_on_world_load( const char *mapName );
void vk_day_night_begin_frame( void );

float vk_day_night_time_of_day( void );
qboolean vk_day_night_active( void );
float vk_day_night_day_factor( void );
float vk_day_night_sun_elevation( void );
float vk_day_night_shadow_factor( void );

#endif /* USE_VULKAN */
