#pragma once

#ifdef USE_VULKAN

/* Material class IDs written by material_classify.comp (Phase 1 stub). */
enum {
	VK_MATCLASS_EMPTY = 0,
	VK_MATCLASS_SIMPLE_OPAQUE = 1,
	VK_MATCLASS_LAYERED = 2,
	VK_MATCLASS_TRANSMISSION = 3,
	VK_MATCLASS_EMISSIVE = 4,
	VK_MATCLASS_ALPHA_TEST = 5,
	VK_MATCLASS_TRANSPARENT_FWD = 6
};

qboolean vk_visibility_buffer_active( void );
qboolean vk_visibility_buffer_fill_wanted( void );
qboolean vk_material_classify_wanted( void );
qboolean vk_visibility_late_shade_wanted( void );
void vk_visibility_buffer_init( void );
void vk_visibility_buffer_shutdown( void );
void vk_visibility_buffer_ensure_runtime( void );
/* After opaque (mode 3) or main geometry: fill ID/bary; optional material classify. */
void vk_visibility_buffer_capture_after_geometry( void );
/* Exclusive opaque lighting when r_visibilityBufferLateShade 1 (skips classic deferred lighting). */
void vk_visibility_late_shade_apply_after_geometry( void );
qboolean vk_visibility_buffer_draw_debug( void );
void vk_visibility_buffer_status_f( void );

#endif /* USE_VULKAN */
