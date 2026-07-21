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
/* Temporal frame index at which vk_prev_* matrices were committed (Phase 5:
 * previous transforms must be exactly one temporal frame old). */
extern uint32_t vk_prev_matrices_frame;
/* Projection jitter (pixels) embedded in vk_prev_projection_matrix (Phase 7:
 * producers rebase the previous projection onto the current jitter so motion
 * vectors carry no jitter delta). */
extern float vk_prev_jitter_x;
extern float vk_prev_jitter_y;
extern qboolean vk_prev_jitter_valid;
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
qboolean vk_temporal_defer_bloom_for_weapon( void );
qboolean vk_temporal_want_dedicated_weapon_bloom( void );
qboolean vk_weapon_bloom( void );
qboolean vk_temporal_defer_weapon_drawsurfs( const void *drawSurfsCmd );
void vk_temporal_flush_deferred_weapon_after_taa( VkImageView *post_fog_src, VkImageView *luminance_src );

void vk_reset_motion_history( void );
void vk_reset_taa_history( void );
void vk_reset_weapon_history( void );
void vk_reset_volumetric_history( void );
/* Normalize/copy current depth into the selected R32F history image. */
qboolean vk_temporal_store_previous_depth( uint32_t writeIndex );
qboolean vk_temporal_store_weapon_depth( uint32_t writeIndex );
/* Resolve MSAA depth before TAA samples the normalized current-depth view. */
qboolean vk_temporal_prepare_current_depth( void );
void vk_temporal_dispatch_depth_reject_stats( uint32_t prevDepthIndex );
void vk_temporal_readback_depth_reject_stats( void );

/* Phase 6: GPU debug markers + once-per-frame resolve counters. */
void vk_temporal_marker_begin( const char *name );
void vk_temporal_marker_end( void );
void vk_temporal_note_world_resolve( void );
void vk_temporal_note_weapon_resolve( void );
void vk_temporal_note_upscale_blit( void );

/* Console: temporal history ownership / reset state (Spine diagnostics). */
void vk_temporal_status_f( void );
/* Console: Phase 3/4 extent + velocity-space report (see vk_velocity_space.h). */
void vk_temporal_resolution_status_f( void );
/* Console: pass inventory for weapon-trail / temporal-ghost bisect. */
void vk_temporal_ghost_status_f( void );
void vk_capture_temporal_debug_f( void );
/* Surf shipping-profile startup summary and resource/cvar validation. */
void vk_surf_log_temporal_config( void );
void vk_surf_validate_temporal_config_f( void );
void vk_print_weapon_presentation_f( void );

#endif
