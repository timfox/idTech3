#pragma once

/*
 * GPU-Driven Visibility Milestone 1 — Phase 1: persistent GPU scene contract.
 * Clustered Hybrid M1 schema contract: one object record feeds deferred,
 * Forward+, OIT, shadows, velocity, and debug ownership without changing the
 * CPU correctness path.
 *
 * One authoritative scene database shared by:
 *   deferred opaque · Forward+ opaque fallback · alpha-test · WBOIT · shadows ·
 *   depth prepass · object-ID/debug · temporal velocity · (later) probes
 *
 * Classic BSP remains authoritative when world type is classic or metadata is absent.
 * CPU direct-draw remains the correctness reference (r_gpuDriven 0).
 * No transient CPU pointers in GPU records. RT stays off under Raster Ultra.
 */

#ifdef USE_VULKAN

#include "../common/tr_types.h"

#define VK_GPU_SCENE_MAX_INSTANCES   4096
#define VK_GPU_SCENE_MAX_MESHES      1024
#define VK_GPU_SCENE_MAX_MESHLETS    65536
#define VK_GPU_SCENE_INDIRECT_MAX    8192

typedef enum {
	VK_WORLD_TYPE_CLASSIC_BSP = 0,
	VK_WORLD_TYPE_TERRAIN     = 1,
	VK_WORLD_TYPE_STREAMED    = 2,
	VK_WORLD_TYPE_HYBRID      = 3
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
	VK_GPU_SCENE_REJECT_OVERFLOW,
	VK_GPU_SCENE_REJECT_INVALID_GEN,
	VK_GPU_SCENE_REJECT_BAD_MESH,
	VK_GPU_SCENE_REJECT_FAULT_INJECT
} vkGpuSceneReject_t;

typedef enum {
	VK_GPU_OBJ_LIFE_FREE = 0,
	VK_GPU_OBJ_LIFE_ACTIVE,
	VK_GPU_OBJ_LIFE_DESPAWNING
} vkGpuObjLifecycle_t;

typedef enum {
	VK_GPU_SRC_NONE = 0,
	VK_GPU_SRC_BSP_SURFACE,
	VK_GPU_SRC_ENTITY,
	VK_GPU_SRC_PROP,
	VK_GPU_SRC_PILOT
} vkGpuObjSource_t;

typedef enum {
	VK_GPU_INVALIDATE_NONE = 0,
	VK_GPU_INVALIDATE_SPAWN,
	VK_GPU_INVALIDATE_DESPAWN,
	VK_GPU_INVALIDATE_MODEL,
	VK_GPU_INVALIDATE_MATERIAL,
	VK_GPU_INVALIDATE_MAP_RESTART,
	VK_GPU_INVALIDATE_VID_RESTART,
	VK_GPU_INVALIDATE_TELEPORT,
	VK_GPU_INVALIDATE_ATTACHMENT,
	VK_GPU_INVALIDATE_OWNERSHIP
} vkGpuInvalidateReason_t;

/*
 * Render-path ownership on the object (material routing unchanged).
 * Path selects which shared consumer list may submit the object.
 */
typedef enum {
	VK_GPU_PATH_NONE = 0,
	VK_GPU_PATH_DEFERRED,           /* MIXED_MATERIAL_DEFERRED opaque */
	VK_GPU_PATH_FORWARD_FALLBACK,   /* Forward+ / classic opaque */
	VK_GPU_PATH_ALPHA_TEST,
	VK_GPU_PATH_TRANSPARENT,        /* WBOIT / OIT */
	VK_GPU_PATH_SHADOW,
	VK_GPU_PATH_WEAPON,             /* first-person; independently owned */
	VK_GPU_PATH_DEPTH_PREPASS,
	VK_GPU_PATH_VELOCITY,           /* temporal motion */
	VK_GPU_PATH_OBJECT_ID           /* debug / identity pass */
} vkGpuRenderPath_t;

/*
 * Canonical GPU object record (fixed-width scalars only).
 * Layout must stay 16-byte aligned for SSBO upload.
 *
 * Preferred GPU-visible contract (Milestone 1 Phase 1):
 *   currentModel / previousModel
 *   boundsSphere / boundsMin / boundsMax
 *   objectId / objectGeneration / meshId / materialId
 *   surfaceId / renderPath / renderFlags / temporalClass
 *   lightmapIndex / reflectionProbeIndex / irradianceProbeIndex / animationIndex
 *   pipelineKey / shadowFlags / visibilityFlags / instanceDataIndex
 *
 * Host-only lifecycle fields follow (never CPU pointers).
 */
typedef struct gpuSceneObject_s {
	/* --- Preferred GPU contract (see docs/GPU_SCENE.md) --- */
	float    currentModel[16];
	float    previousModel[16];

	float    boundsSphere[4];
	float    boundsMin[4];
	float    boundsMax[4];

	uint32_t objectId;
	uint32_t objectGeneration;
	uint32_t meshId;
	uint32_t materialId;

	uint32_t surfaceId;
	uint32_t renderPath;
	uint32_t renderFlags;
	uint32_t temporalClass;

	uint32_t lightmapIndex;
	uint32_t reflectionProbeIndex;
	uint32_t irradianceProbeIndex;
	uint32_t animationIndex;

	uint32_t pipelineKey;
	uint32_t shadowFlags;
	uint32_t visibilityFlags;
	uint32_t instanceDataIndex;

	/* --- Host / cull helpers (mirrored; no pointers) --- */
	uint32_t handle;
	float    transform[12];
	float    prevTransform[12];
	float    normalMatrix[12];
	float    mins[3];
	float    maxs[3];
	float    sphere[4];
	uint32_t firstIndex;
	uint32_t indexCount;
	int32_t  vertexOffset;
	uint32_t lodLevel;
	uint32_t lodHysteresis;
	uint32_t flags;
	uint32_t streamState;
	uint32_t visibleAge;
	uint32_t lastReject;
	uint32_t generation;
	uint32_t lifecycleState;
	uint32_t sourceKind;
	uint32_t sourceRef;
	uint32_t lastInvalidationReason;
	uint32_t transformFrame;
	uint32_t prevTransformFrame;
	uint32_t lastVisibleFrame;
	uint32_t lastSubmittedFrame;
	uint32_t reserved;
	uint32_t _padSSBO[2]; /* sizeof % 16 == 0 */
} gpuSceneObject_t;

/* Legacy aliases — same type. */
typedef gpuSceneObject_t GpuSceneObject;
typedef gpuSceneObject_t vkGpuSceneInstance_t;

typedef struct {
	uint32_t meshId;
	uint32_t materialId;
	uint32_t firstMeshlet;
	uint32_t meshletCount;
	uint32_t indexFirst;
	uint32_t indexCount;
	uint32_t flags;
	uint32_t generation;
	float    mins[3];
	float    maxs[3];
	uint32_t indexType; /* 0=u16 1=u32 */
	uint32_t vertexCount;
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

vkWorldType_t vk_gpu_scene_world_type_requested( void );
vkWorldType_t vk_gpu_scene_world_type( void );
const char *vk_gpu_scene_world_fallback_reason( void );
qboolean vk_gpu_scene_terrain_metadata_present( void );
qboolean vk_gpu_scene_terrain_resources_ready( void );

void vk_gpu_scene_on_world_load( void );
void vk_gpu_scene_on_world_unload( void );
void vk_gpu_scene_on_vid_restart( void );

uint32_t vk_gpu_scene_register_mesh( uint32_t materialId, const vec3_t mins, const vec3_t maxs,
	uint32_t flags );
/* Extended mesh registration with validated index range. */
uint32_t vk_gpu_scene_register_mesh_ex( uint32_t materialId, const vec3_t mins, const vec3_t maxs,
	uint32_t flags, uint32_t indexFirst, uint32_t indexCount, int32_t vertexOffset );

uint32_t vk_gpu_scene_register_instance( uint32_t meshId, uint32_t materialId, uint32_t objectId,
	const float *axis /*3x3*/, const vec3_t origin, const vec3_t mins, const vec3_t maxs,
	uint32_t flags );
uint32_t vk_gpu_scene_register_instance_ex( uint32_t meshId, uint32_t materialId, uint32_t objectId,
	const float *axis, const vec3_t origin, const vec3_t mins, const vec3_t maxs,
	uint32_t flags, uint32_t sourceKind, uint32_t sourceRef, uint32_t renderPath );

void vk_gpu_scene_unregister_instance( uint32_t handle, vkGpuInvalidateReason_t reason );
void vk_gpu_scene_invalidate_instance( uint32_t handle, vkGpuInvalidateReason_t reason );
void vk_gpu_scene_update_instance_transform( uint32_t handle, const float *axis, const vec3_t origin );
void vk_gpu_scene_set_prev_transforms( void );

void vk_gpu_scene_cull_and_build_indirect( void );

uint32_t vk_gpu_scene_visible_count( void );
uint32_t vk_gpu_scene_indirect_count( void );
uint32_t vk_gpu_scene_generation( void );
uint32_t vk_gpu_scene_frame_id( void );
qboolean vk_gpu_scene_driven_active( void );
void vk_gpu_scene_telemetry( uint32_t *outCandidates, uint32_t *outFrustumRejected,
	uint32_t *outHizRejected, uint32_t *outGeneratedDraws );
qboolean vk_gpu_scene_validate_layout( void );
const uint32_t *vk_gpu_scene_visible_handles( void );
const vkGpuSceneDrawCmd_t *vk_gpu_scene_indirect_cmds( void );
const gpuSceneObject_t *vk_gpu_scene_object_by_handle( uint32_t handle );

void vk_gpu_scene_merge_compatible_draws( uint32_t *outCmdsIn, uint32_t *outCmdsOut,
	uint32_t *outGroups, uint32_t *outSkipped );

qboolean vk_gpu_scene_indirect_buffer_ready( void );
uint32_t vk_gpu_scene_draw_count_published( void );
/* Opaque handles for draw / cull consumers — cast to VkBuffer where vk.h is included. */
void *vk_gpu_scene_indirect_buffer_handle( void );
void *vk_gpu_scene_object_buffer_handle( void );
qboolean vk_gpu_scene_object_buffer_ready( void );

/* Pilots (opt-in; do not migrate all world geometry). */
uint32_t vk_gpu_scene_pilot_register_bsp_surface( uint32_t surfaceIndex, uint32_t materialId,
	const vec3_t mins, const vec3_t maxs, uint32_t indexFirst, uint32_t indexCount,
	uint32_t renderPath );
uint32_t vk_gpu_scene_pilot_register_rigid( uint32_t entityNum, uint32_t materialId,
	const float *axis, const vec3_t origin, const vec3_t mins, const vec3_t maxs,
	uint32_t indexFirst, uint32_t indexCount );

void vk_gpu_scene_status_f( void );
void vk_gpu_scene_layout_f( void );
void vk_gpu_scene_object_status_f( void );
void vk_gpu_draw_status_f( void );
void vk_gpu_frame_ownership_status_f( void );
void vk_gpu_draw_perf_f( void );

#endif /* USE_VULKAN */
