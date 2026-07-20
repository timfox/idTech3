#pragma once

/*
 * Raster Ultra 1.6 / 1.14 — Persistent GPU scene + cull/indirect scaffolding.
 *
 * Classic BSP remains authoritative when world type is classic or metadata is absent.
 * Terrain / streamed / hybrid only become effective when metadata is present.
 * No transient CPU pointers in GPU records. RT stays off under Raster Ultra.
 */

#ifdef USE_VULKAN

#include "../common/tr_types.h"

#define VK_GPU_SCENE_MAX_INSTANCES   4096
#define VK_GPU_SCENE_MAX_MESHES      1024
#define VK_GPU_SCENE_MAX_MESHLETS    65536
#define VK_GPU_SCENE_INDIRECT_MAX    8192

typedef enum {
	VK_WORLD_TYPE_CLASSIC_BSP = 0, /* WORLD_CLASSIC_BSP — default */
	VK_WORLD_TYPE_TERRAIN     = 1, /* WORLD_TERRAIN — CBT heightfield primary */
	VK_WORLD_TYPE_STREAMED    = 2, /* WORLD_STREAMED — open-world / sector stream */
	VK_WORLD_TYPE_HYBRID      = 3  /* WORLD_HYBRID — BSP + terrain/stream */
} vkWorldType_t;

#define WORLD_CLASSIC_BSP VK_WORLD_TYPE_CLASSIC_BSP
#define WORLD_TERRAIN     VK_WORLD_TYPE_TERRAIN
#define WORLD_STREAMED    VK_WORLD_TYPE_STREAMED
#define WORLD_HYBRID      VK_WORLD_TYPE_HYBRID

typedef enum {
	VK_GPU_SCENE_STREAM_RESIDENT = 0,
	VK_GPU_SCENE_STREAM_LOADING  = 1,
	VK_GPU_SCENE_STREAM_FALLBACK = 2,
	VK_GPU_SCENE_STREAM_EVICTED  = 3
} vkGpuSceneStreamState_t;

typedef enum {
	VK_GPU_SCENE_REJECT_NONE = 0,
	VK_GPU_SCENE_REJECT_FRUSTUM,
	VK_GPU_SCENE_REJECT_HIZ,
	VK_GPU_SCENE_REJECT_LOD,
	VK_GPU_SCENE_REJECT_PORTAL,
	VK_GPU_SCENE_REJECT_STREAM,
	VK_GPU_SCENE_REJECT_OVERFLOW
} vkGpuSceneReject_t;

/* GPU-friendly instance record (no CPU pointers). */
typedef struct {
	uint32_t handle;
	uint32_t meshId;
	uint32_t materialId;
	uint32_t objectId;
	float    transform[12];     /* 3x4 row-major */
	float    prevTransform[12];
	float    mins[3];
	float    maxs[3];
	float    sphere[4];         /* xyz + radius */
	uint32_t lodLevel;
	uint32_t lodHysteresis;
	uint32_t flags;             /* bit0 static, bit1 hlod, bit2 foliage, bit3 dynamic */
	uint32_t streamState;
	uint32_t visibleAge;        /* frames visible — anti one-frame pop */
	uint32_t lastReject;
	uint32_t generation;
	uint32_t _pad;
} vkGpuSceneInstance_t;

typedef struct {
	uint32_t meshId;
	uint32_t materialId;
	uint32_t firstMeshlet;
	uint32_t meshletCount;
	uint32_t indexFirst;
	uint32_t indexCount;
	uint32_t flags;             /* hard-edge / alpha-test / skinned */
	uint32_t generation;
	float    mins[3];
	float    maxs[3];
} vkGpuSceneMesh_t;

typedef struct {
	uint32_t indexCount;
	uint32_t instanceCount;
	uint32_t firstIndex;
	int32_t  vertexOffset;
	uint32_t firstInstance;
} vkGpuSceneDrawCmd_t;

void vk_gpu_scene_register_cvars( void );
void vk_gpu_scene_init( void );
void vk_gpu_scene_shutdown( void );
void vk_gpu_scene_begin_frame( void );
void vk_gpu_scene_end_frame( void );

qboolean vk_gpu_scene_active( void );

/* Requested cvar value (may differ from effective). */
vkWorldType_t vk_gpu_scene_world_type_requested( void );
/* Effective ownership after metadata / fallback routing. */
vkWorldType_t vk_gpu_scene_world_type( void );
const char *vk_gpu_scene_world_fallback_reason( void );
qboolean vk_gpu_scene_terrain_metadata_present( void );
qboolean vk_gpu_scene_terrain_resources_ready( void );

/* Map / restart lifecycle — invalidate generation, keep classic BSP path. */
void vk_gpu_scene_on_world_load( void );
void vk_gpu_scene_on_world_unload( void );
void vk_gpu_scene_on_vid_restart( void );

uint32_t vk_gpu_scene_register_mesh( uint32_t materialId, const vec3_t mins, const vec3_t maxs,
	uint32_t flags );
uint32_t vk_gpu_scene_register_instance( uint32_t meshId, uint32_t materialId, uint32_t objectId,
	const float *axis /*3x3*/, const vec3_t origin, const vec3_t mins, const vec3_t maxs,
	uint32_t flags );
void vk_gpu_scene_update_instance_transform( uint32_t handle, const float *axis, const vec3_t origin );
void vk_gpu_scene_set_prev_transforms( void );

/* Frustum (+ optional Hi-Z) cull → compacted visible list + indirect cmds (host, no readback). */
void vk_gpu_scene_cull_and_build_indirect( void );

uint32_t vk_gpu_scene_visible_count( void );
uint32_t vk_gpu_scene_indirect_count( void );
const uint32_t *vk_gpu_scene_visible_handles( void );
const vkGpuSceneDrawCmd_t *vk_gpu_scene_indirect_cmds( void );

void vk_gpu_scene_status_f( void );

#endif /* USE_VULKAN */
