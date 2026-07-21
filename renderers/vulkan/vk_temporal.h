#ifndef VK_TEMPORAL_H
#define VK_TEMPORAL_H

#include "vk.h"

/*
 * Shared temporal reset policy (RENDERER_2026 Phase 1).
 * Centralizes history invalidation for volumetrics, motion vectors, exposure,
 * and future TAA/upscaler consumers. Call vk_temporal_request_sticky_reset()
 * when resize, map load, camera cut, or missing prev-frame data is detected.
 * vk_temporal_apply_resets() runs at frame start and clears motion history,
 * volumetric froxel history, and exposure state.
 */
typedef enum {
	VK_TEMPORAL_RESET_NONE                 = 0,
	VK_TEMPORAL_RESET_RENDERER_INIT        = 1u << 0,
	VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE     = 1u << 1,
	VK_TEMPORAL_RESET_RENDER_SIZE_CHANGE   = 1u << 2,
	VK_TEMPORAL_RESET_WORLD_CHANGE         = 1u << 3,
	VK_TEMPORAL_RESET_CLIENT_STATE_CHANGE  = 1u << 4,
	VK_TEMPORAL_RESET_CAMERA_CUT           = 1u << 5,
	VK_TEMPORAL_RESET_EXPLICIT_DEBUG       = 1u << 6,
	VK_TEMPORAL_RESET_MISSING_PREV_DATA    = 1u << 7
} vkTemporalResetReason;

typedef struct {
	uint32_t camera_cut_events;
	uint32_t forced_camera_cut_events;
	uint32_t local_shadow_ready_spot;
	uint32_t local_shadow_ready_point;
	uint32_t local_light_count;
	uint32_t telemetry_nan_or_inf;
	uint32_t telemetry_extinction_clamp_hits;
	uint32_t telemetry_velocity_clamp_hits;
	uint32_t telemetry_density_clamp_hits;
	uint32_t telemetry_pressure_sanitize_hits;
	uint32_t telemetry_temporal_rejects;
} vk_volumetric_validation_state_t;

extern float vk_prev_view_matrix[16];
extern float vk_prev_projection_matrix[16];
extern float vk_prev_viewproj_matrix[16];
extern qboolean vk_prev_matrices_valid;
extern int vk_prev_volumetric_time_ms;
extern int vk_near_static_view_frames;
extern qboolean vk_prev_volumetric_time_valid;
extern float vk_volumetric_noise_time;
extern vk_volumetric_validation_state_t vk_volumetric_validation_state;

void vk_temporal_begin_frame( void );
void vk_temporal_commit_frame_state( void );
void vk_temporal_capture_world_viewparms( void );
void vk_temporal_update_auto_exposure( void );
void vk_temporal_request_sticky_reset( uint32_t reasons );
void vk_temporal_note_first_person_projection( void );
qboolean vk_temporal_has_reason( uint32_t reasonMask );
/* True when r_volumetricFogSkipStatic and view has been near-static ~0.5s (death cam). */
qboolean vk_temporal_near_static_streak_guard( void );

/* World Temporal Reconstruction is active this frame (r_taa / aaMode / upscale). */
qboolean vk_temporal_reconstruction_wanted( void );
/* Defer RDF_NOWORLDMODEL weapon draws until after world TAA. */
qboolean vk_temporal_want_weapon_after_taa( void );
/* Defer weapon until after world temporal/SSR passes that cannot consume weapon depth safely. */
qboolean vk_temporal_want_weapon_after_world_post( void );
qboolean vk_temporal_defer_weapon_drawsurfs( const void *drawSurfsCmd );
void vk_temporal_flush_deferred_weapon_after_taa( VkImageView *post_fog_src, VkImageView *luminance_src );

void vk_reset_motion_history( void );
void vk_reset_taa_history( void );
void vk_reset_volumetric_history( void );

/* Console: temporal history ownership / reset state (Spine diagnostics). */
void vk_temporal_status_f( void );
/* Console: pass inventory for weapon-trail / temporal-ghost bisect. */
void vk_temporal_ghost_status_f( void );

#endif
