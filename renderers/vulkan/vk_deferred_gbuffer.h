#pragma once

#ifdef USE_VULKAN

/*
 * View classification for G-buffer / Ambient Visibility / temporal ownership.
 * Main-world history must not be shared with weapon, portal, mirror, menu, or UI.
 */
typedef enum {
	VK_VIEW_CLASS_MAIN_WORLD = 0,
	VK_VIEW_CLASS_PORTAL,
	VK_VIEW_CLASS_SKY_PORTAL,
	VK_VIEW_CLASS_MIRROR,
	VK_VIEW_CLASS_WEAPON,
	VK_VIEW_CLASS_NO_WORLD,
	VK_VIEW_CLASS_UI,
	VK_VIEW_CLASS_COUNT
} vkViewClass_t;

vkViewClass_t vk_classify_current_view( void );
const char *vk_view_class_name( vkViewClass_t cls );

/* Persistent capability: should G-buffer images exist for the current profile? */
qboolean vk_deferred_gbuffer_resources_wanted( void );
/* Live allocation present and cvars still request the feature. */
qboolean vk_deferred_gbuffer_active( void );
/* Per-frame fill: resources ready + supported world view + opaque work. */
qboolean vk_deferred_gbuffer_fill_wanted( void );
qboolean vk_deferred_lighting_active( void );
qboolean vk_deferred_unlit_base_wanted( void );
/* r_renderMode 3: Unified Clustered Renderer (deferred opaque + Forward+ transparent). */
qboolean vk_unified_clustered_active( void );
qboolean vk_deferred_opaque_transparent_split( void );
/* Opaque draw: fragment should hand off dynamics to deferred (skip Forward+ add). */
qboolean vk_unified_clustered_opaque_handoff( void );

uint32_t vk_deferred_gbuffer_generation( void );
qboolean vk_deferred_gbuffer_generation_valid( void );
void vk_deferred_gbuffer_note_recreate( const char *reason );
void vk_deferred_gbuffer_set_fallback( const char *reason );
void vk_deferred_gbuffer_clear_fallback( void );

void vk_deferred_gbuffer_init( void );
void vk_deferred_gbuffer_shutdown( void );
/* Destroy pipelines/descriptors that hold image views (before attachment destroy). */
void vk_deferred_gbuffer_invalidate_runtime( void );
void vk_deferred_gbuffer_ensure_runtime( void );
void vk_deferred_gbuffer_capture_after_geometry( void );
void vk_deferred_lighting_apply_after_geometry( void );
qboolean vk_deferred_gbuffer_draw_debug( void );
/* Dev: print generation/extent/fill/fallback; verify resources match. */
void vk_deferred_gbuffer_status_f( void );

#endif /* USE_VULKAN */
