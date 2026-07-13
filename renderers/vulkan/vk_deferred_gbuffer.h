#pragma once

#ifdef USE_VULKAN

qboolean vk_deferred_gbuffer_active( void );
qboolean vk_deferred_gbuffer_fill_wanted( void );
qboolean vk_deferred_lighting_active( void );
qboolean vk_deferred_unlit_base_wanted( void );
void vk_deferred_gbuffer_init( void );
void vk_deferred_gbuffer_shutdown( void );
void vk_deferred_gbuffer_capture_after_geometry( void );
void vk_deferred_lighting_apply_after_geometry( void );
qboolean vk_deferred_gbuffer_draw_debug( void );

#endif /* USE_VULKAN */
