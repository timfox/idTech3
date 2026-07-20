/*
===========================================================================
Cinematic Engine Platform 1.0 — Environment Vertical Slice.
Unified scene registry: stable IDs, authoring/runtime split, live-edit
invalidation. Does not replace BSP ownership. No RT / TAA dependence.
===========================================================================
*/

#pragma once

#ifdef USE_VULKAN

#include "../common/tr_types.h"

#define VK_SCENE_MAX_NODES       4096
#define VK_SCENE_NAME_LEN        64

typedef enum {
	VK_SCENE_KIND_NONE = 0,
	VK_SCENE_KIND_ROOT,
	VK_SCENE_KIND_BSP_WORLD,      /* classic map — not GPU-scene driven */
	VK_SCENE_KIND_STATIC_PROP,
	VK_SCENE_KIND_DYNAMIC_PROP,
	VK_SCENE_KIND_LIGHT,
	VK_SCENE_KIND_CAMERA,
	VK_SCENE_KIND_VOLUME,         /* fog / atmosphere volume */
	VK_SCENE_KIND_PROBE,
	VK_SCENE_KIND_DECAL,
	VK_SCENE_KIND_EMITTER,
	VK_SCENE_KIND_USD_PRIM,
	VK_SCENE_KIND_DISTRICT,
	VK_SCENE_KIND_COUNT
} vkSceneNodeKind_t;

typedef enum {
	VK_SCENE_DIRTY_NONE          = 0,
	VK_SCENE_DIRTY_TRANSFORM     = 1 << 0,
	VK_SCENE_DIRTY_VISIBILITY    = 1 << 1,
	VK_SCENE_DIRTY_MATERIAL      = 1 << 2,
	VK_SCENE_DIRTY_LIGHT         = 1 << 3,
	VK_SCENE_DIRTY_BOUNDS        = 1 << 4,
	VK_SCENE_DIRTY_SHADOW        = 1 << 5,
	VK_SCENE_DIRTY_GI            = 1 << 6,
	VK_SCENE_DIRTY_PROBE         = 1 << 7,
	VK_SCENE_DIRTY_VOLUME        = 1 << 8
} vkSceneDirtyFlags_t;

/* Stable ID: never a raw pointer. Packed type + sequential index + generation. */
typedef uint64_t vkSceneId_t;

typedef struct vkSceneNode_s {
	vkSceneId_t       id;
	vkSceneId_t       parent;
	vkSceneNodeKind_t kind;
	char              name[VK_SCENE_NAME_LEN];
	float             localOrigin[3];
	float             localAxis[3][3];
	float             worldOrigin[3];
	float             worldAxis[3][3];
	float             prevWorldOrigin[3];
	float             mins[3];
	float             maxs[3];
	qboolean          visible;
	uint32_t          layer;
	uint32_t          materialId;     /* runtime material index / 0 */
	uint32_t          lightIndex;     /* dlight index when kind==LIGHT; ~0 none */
	uint32_t          gpuHandle;      /* vk_gpu_scene instance handle; 0 = none */
	uint32_t          revision;
	uint32_t          dirty;
	uint32_t          streamingCell;
	qboolean          alive;
} vkSceneNode_t;

typedef struct vkSceneInvalidationEvent_s {
	vkSceneId_t id;
	uint32_t    flags;
	uint32_t    frame;
} vkSceneInvalidationEvent_t;

#define VK_SCENE_INVALIDATION_LOG 64

typedef struct vkScenePlatformState_s {
	uint32_t nodeCount;
	uint32_t aliveCount;
	uint32_t sceneRevision;
	uint32_t compileRevision;   /* authoring→runtime compile bump */
	uint32_t editCount;
	uint32_t invalidationCount;
	uint32_t lastInvalidationFlags;
	qboolean liveEdit;
} vkScenePlatformState_t;

void vk_scene_platform_register_cvars( void );
void vk_scene_platform_init( void );
void vk_scene_platform_shutdown( void );
void vk_scene_platform_begin_frame( void );

qboolean vk_scene_platform_active( void );
const vkScenePlatformState_t *vk_scene_platform_state( void );

void vk_scene_platform_on_world_load( void );
void vk_scene_platform_on_world_unload( void );
void vk_scene_platform_on_vid_restart( void );

/* Ensure a root + BSP world node exist (classic ownership). */
void vk_scene_platform_ensure_world_nodes( void );

vkSceneId_t vk_scene_platform_create_node( vkSceneNodeKind_t kind, const char *name,
	vkSceneId_t parent );
qboolean vk_scene_platform_find( vkSceneId_t id, vkSceneNode_t **out );
const vkSceneNode_t *vk_scene_platform_get( vkSceneId_t id );

/* Live edits — selective invalidation, no renderer restart. */
qboolean vk_scene_platform_edit_transform( vkSceneId_t id, const vec3_t origin,
	const float *axis9 /* optional row-major 3x3, NULL = identity */ );
qboolean vk_scene_platform_edit_visibility( vkSceneId_t id, qboolean visible );
qboolean vk_scene_platform_edit_material( vkSceneId_t id, uint32_t materialId );
qboolean vk_scene_platform_link_gpu_instance( vkSceneId_t id, uint32_t gpuHandle );
qboolean vk_scene_platform_link_light( vkSceneId_t id, uint32_t lightIndex );

void vk_scene_platform_note_invalidation( vkSceneId_t id, uint32_t flags );
uint32_t vk_scene_platform_consume_pending_dirty( void );

void vk_scene_platform_status_f( void );
void vk_scene_platform_node_status_f( void );
void vk_scene_platform_invalidate_debug_f( void );

const char *vk_scene_platform_kind_name( vkSceneNodeKind_t k );

#endif /* USE_VULKAN */
