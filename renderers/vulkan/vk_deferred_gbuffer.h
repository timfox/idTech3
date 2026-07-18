#pragma once

#ifdef USE_VULKAN

qboolean vk_deferred_gbuffer_active( void );
qboolean vk_deferred_gbuffer_fill_wanted( void );
qboolean vk_deferred_lighting_active( void );
qboolean vk_deferred_unlit_base_wanted( void );
/* r_renderMode 3: Unified Clustered Renderer (deferred opaque + Forward+ transparent). */
qboolean vk_unified_clustered_active( void );
/* Mode 3 opaque draw: fragment should hand off dynamics to deferred (skip Forward+ add). */
qboolean vk_unified_clustered_opaque_handoff( void );
void vk_deferred_gbuffer_init( void );
void vk_deferred_gbuffer_shutdown( void );
void vk_deferred_gbuffer_ensure_runtime( void );
void vk_deferred_gbuffer_capture_after_geometry( void );
void vk_deferred_lighting_apply_after_geometry( void );
qboolean vk_deferred_gbuffer_draw_debug( void );

#endif /* USE_VULKAN */
