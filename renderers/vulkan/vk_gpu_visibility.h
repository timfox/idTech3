#pragma once

/*
 * GPU-Driven Visibility Milestone 1 — stages, candidates, occlusion policy.
 * BSP/PVS remains Stage 0 (conservative). Does not replace classic draws.
 */

#ifdef USE_VULKAN

#include "../common/tr_types.h"

typedef enum {
	VISIBILITY_PVS = 0,
	VISIBILITY_FRUSTUM,
	VISIBILITY_HIZ,
	VISIBILITY_LOD,
	VISIBILITY_RENDER_PATH,
	VISIBILITY_FINAL,
	VISIBILITY_STAGE_COUNT
} vkVisibilityStage_t;

typedef enum {
	VK_GPU_DRAW_LIST_DEFERRED_OPAQUE = 0,
	VK_GPU_DRAW_LIST_FORWARD_OPAQUE,
	VK_GPU_DRAW_LIST_ALPHA_TEST,
	VK_GPU_DRAW_LIST_SHADOW,
	VK_GPU_DRAW_LIST_TRANSPARENT,
	VK_GPU_DRAW_LIST_WEAPON,
	VK_GPU_DRAW_LIST_DEPTH_PREPASS,
	VK_GPU_DRAW_LIST_VELOCITY,
	VK_GPU_DRAW_LIST_DEBUG,
	VK_GPU_DRAW_LIST_COUNT
} vkGpuDrawList_t;

void vk_gpu_visibility_register_cvars( void );
void vk_gpu_visibility_init( void );
void vk_gpu_visibility_shutdown( void );
void vk_gpu_visibility_begin_frame( void );

/* Stage 0: append PVS/CPU candidates (object handles). Never less conservative than BSP. */
void vk_gpu_visibility_clear_candidates( void );
qboolean vk_gpu_visibility_add_candidate( uint32_t objectHandle, vkVisibilityStage_t passedStage );
uint32_t vk_gpu_visibility_candidate_count( void );
const uint32_t *vk_gpu_visibility_candidates( void );

void vk_gpu_visibility_note_reject( vkVisibilityStage_t stage );
void vk_gpu_visibility_telemetry( uint32_t outRejected[VISIBILITY_STAGE_COUNT] );

qboolean vk_gpu_occlusion_enabled( void );
uint32_t vk_gpu_occlusion_grace_frames( void );
float vk_gpu_occlusion_bias( void );

void vk_gpu_visibility_status_f( void );
void vk_gpu_visibility_perf_f( void );

/* Pure math (CPU reference / unit tests) — sphere vs 4 frustum planes. */
qboolean vk_gpu_frustum_sphere_visible( const float sphere[4],
	const float planeNormals[4][3], const float planeDists[4] );

#endif /* USE_VULKAN */
