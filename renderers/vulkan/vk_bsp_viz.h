#pragma once

#ifdef USE_VULKAN

#include "../common/tr_types.h"

typedef enum bspCullReason_e {
	BSP_CULL_NONE             = 0,
	BSP_CULL_INVALID          = 1 << 0,
	BSP_CULL_PVS              = 1 << 1,
	BSP_CULL_AREA             = 1 << 2,
	BSP_CULL_FRUSTUM          = 1 << 3,
	BSP_CULL_BACKFACE         = 1 << 4,
	BSP_CULL_DUPLICATE        = 1 << 5,
	BSP_CULL_CONTENTS         = 1 << 6,
	BSP_CULL_OCCLUDED         = 1 << 7,
	BSP_CULL_NOVIS_FALLBACK   = 1 << 8
} bspCullReason_t;

typedef struct bspVisibilityFrame_s {
	uint64_t frameNumber;
	uint32_t generation;

	int32_t viewLeaf;
	int32_t viewCluster;
	int32_t viewArea;

	uint32_t visibleLeafCount;
	uint32_t visibleSurfaceCount;
	uint32_t submittedSurfaceCount;
	uint32_t submittedSkySurfaceCount;
	uint32_t submittedNonSkySurfaceCount;
	uint32_t duplicateRejects;
	uint32_t backfaceRejects;
	uint32_t frustumRejects;

	uint32_t pvsGeneration;
	uint32_t areaMaskGeneration;
	uint32_t frustumGeneration;
	uint32_t mapGeneration;

	qboolean novisActive;
	qboolean stale;
} bspVisibilityFrame_t;

void vk_bsp_viz_register( void );
void vk_bsp_viz_begin_frame( void );
void vk_bsp_viz_on_map_change( void );
void vk_bsp_viz_note_mark_leaves( int32_t viewLeaf, int32_t viewCluster, int32_t viewArea,
	qboolean novisFallback );
void vk_bsp_viz_note_leaf_accepted( void );
void vk_bsp_viz_note_surface_accepted( void );
void vk_bsp_viz_note_surface_classified( qboolean isSky );
void vk_bsp_viz_note_surface_duplicate( void );
void vk_bsp_viz_note_surface_backface( void );
void vk_bsp_viz_note_leaf_frustum_reject( void );
void vk_bsp_viz_finalize_world( void );

/* Returns qtrue when overlay should depth-test like production (visible-only). */
qboolean vk_bsp_viz_want_visible_overlay( void );
/* Returns qtrue when explicit through-walls developer mode is active. */
qboolean vk_bsp_viz_want_through_walls( void );
/* When non-zero, force wireframe of authoritative submitted geometry. */
int vk_bsp_viz_force_showtris_mode( void );
/* Effective r_showtris mode: max(r_showtris, r_bspViz policy). */
int vk_bsp_viz_effective_showtris( void );

const bspVisibilityFrame_t *vk_bsp_viz_current_frame( void );

#endif
