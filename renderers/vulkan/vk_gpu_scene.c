/*
===========================================================================
Raster Ultra 1.6 — Persistent GPU scene records + frustum cull + indirect.

Classic BSP remains the default world path. GPU scene augments static /
repeated props. No CPU pointers in GPU records. No frame generation.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_gpu_scene.h"
#include "vk_gpu_visibility.h"
#include "vk_gpu_frustum_math.h"
#include "vk_hiz.h"
#include "vk_meshlets.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include "vk_pass_registry.h"
#include "vk_raster_ultra.h"
#include "vk_terrain.h"

#include <math.h>
#include <stddef.h>

static_assert( sizeof( gpuSceneObject_t ) % 16u == 0u, "gpuSceneObject_t SSBO align" );
static_assert( sizeof( vkGpuSceneDrawCmd_t ) == 20u || sizeof( vkGpuSceneDrawCmd_t ) == 24u,
	"indirect cmd size unexpected" );

static cvar_t *r_gpuScene;
static cvar_t *r_gpuSceneCull;
static cvar_t *r_gpuSceneIndirect;
static cvar_t *r_gpuSceneDebug;
static cvar_t *r_gpuSceneWorldType;
static cvar_t *r_gpuSceneHlod;
static cvar_t *r_gpuSceneLodHysteresis;
static cvar_t *r_gpuDriven;
static cvar_t *r_gpuDrawCompare;
static cvar_t *r_gpuSceneBspPilot;
static cvar_t *r_gpuSceneDynamicPilot;
static cvar_t *r_gpuSceneMaxObjects;
static cvar_t *r_gpuDrawMaxCommands;
static cvar_t *r_gpuVisibilityMaxCandidates;
static cvar_t *r_gpuDrawForceStaleCommand;
static cvar_t *r_gpuDrawForceBadFirstIndex;
static cvar_t *r_gpuDrawForceBadVertexOffset;
static cvar_t *r_gpuDrawForceBadInstance;
static cvar_t *r_gpuDrawForceCountOverflow;
static cvar_t *r_gpuLOD;
static cvar_t *r_gpuLODDebug;
static cvar_t *r_gpuLODBias;
static cvar_t *r_gpuLODHysteresis;

static char s_worldFallbackReason[96];

static vkGpuSceneInstance_t s_instances[VK_GPU_SCENE_MAX_INSTANCES];
static vkGpuSceneMesh_t     s_meshes[VK_GPU_SCENE_MAX_MESHES];
static uint32_t s_freeList[VK_GPU_SCENE_MAX_INSTANCES];
static uint32_t s_freeCount;
static uint32_t s_instanceCount;
static uint32_t s_meshCount;
static uint32_t s_nextHandle = 1;
static uint32_t s_generation = 1;
static uint32_t s_frameId = 1;
static uint32_t s_sceneUpdateFrame;
static uint32_t s_objectTransformFrame;
static uint32_t s_visibilityFrame;
static uint32_t s_indirectCommandFrame;
static uint32_t s_drawCountPublished;
static uint32_t s_staleCommandRejects;
static uint32_t s_invalidCommandRejects;
static uint32_t s_fallbackObjects;
static uint32_t s_bspPilotCount;
static uint32_t s_dynamicPilotCount;

static uint32_t s_visible[VK_GPU_SCENE_MAX_INSTANCES];
static uint32_t s_visibleCount;
static vkGpuSceneDrawCmd_t s_indirect[VK_GPU_SCENE_INDIRECT_MAX];
static uint32_t s_indirectCount;
static uint32_t s_listCounts[VK_GPU_DRAW_LIST_COUNT];

static uint32_t s_rejectFrustum;
static uint32_t s_rejectHiz;
static uint32_t s_rejectLod;
static uint32_t s_rejectStream;
static uint32_t s_cullFrames;
static qboolean s_cmds;
static qboolean s_logged;

/* Host-visible indirect buffer + persistent object SSBO. */
static VkBuffer s_indirectBuf;
static VkDeviceMemory s_indirectMem;
static void *s_indirectMapped;
static VkBuffer s_drawCountBuf;
static VkDeviceMemory s_drawCountMem;
static uint32_t *s_drawCountMapped;
static VkBuffer s_objectBuf;
static VkDeviceMemory s_objectMem;
static void *s_objectMapped;
static uint32_t s_objectUploadFrame;

static void GPUScene_PackTransform( float out12[12], const float axis[3][3], const vec3_t origin )
{
	out12[0] = axis[0][0]; out12[1] = axis[0][1]; out12[2] = axis[0][2]; out12[3] = origin[0];
	out12[4] = axis[1][0]; out12[5] = axis[1][1]; out12[6] = axis[1][2]; out12[7] = origin[1];
	out12[8] = axis[2][0]; out12[9] = axis[2][1]; out12[10] = axis[2][2]; out12[11] = origin[2];
}

static void GPUScene_SyncModelMats( vkGpuSceneInstance_t *inst )
{
	float *m = inst->currentModel;
	const float *t = inst->transform;
	Com_Memset( m, 0, sizeof( inst->currentModel ) );
	m[0] = t[0]; m[1] = t[1]; m[2] = t[2]; m[3] = 0.0f;
	m[4] = t[4]; m[5] = t[5]; m[6] = t[6]; m[7] = 0.0f;
	m[8] = t[8]; m[9] = t[9]; m[10] = t[10]; m[11] = 0.0f;
	m[12] = t[3]; m[13] = t[7]; m[14] = t[11]; m[15] = 1.0f;
	Com_Memcpy( inst->boundsSphere, inst->sphere, sizeof( inst->boundsSphere ) );
	inst->boundsMin[0] = inst->mins[0]; inst->boundsMin[1] = inst->mins[1];
	inst->boundsMin[2] = inst->mins[2]; inst->boundsMin[3] = 0.0f;
	inst->boundsMax[0] = inst->maxs[0]; inst->boundsMax[1] = inst->maxs[1];
	inst->boundsMax[2] = inst->maxs[2]; inst->boundsMax[3] = 0.0f;
}

static void GPUScene_SphereFromBounds( float sphere[4], const vec3_t mins, const vec3_t maxs )
{
	vec3_t c, e;
	VectorAdd( mins, maxs, c );
	VectorScale( c, 0.5f, c );
	VectorSubtract( maxs, mins, e );
	sphere[0] = c[0];
	sphere[1] = c[1];
	sphere[2] = c[2];
	sphere[3] = VectorLength( e ) * 0.5f;
}

static vkGpuSceneInstance_t *GPUScene_FindByHandle( uint32_t handle )
{
	uint32_t i;
	if ( handle == 0 ) {
		return NULL;
	}
	for ( i = 0; i < s_instanceCount; i++ ) {
		if ( s_instances[i].handle == handle &&
			s_instances[i].lifecycleState == VK_GPU_OBJ_LIFE_ACTIVE ) {
			return &s_instances[i];
		}
	}
	return NULL;
}

static void GPUScene_DestroyBuffers( void )
{
	if ( s_indirectMapped && s_indirectMem != VK_NULL_HANDLE ) {
		qvkUnmapMemory( vk.device, s_indirectMem );
		s_indirectMapped = NULL;
	}
	if ( s_indirectBuf != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, s_indirectBuf, NULL );
		s_indirectBuf = VK_NULL_HANDLE;
	}
	if ( s_indirectMem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, s_indirectMem, NULL );
		s_indirectMem = VK_NULL_HANDLE;
	}
	if ( s_drawCountMapped && s_drawCountMem != VK_NULL_HANDLE ) {
		qvkUnmapMemory( vk.device, s_drawCountMem );
		s_drawCountMapped = NULL;
	}
	if ( s_drawCountBuf != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, s_drawCountBuf, NULL );
		s_drawCountBuf = VK_NULL_HANDLE;
	}
	if ( s_drawCountMem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, s_drawCountMem, NULL );
		s_drawCountMem = VK_NULL_HANDLE;
	}
	if ( s_objectMapped && s_objectMem != VK_NULL_HANDLE ) {
		qvkUnmapMemory( vk.device, s_objectMem );
		s_objectMapped = NULL;
	}
	if ( s_objectBuf != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, s_objectBuf, NULL );
		s_objectBuf = VK_NULL_HANDLE;
	}
	if ( s_objectMem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, s_objectMem, NULL );
		s_objectMem = VK_NULL_HANDLE;
	}
	s_objectUploadFrame = 0;
}

static void GPUScene_DestroyIndirectBuf( void )
{
	GPUScene_DestroyBuffers();
}

static qboolean GPUScene_CreateHostVisibleBuffer( VkDeviceSize size, VkBufferUsageFlags usage,
	VkBuffer *outBuf, VkDeviceMemory *outMem, void **outMapped )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo mai;

	Com_Memset( &bci, 0, sizeof( bci ) );
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = size;
	bci.usage = usage;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if ( qvkCreateBuffer( vk.device, &bci, NULL, outBuf ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetBufferMemoryRequirements( vk.device, *outBuf, &req );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	if ( qvkAllocateMemory( vk.device, &mai, NULL, outMem ) != VK_SUCCESS ) {
		qvkDestroyBuffer( vk.device, *outBuf, NULL );
		*outBuf = VK_NULL_HANDLE;
		return qfalse;
	}
	qvkBindBufferMemory( vk.device, *outBuf, *outMem, 0 );
	if ( qvkMapMemory( vk.device, *outMem, 0, size, 0, outMapped ) != VK_SUCCESS ) {
		qvkFreeMemory( vk.device, *outMem, NULL );
		qvkDestroyBuffer( vk.device, *outBuf, NULL );
		*outBuf = VK_NULL_HANDLE;
		*outMem = VK_NULL_HANDLE;
		*outMapped = NULL;
		return qfalse;
	}
	return qtrue;
}

static qboolean GPUScene_EnsureIndirectBuf( void )
{
	const VkDeviceSize cmdSize = sizeof( vkGpuSceneDrawCmd_t ) * VK_GPU_SCENE_INDIRECT_MAX;
	const VkDeviceSize objSize = sizeof( gpuSceneObject_t ) * VK_GPU_SCENE_MAX_INSTANCES;
	const VkBufferUsageFlags cmdUsage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const VkBufferUsageFlags objUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	if ( s_indirectMapped && s_objectMapped ) {
		return qtrue;
	}
	if ( vk.device == VK_NULL_HANDLE ) {
		return qfalse;
	}

	if ( !s_indirectMapped ) {
		if ( !GPUScene_CreateHostVisibleBuffer( cmdSize, cmdUsage,
				&s_indirectBuf, &s_indirectMem, &s_indirectMapped ) ) {
			GPUScene_DestroyBuffers();
			return qfalse;
		}
	}

	/* Exact draw-count buffer (IndirectCount / CPU fallback). */
	if ( !s_drawCountMapped ) {
		void *mapped = NULL;
		if ( GPUScene_CreateHostVisibleBuffer( sizeof( uint32_t ) * 4u, cmdUsage,
				&s_drawCountBuf, &s_drawCountMem, &mapped ) ) {
			s_drawCountMapped = (uint32_t *)mapped;
			s_drawCountMapped[0] = 0u;
		}
	}

	/* Persistent object SSBO — shared cull/draw/debug consumers (Phase 1). */
	if ( !s_objectMapped ) {
		if ( !GPUScene_CreateHostVisibleBuffer( objSize, objUsage,
				&s_objectBuf, &s_objectMem, &s_objectMapped ) ) {
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][GPUScene] object SSBO alloc failed — host records only\n" S_COLOR_WHITE );
		} else {
			Com_Memset( s_objectMapped, 0, (size_t)objSize );
			ri.Printf( PRINT_ALL,
				"[VK][GPUScene] persistent object SSBO ready (%u × %zu bytes)\n",
				VK_GPU_SCENE_MAX_INSTANCES, sizeof( gpuSceneObject_t ) );
		}
	}
	return qtrue;
}

static void GPUScene_UploadObjects( void )
{
	if ( !s_objectMapped ) {
		return;
	}
	Com_Memcpy( s_objectMapped, s_instances,
		sizeof( gpuSceneObject_t ) * VK_GPU_SCENE_MAX_INSTANCES );
	s_objectUploadFrame = s_frameId;
}

void vk_gpu_scene_register_cvars( void )
{
	if ( r_gpuScene ) {
		return;
	}
	r_gpuScene = ri.Cvar_Get( "r_gpuScene", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_gpuScene, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_gpuScene,
		"Raster Ultra 1.6 persistent GPU scene (latched).\n"
		" 0 off — classic BSP / CPU drawSurfs only (default)\n"
		" 1 enable instance/mesh records + cull/indirect scaffolding\n"
		"Does not replace classic maps; world type routes ownership." );
	ri.Cvar_SetGroup( r_gpuScene, CVG_RENDERER );

	r_gpuSceneCull = ri.Cvar_Get( "r_gpuSceneCull", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_gpuSceneCull, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_gpuSceneCull,
		"When r_gpuScene 1: frustum (+ Hi-Z companion) cull into compacted visible list." );

	r_gpuSceneIndirect = ri.Cvar_Get( "r_gpuSceneIndirect", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_gpuSceneIndirect, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_gpuSceneIndirect,
		"When r_gpuScene 1: pack VkDrawIndexedIndirectCommand list (host; no readback)." );

	r_gpuSceneWorldType = ri.Cvar_Get( "r_gpuSceneWorldType", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_gpuSceneWorldType, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_gpuSceneWorldType,
		"World ownership (Raster Ultra 1.14):\n"
		" 0 classic BSP (default — always safe)\n"
		" 1 terrain (CBT heightfield; requires terrain metadata)\n"
		" 2 streamed (open-world / sector stream metadata)\n"
		" 3 hybrid (BSP + terrain/stream augment)\n"
		"Absent metadata always routes as classic BSP — classic maps stay valid." );

	r_gpuSceneHlod = ri.Cvar_Get( "r_gpuSceneHlod", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_gpuSceneHlod, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_gpuSceneHlod,
		"Enable HLOD instance flag handling for distant aggregates (static only)." );

	r_gpuSceneLodHysteresis = ri.Cvar_Get( "r_gpuSceneLodHysteresis", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_gpuSceneLodHysteresis, "0", "1", CV_INTEGER );

	r_gpuSceneDebug = ri.Cvar_Get( "r_gpuSceneDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_gpuSceneDebug, "0", "9", CV_INTEGER );
	ri.Cvar_SetDescription( r_gpuSceneDebug,
		"GPU scene debug 0-9 (object/material/mesh/temporal/path/gen/prev/shadow/probe)." );

	r_gpuDriven = ri.Cvar_Get( "r_gpuDriven", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_gpuDriven, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_gpuDriven,
		"GPU-driven draw submission:\n"
		" 0 direct CPU submission (safe default)\n"
		" 1 GPU visibility + indirect lists\n"
		" 2 comparison mode (direct + GPU metrics)" );
	ri.Cvar_SetGroup( r_gpuDriven, CVG_RENDERER );
	r_gpuDrawCompare = ri.Cvar_Get( "r_gpuDrawCompare", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_gpuDrawCompare, "0", "9", CV_INTEGER );
	ri.Cvar_SetDescription( r_gpuDrawCompare,
		"Compare direct vs GPU-driven: 1 color 2 depth 3 objectID 4 material 5 ownership "
		"6 visible set 7 command fields 8 missing 9 duplicates" );
	ri.Cvar_SetGroup( r_gpuDrawCompare, CVG_RENDERER );

	r_gpuSceneBspPilot = ri.Cvar_Get( "r_gpuSceneBspPilot", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_gpuSceneBspPilot, "0", "1", CV_INTEGER );
	r_gpuSceneDynamicPilot = ri.Cvar_Get( "r_gpuSceneDynamicPilot", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_gpuSceneDynamicPilot, "0", "1", CV_INTEGER );

	r_gpuSceneMaxObjects = ri.Cvar_Get( "r_gpuSceneMaxObjects", "4096", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_gpuDrawMaxCommands = ri.Cvar_Get( "r_gpuDrawMaxCommands", "8192", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_gpuVisibilityMaxCandidates = ri.Cvar_Get( "r_gpuVisibilityMaxCandidates", "8192", CVAR_ARCHIVE_ND );

	r_gpuDrawForceStaleCommand = ri.Cvar_Get( "r_gpuDrawForceStaleCommand", "0", CVAR_CHEAT );
	r_gpuDrawForceBadFirstIndex = ri.Cvar_Get( "r_gpuDrawForceBadFirstIndex", "0", CVAR_CHEAT );
	r_gpuDrawForceBadVertexOffset = ri.Cvar_Get( "r_gpuDrawForceBadVertexOffset", "0", CVAR_CHEAT );
	r_gpuDrawForceBadInstance = ri.Cvar_Get( "r_gpuDrawForceBadInstance", "0", CVAR_CHEAT );
	r_gpuDrawForceCountOverflow = ri.Cvar_Get( "r_gpuDrawForceCountOverflow", "0", CVAR_CHEAT );

	r_gpuLOD = ri.Cvar_Get( "r_gpuLOD", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_gpuLOD, "0", "1", CV_INTEGER );
	r_gpuLODDebug = ri.Cvar_Get( "r_gpuLODDebug", "0", CVAR_CHEAT );
	r_gpuLODBias = ri.Cvar_Get( "r_gpuLODBias", "0", CVAR_ARCHIVE_ND );
	r_gpuLODHysteresis = ri.Cvar_Get( "r_gpuLODHysteresis", "1", CVAR_ARCHIVE_ND );
}

void vk_gpu_scene_init( void )
{
	vk_gpu_scene_register_cvars();
	vk_hiz_init();
	vk_gpu_visibility_init();

	s_instanceCount = 0;
	s_meshCount = 0;
	s_freeCount = 0;
	s_visibleCount = 0;
	s_indirectCount = 0;
	s_drawCountPublished = 0;
	s_staleCommandRejects = 0;
	s_invalidCommandRejects = 0;
	s_fallbackObjects = 0;
	s_bspPilotCount = 0;
	s_dynamicPilotCount = 0;
	Com_Memset( s_indirect, 0, sizeof( s_indirect ) );
	Com_Memset( s_listCounts, 0, sizeof( s_listCounts ) );
	if ( s_indirectMapped ) {
		Com_Memset( s_indirectMapped, 0, sizeof( vkGpuSceneDrawCmd_t ) * VK_GPU_SCENE_INDIRECT_MAX );
	}
	if ( s_drawCountMapped ) {
		s_drawCountMapped[0] = 0u;
	}
	s_nextHandle = 1;
	s_generation = 1;
	s_frameId = 1;
	s_rejectFrustum = s_rejectHiz = s_rejectLod = s_rejectStream = 0;
	s_cullFrames = 0;

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "gpu_scene_status", vk_gpu_scene_status_f );
		ri.Cmd_AddCommand( "gpu_scene_layout", vk_gpu_scene_layout_f );
		ri.Cmd_AddCommand( "gpu_scene_object_status", vk_gpu_scene_object_status_f );
		ri.Cmd_AddCommand( "gpu_scene_inspect", vk_gpu_scene_object_status_f );
		ri.Cmd_AddCommand( "gpu_draw_status", vk_gpu_draw_status_f );
		ri.Cmd_AddCommand( "gpu_frame_ownership_status", vk_gpu_frame_ownership_status_f );
		ri.Cmd_AddCommand( "gpu_draw_perf", vk_gpu_draw_perf_f );
		ri.Cmd_AddCommand( "gpu_visibility_perf", vk_gpu_visibility_perf_f );
		s_cmds = qtrue;
	}

	if ( r_gpuScene && r_gpuScene->integer ) {
		GPUScene_EnsureIndirectBuf();
		if ( !s_logged ) {
			const size_t objBytes = sizeof( gpuSceneObject_t ) * VK_GPU_SCENE_MAX_INSTANCES;
			const size_t cmdBytes = sizeof( vkGpuSceneDrawCmd_t ) * VK_GPU_SCENE_INDIRECT_MAX;
			ri.Printf( PRINT_ALL,
				"[VK][GPUScene] M1 Phase1: objectSize=%zu align16=%s maxObj=%u maxMesh=%u "
				"cmdCap=%u mem≈%zuKiB objectSSBO=%s gen=%u worldType=%d\n",
				sizeof( gpuSceneObject_t ),
				( sizeof( gpuSceneObject_t ) % 16u ) == 0u ? "yes" : "NO",
				VK_GPU_SCENE_MAX_INSTANCES, VK_GPU_SCENE_MAX_MESHES,
				VK_GPU_SCENE_INDIRECT_MAX,
				( objBytes + cmdBytes ) / 1024u,
				vk_gpu_scene_object_buffer_ready() ? "yes" : "no",
				s_generation,
				r_gpuSceneWorldType ? r_gpuSceneWorldType->integer : 0 );
			s_logged = qtrue;
		}
	}
}

void vk_gpu_scene_shutdown( void )
{
	GPUScene_DestroyIndirectBuf();
	vk_hiz_shutdown();
	vk_gpu_visibility_shutdown();
	s_instanceCount = 0;
	s_meshCount = 0;
	s_freeCount = 0;
	s_visibleCount = 0;
	s_indirectCount = 0;
	Com_Memset( s_indirect, 0, sizeof( s_indirect ) );
	if ( s_indirectMapped ) {
		Com_Memset( s_indirectMapped, 0, sizeof( vkGpuSceneDrawCmd_t ) * VK_GPU_SCENE_INDIRECT_MAX );
	}
}

qboolean vk_gpu_scene_active( void )
{
	return ( r_gpuScene && r_gpuScene->integer ) ? qtrue : qfalse;
}

vkWorldType_t vk_gpu_scene_world_type_requested( void )
{
	int t;

	if ( !r_gpuSceneWorldType ) {
		return VK_WORLD_TYPE_CLASSIC_BSP;
	}
	t = r_gpuSceneWorldType->integer;
	if ( t < 0 || t > (int)VK_WORLD_TYPE_HYBRID ) {
		return VK_WORLD_TYPE_CLASSIC_BSP;
	}
	return (vkWorldType_t)t;
}

qboolean vk_gpu_scene_terrain_metadata_present( void )
{
	return CBTerrain_HasMetadata();
}

qboolean vk_gpu_scene_terrain_resources_ready( void )
{
	return CBTerrain_ResourcesReady();
}

const char *vk_gpu_scene_world_fallback_reason( void )
{
	return s_worldFallbackReason[0] ? s_worldFallbackReason : "none";
}

vkWorldType_t vk_gpu_scene_world_type( void )
{
	vkWorldType_t requested = vk_gpu_scene_world_type_requested();
	qboolean hasWorld = ( tr.world && tr.world->name[0] ) ? qtrue : qfalse;
	qboolean hasTerrain = CBTerrain_HasMetadata();
	qboolean hasStream = qfalse;

	s_worldFallbackReason[0] = '\0';

	if ( !hasWorld ) {
		Q_strncpyz( s_worldFallbackReason, "no_world", sizeof( s_worldFallbackReason ) );
		return VK_WORLD_TYPE_CLASSIC_BSP;
	}

	/* Streamed metadata: open-world / BSP stream residency (soft signal). */
	if ( ri.Cvar_VariableIntegerValue( "r_openWorld" ) ) {
		hasStream = qtrue;
	}

	switch ( requested ) {
	case VK_WORLD_TYPE_CLASSIC_BSP:
		return VK_WORLD_TYPE_CLASSIC_BSP;

	case VK_WORLD_TYPE_TERRAIN:
		if ( hasTerrain ) {
			return VK_WORLD_TYPE_TERRAIN;
		}
		Q_strncpyz( s_worldFallbackReason, "no_terrain_metadata", sizeof( s_worldFallbackReason ) );
		return VK_WORLD_TYPE_CLASSIC_BSP;

	case VK_WORLD_TYPE_STREAMED:
		if ( hasStream ) {
			return VK_WORLD_TYPE_STREAMED;
		}
		Q_strncpyz( s_worldFallbackReason, "no_stream_metadata", sizeof( s_worldFallbackReason ) );
		return VK_WORLD_TYPE_CLASSIC_BSP;

	case VK_WORLD_TYPE_HYBRID:
		if ( hasTerrain || hasStream ) {
			return VK_WORLD_TYPE_HYBRID;
		}
		Q_strncpyz( s_worldFallbackReason, "no_hybrid_metadata", sizeof( s_worldFallbackReason ) );
		return VK_WORLD_TYPE_CLASSIC_BSP;

	default:
		Q_strncpyz( s_worldFallbackReason, "invalid_request", sizeof( s_worldFallbackReason ) );
		return VK_WORLD_TYPE_CLASSIC_BSP;
	}
}

void vk_gpu_scene_on_world_load( void )
{
	s_generation++;
	s_instanceCount = 0;
	s_meshCount = 0;
	s_freeCount = 0;
	s_visibleCount = 0;
	s_indirectCount = 0;
	s_bspPilotCount = 0;
	s_dynamicPilotCount = 0;
	Com_Memset( s_indirect, 0, sizeof( s_indirect ) );
	if ( s_indirectMapped ) {
		Com_Memset( s_indirectMapped, 0, sizeof( vkGpuSceneDrawCmd_t ) * VK_GPU_SCENE_INDIRECT_MAX );
	}
	if ( s_drawCountMapped ) {
		s_drawCountMapped[0] = 0u;
	}
	vk_hiz_on_camera_cut();
	R_Meshlets_InvalidateCache();
	ri.Printf( PRINT_DEVELOPER, "[VK][GPUScene] world load gen=%u (classic BSP path intact)\n",
		s_generation );
}

void vk_gpu_scene_on_world_unload( void )
{
	s_instanceCount = 0;
	s_meshCount = 0;
	s_visibleCount = 0;
	s_indirectCount = 0;
	Com_Memset( s_indirect, 0, sizeof( s_indirect ) );
	if ( s_indirectMapped ) {
		Com_Memset( s_indirectMapped, 0, sizeof( vkGpuSceneDrawCmd_t ) * VK_GPU_SCENE_INDIRECT_MAX );
	}
	vk_hiz_on_camera_cut();
}

void vk_gpu_scene_on_vid_restart( void )
{
	GPUScene_DestroyIndirectBuf();
	vk_hiz_on_resize();
	if ( vk_gpu_scene_active() ) {
		GPUScene_EnsureIndirectBuf();
	}
	s_generation++;
	/* Keep instance table across soft restart; invalidate draw buffers only. */
	s_visibleCount = 0;
	s_indirectCount = 0;
	Com_Memset( s_indirect, 0, sizeof( s_indirect ) );
	if ( s_indirectMapped ) {
		Com_Memset( s_indirectMapped, 0, sizeof( vkGpuSceneDrawCmd_t ) * VK_GPU_SCENE_INDIRECT_MAX );
	}
}

void vk_gpu_scene_begin_frame( void )
{
	if ( !vk_gpu_scene_active() ) {
		return;
	}
	s_frameId++;
	s_sceneUpdateFrame = s_frameId;
	s_visibleCount = 0;
	s_indirectCount = 0;
	s_drawCountPublished = 0;
	Com_Memset( s_indirect, 0, sizeof( s_indirect ) );
	Com_Memset( s_listCounts, 0, sizeof( s_listCounts ) );
	if ( s_indirectMapped ) {
		Com_Memset( s_indirectMapped, 0, sizeof( vkGpuSceneDrawCmd_t ) * VK_GPU_SCENE_INDIRECT_MAX );
	}
	if ( s_drawCountMapped ) {
		s_drawCountMapped[0] = 0u;
	}
	s_rejectFrustum = s_rejectHiz = s_rejectLod = s_rejectStream = 0;
	vk_gpu_visibility_begin_frame();
}

void vk_gpu_scene_end_frame( void )
{
	if ( !vk_gpu_scene_active() ) {
		return;
	}
	vk_gpu_scene_set_prev_transforms();
}

uint32_t vk_gpu_scene_register_mesh( uint32_t materialId, const vec3_t mins, const vec3_t maxs,
	uint32_t flags )
{
	return vk_gpu_scene_register_mesh_ex( materialId, mins, maxs, flags, 0u, 0u, 0 );
}

uint32_t vk_gpu_scene_register_mesh_ex( uint32_t materialId, const vec3_t mins, const vec3_t maxs,
	uint32_t flags, uint32_t indexFirst, uint32_t indexCount, int32_t vertexOffset )
{
	vkGpuSceneMesh_t *m;

	if ( !vk_gpu_scene_active() || s_meshCount >= VK_GPU_SCENE_MAX_MESHES ) {
		return 0;
	}
	m = &s_meshes[s_meshCount];
	Com_Memset( m, 0, sizeof( *m ) );
	m->meshId = s_meshCount + 1;
	m->materialId = materialId;
	m->flags = flags;
	m->generation = s_generation;
	m->indexFirst = indexFirst;
	m->indexCount = indexCount;
	(void)vertexOffset;
	VectorCopy( mins, m->mins );
	VectorCopy( maxs, m->maxs );
	s_meshCount++;
	return m->meshId;
}

uint32_t vk_gpu_scene_register_instance( uint32_t meshId, uint32_t materialId, uint32_t objectId,
	const float *axis, const vec3_t origin, const vec3_t mins, const vec3_t maxs,
	uint32_t flags )
{
	return vk_gpu_scene_register_instance_ex( meshId, materialId, objectId, axis, origin, mins, maxs,
		flags, VK_GPU_SRC_NONE, 0u, VK_GPU_PATH_FORWARD_FALLBACK );
}

uint32_t vk_gpu_scene_register_instance_ex( uint32_t meshId, uint32_t materialId, uint32_t objectId,
	const float *axis, const vec3_t origin, const vec3_t mins, const vec3_t maxs,
	uint32_t flags, uint32_t sourceKind, uint32_t sourceRef, uint32_t renderPath )
{
	vkGpuSceneInstance_t *inst;
	float ax[3][3];
	int i;
	uint32_t slot;

	if ( !vk_gpu_scene_active() ) {
		return 0;
	}
	if ( !axis || !origin || !mins || !maxs ) {
		return 0;
	}

	if ( s_freeCount > 0u ) {
		slot = s_freeList[--s_freeCount];
		inst = &s_instances[slot];
		inst->objectGeneration++;
	} else {
		if ( s_instanceCount >= VK_GPU_SCENE_MAX_INSTANCES ) {
			s_fallbackObjects++;
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][GPUScene] capacity exceeded (%u) — excess falls back to direct\n" S_COLOR_WHITE,
				VK_GPU_SCENE_MAX_INSTANCES );
			return 0;
		}
		slot = s_instanceCount++;
		inst = &s_instances[slot];
		Com_Memset( inst, 0, sizeof( *inst ) );
		inst->objectGeneration = 1u;
	}

	for ( i = 0; i < 3; i++ ) {
		ax[i][0] = axis[i * 3 + 0];
		ax[i][1] = axis[i * 3 + 1];
		ax[i][2] = axis[i * 3 + 2];
	}

	inst->handle = s_nextHandle++;
	if ( s_nextHandle == 0 ) {
		s_nextHandle = 1;
	}
	inst->meshId = meshId;
	inst->materialId = materialId;
	inst->objectId = objectId ? objectId : inst->handle;
	GPUScene_PackTransform( inst->transform, ax, origin );
	Com_Memcpy( inst->prevTransform, inst->transform, sizeof( inst->prevTransform ) );
	GPUScene_SyncModelMats( inst );
	Com_Memcpy( inst->previousModel, inst->currentModel, sizeof( inst->previousModel ) );
	VectorCopy( mins, inst->mins );
	VectorCopy( maxs, inst->maxs );
	GPUScene_SphereFromBounds( inst->sphere, mins, maxs );
	GPUScene_SyncModelMats( inst );
	inst->flags = flags;
	inst->streamState = VK_GPU_SCENE_STREAM_RESIDENT;
	inst->generation = s_generation;
	inst->visibleAge = 0;
	inst->lifecycleState = VK_GPU_OBJ_LIFE_ACTIVE;
	inst->sourceKind = sourceKind;
	inst->sourceRef = sourceRef;
	inst->renderPath = renderPath;
	inst->lastInvalidationReason = VK_GPU_INVALIDATE_SPAWN;
	inst->transformFrame = s_frameId;
	inst->prevTransformFrame = s_frameId;
	{
		uint32_t mi;
		for ( mi = 0; mi < s_meshCount; mi++ ) {
			if ( s_meshes[mi].meshId == meshId ) {
				inst->firstIndex = s_meshes[mi].indexFirst;
				inst->indexCount = s_meshes[mi].indexCount;
				break;
			}
		}
	}
	return inst->handle;
}

void vk_gpu_scene_unregister_instance( uint32_t handle, vkGpuInvalidateReason_t reason )
{
	vkGpuSceneInstance_t *inst = GPUScene_FindByHandle( handle );
	uint32_t i;

	if ( !inst || inst->lifecycleState == VK_GPU_OBJ_LIFE_FREE ) {
		return;
	}
	inst->lifecycleState = VK_GPU_OBJ_LIFE_FREE;
	inst->lastInvalidationReason = reason;
	inst->visibleAge = 0;
	inst->handle = 0; /* prevent stale handle match */
	inst->objectGeneration++;
	for ( i = 0; i < s_instanceCount; i++ ) {
		if ( &s_instances[i] == inst ) {
			if ( s_freeCount < VK_GPU_SCENE_MAX_INSTANCES ) {
				s_freeList[s_freeCount++] = i;
			}
			break;
		}
	}
}

void vk_gpu_scene_invalidate_instance( uint32_t handle, vkGpuInvalidateReason_t reason )
{
	vkGpuSceneInstance_t *inst = GPUScene_FindByHandle( handle );
	if ( !inst ) {
		return;
	}
	inst->lastInvalidationReason = reason;
	Com_Memcpy( inst->prevTransform, inst->transform, sizeof( inst->prevTransform ) );
	Com_Memcpy( inst->previousModel, inst->currentModel, sizeof( inst->previousModel ) );
	inst->prevTransformFrame = inst->transformFrame;
	inst->visibleAge = 0;
}

void vk_gpu_scene_update_instance_transform( uint32_t handle, const float *axis, const vec3_t origin )
{
	vkGpuSceneInstance_t *inst = GPUScene_FindByHandle( handle );
	float ax[3][3];
	int i;

	if ( !inst || !axis || !origin || inst->lifecycleState != VK_GPU_OBJ_LIFE_ACTIVE ) {
		return;
	}
	for ( i = 0; i < 3; i++ ) {
		ax[i][0] = axis[i * 3 + 0];
		ax[i][1] = axis[i * 3 + 1];
		ax[i][2] = axis[i * 3 + 2];
	}
	GPUScene_PackTransform( inst->transform, ax, origin );
	GPUScene_SyncModelMats( inst );
	inst->transformFrame = s_frameId;
	s_objectTransformFrame = s_frameId;
}

void vk_gpu_scene_set_prev_transforms( void )
{
	uint32_t i;
	for ( i = 0; i < s_instanceCount; i++ ) {
		if ( s_instances[i].lifecycleState != VK_GPU_OBJ_LIFE_ACTIVE ) {
			continue;
		}
		Com_Memcpy( s_instances[i].prevTransform, s_instances[i].transform,
			sizeof( s_instances[i].prevTransform ) );
		Com_Memcpy( s_instances[i].previousModel, s_instances[i].currentModel,
			sizeof( s_instances[i].previousModel ) );
		s_instances[i].prevTransformFrame = s_instances[i].transformFrame;
	}
}

static qboolean GPUScene_SphereInFrustum( const float sphere[4] )
{
	float normals[4][3];
	float dists[4];
	int i;

	for ( i = 0; i < 4; i++ ) {
		normals[i][0] = backEnd.viewParms.frustum[i].normal[0];
		normals[i][1] = backEnd.viewParms.frustum[i].normal[1];
		normals[i][2] = backEnd.viewParms.frustum[i].normal[2];
		dists[i] = backEnd.viewParms.frustum[i].dist;
	}
	return vk_gpu_frustum_sphere_visible( sphere, normals, dists );
}

void vk_gpu_scene_cull_and_build_indirect( void )
{
	uint32_t i;
	const qboolean doCull = ( !r_gpuSceneCull || r_gpuSceneCull->integer ) ? qtrue : qfalse;
	const qboolean doIndirect = ( r_gpuSceneIndirect && r_gpuSceneIndirect->integer ) ? qtrue : qfalse;
	const qboolean doOcclusion = vk_gpu_occlusion_enabled();
	const uint32_t cmdCap = ( r_gpuDrawMaxCommands && r_gpuDrawMaxCommands->integer > 0 )
		? (uint32_t)Com_Clamp( 1, VK_GPU_SCENE_INDIRECT_MAX, r_gpuDrawMaxCommands->integer )
		: VK_GPU_SCENE_INDIRECT_MAX;

	if ( !vk_gpu_scene_active() ) {
		return;
	}

	s_cullFrames++;
	s_visibilityFrame = s_frameId;
	s_visibleCount = 0;
	s_indirectCount = 0;
	s_drawCountPublished = 0;
	Com_Memset( s_indirect, 0, sizeof( s_indirect ) );
	Com_Memset( s_listCounts, 0, sizeof( s_listCounts ) );
	if ( s_indirectMapped ) {
		Com_Memset( s_indirectMapped, 0, sizeof( vkGpuSceneDrawCmd_t ) * VK_GPU_SCENE_INDIRECT_MAX );
	}
	if ( s_drawCountMapped ) {
		s_drawCountMapped[0] = 0u;
	}

	if ( vk_gpu_scene_world_type() == VK_WORLD_TYPE_CLASSIC_BSP && s_instanceCount == 0 ) {
		return;
	}

	vk_hiz_build();

	for ( i = 0; i < s_instanceCount; i++ ) {
		vkGpuSceneInstance_t *inst = &s_instances[i];
		qboolean wasVis;
		vec3_t wmins, wmaxs;

		if ( inst->lifecycleState != VK_GPU_OBJ_LIFE_ACTIVE || inst->handle == 0u ) {
			continue;
		}
		wasVis = ( inst->visibleAge > 0 ) ? qtrue : qfalse;

		if ( inst->generation != s_generation ) {
			inst->lastReject = VK_GPU_SCENE_REJECT_INVALID_GEN;
			s_staleCommandRejects++;
			vk_gpu_visibility_note_reject( VISIBILITY_FINAL );
			continue;
		}
		if ( inst->streamState == VK_GPU_SCENE_STREAM_EVICTED ) {
			inst->lastReject = VK_GPU_SCENE_REJECT_STREAM;
			s_rejectStream++;
			inst->visibleAge = 0;
			vk_gpu_visibility_note_reject( VISIBILITY_PVS );
			continue;
		}

		vk_gpu_visibility_add_candidate( inst->handle, VISIBILITY_PVS );

		if ( doCull && !GPUScene_SphereInFrustum( inst->sphere ) ) {
			if ( inst->visibleAge > 0 && inst->visibleAge < vk_gpu_occlusion_grace_frames() ) {
				/* grace — keep */
			} else {
				inst->lastReject = VK_GPU_SCENE_REJECT_FRUSTUM;
				s_rejectFrustum++;
				inst->visibleAge = 0;
				vk_gpu_visibility_note_reject( VISIBILITY_FRUSTUM );
				continue;
			}
		}

		wmins[0] = inst->sphere[0] - inst->sphere[3];
		wmins[1] = inst->sphere[1] - inst->sphere[3];
		wmins[2] = inst->sphere[2] - inst->sphere[3];
		wmaxs[0] = inst->sphere[0] + inst->sphere[3];
		wmaxs[1] = inst->sphere[1] + inst->sphere[3];
		wmaxs[2] = inst->sphere[2] + inst->sphere[3];

		if ( doCull && doOcclusion &&
			!vk_hiz_aabb_visible( wmins, wmaxs, wasVis, inst->visibleAge ) ) {
			inst->lastReject = VK_GPU_SCENE_REJECT_HIZ;
			s_rejectHiz++;
			inst->visibleAge = 0;
			vk_gpu_visibility_note_reject( VISIBILITY_HIZ );
			continue;
		}

		if ( r_gpuSceneHlod && r_gpuSceneHlod->integer && ( inst->flags & 2u ) ) {
			vec3_t center;
			float dist;
			center[0] = inst->sphere[0];
			center[1] = inst->sphere[1];
			center[2] = inst->sphere[2];
			dist = Distance( backEnd.viewParms.or.origin, center );
			if ( dist < 2048.0f && ( inst->flags & 1u ) == 0 ) {
				inst->lastReject = VK_GPU_SCENE_REJECT_LOD;
				s_rejectLod++;
				vk_gpu_visibility_note_reject( VISIBILITY_LOD );
				continue;
			}
		}

		inst->lastReject = VK_GPU_SCENE_REJECT_NONE;
		inst->visibleAge++;
		inst->lastVisibleFrame = s_frameId;
		if ( s_visibleCount < VK_GPU_SCENE_MAX_INSTANCES ) {
			s_visible[s_visibleCount++] = inst->handle;
		}

		if ( doIndirect && s_indirectCount < cmdCap ) {
			vkGpuSceneDrawCmd_t *cmd = &s_indirect[s_indirectCount];
			const vkGpuSceneMesh_t *mesh = NULL;
			uint32_t mi;
			uint32_t listIdx = VK_GPU_DRAW_LIST_FORWARD_OPAQUE;

			for ( mi = 0; mi < s_meshCount; mi++ ) {
				if ( s_meshes[mi].meshId == inst->meshId ) {
					mesh = &s_meshes[mi];
					break;
				}
			}
			if ( !mesh || mesh->generation != s_generation ) {
				inst->lastReject = VK_GPU_SCENE_REJECT_BAD_MESH;
				s_invalidCommandRejects++;
				continue;
			}
			cmd->indexCount = mesh->indexCount ? mesh->indexCount : inst->indexCount;
			cmd->instanceCount = 1;
			cmd->firstIndex = mesh->indexCount ? mesh->indexFirst : inst->firstIndex;
			cmd->vertexOffset = inst->vertexOffset;
			cmd->firstInstance = inst->handle;

			/* Fault injection — reject rather than execute bad commands. */
			if ( r_gpuDrawForceBadFirstIndex && r_gpuDrawForceBadFirstIndex->integer ) {
				cmd->firstIndex = 0x7fffffffu;
			}
			if ( r_gpuDrawForceBadVertexOffset && r_gpuDrawForceBadVertexOffset->integer ) {
				cmd->vertexOffset = 0x7fffffff;
			}
			if ( r_gpuDrawForceBadInstance && r_gpuDrawForceBadInstance->integer ) {
				cmd->instanceCount = 0u;
			}
			if ( r_gpuDrawForceStaleCommand && r_gpuDrawForceStaleCommand->integer ) {
				inst->lastReject = VK_GPU_SCENE_REJECT_FAULT_INJECT;
				s_staleCommandRejects++;
				ri.Printf( PRINT_ALL, "[VK][GPUScene] fault: stale command rejected handle=%u\n",
					inst->handle );
				continue;
			}
			if ( cmd->indexCount == 0u || cmd->instanceCount == 0u ) {
				s_invalidCommandRejects++;
				continue;
			}
			if ( r_gpuDrawForceCountOverflow && r_gpuDrawForceCountOverflow->integer &&
				s_indirectCount + 1u >= cmdCap ) {
				s_fallbackObjects++;
				ri.Printf( PRINT_ALL, "[VK][GPUScene] fault: count overflow — direct fallback\n" );
				continue;
			}

			switch ( inst->renderPath ) {
			case VK_GPU_PATH_DEFERRED: listIdx = VK_GPU_DRAW_LIST_DEFERRED_OPAQUE; break;
			case VK_GPU_PATH_ALPHA_TEST: listIdx = VK_GPU_DRAW_LIST_ALPHA_TEST; break;
			case VK_GPU_PATH_TRANSPARENT: listIdx = VK_GPU_DRAW_LIST_TRANSPARENT; break;
			case VK_GPU_PATH_SHADOW: listIdx = VK_GPU_DRAW_LIST_SHADOW; break;
			case VK_GPU_PATH_WEAPON: listIdx = VK_GPU_DRAW_LIST_WEAPON; break;
			case VK_GPU_PATH_DEPTH_PREPASS: listIdx = VK_GPU_DRAW_LIST_DEPTH_PREPASS; break;
			case VK_GPU_PATH_VELOCITY: listIdx = VK_GPU_DRAW_LIST_VELOCITY; break;
			case VK_GPU_PATH_OBJECT_ID: listIdx = VK_GPU_DRAW_LIST_DEBUG; break;
			default: listIdx = VK_GPU_DRAW_LIST_FORWARD_OPAQUE; break;
			}
			s_listCounts[listIdx]++;
			inst->lastSubmittedFrame = s_frameId;
			s_indirectCount++;
		} else if ( doIndirect ) {
			s_fallbackObjects++;
		}
	}

	s_indirectCommandFrame = s_frameId;
	s_drawCountPublished = s_indirectCount;
	if ( s_drawCountMapped ) {
		s_drawCountMapped[0] = s_drawCountPublished;
	}
	if ( doIndirect && s_indirectMapped && s_indirectCount > 0 ) {
		Com_Memcpy( s_indirectMapped, s_indirect, sizeof( vkGpuSceneDrawCmd_t ) * s_indirectCount );
	}
	/* Always refresh object SSBO so shared consumers see current transforms/paths. */
	GPUScene_UploadObjects();

	if ( r_gpuDrawCompare && r_gpuDrawCompare->integer ) {
		uint32_t mismatches = 0u;
		uint32_t n = s_visibleCount < s_indirectCount ? s_visibleCount : s_indirectCount;
		uint32_t j;
		uint32_t invalidCmds = 0u;

		for ( j = 0; j < n; j++ ) {
			if ( s_indirect[j].firstInstance != s_visible[j] ) {
				mismatches++;
			}
			if ( s_indirect[j].indexCount == 0u && s_indirect[j].instanceCount != 0u ) {
				invalidCmds++;
			}
		}
		if ( s_visibleCount != s_indirectCount ) {
			mismatches += ( s_visibleCount > s_indirectCount )
				? ( s_visibleCount - s_indirectCount )
				: ( s_indirectCount - s_visibleCount );
		}
		ri.Printf( PRINT_ALL,
			"[VK][gpuDrawCompare] mode=%d visible=%u indirect=%u published=%u "
			"idMismatches=%u invalidCmds=%u staleRej=%u frustumRej=%u hizRej=%u\n",
			r_gpuDrawCompare->integer, s_visibleCount, s_indirectCount, s_drawCountPublished,
			mismatches, invalidCmds, s_staleCommandRejects, s_rejectFrustum, s_rejectHiz );
	}
}

uint32_t vk_gpu_scene_visible_count( void )
{
	return s_visibleCount;
}

uint32_t vk_gpu_scene_indirect_count( void )
{
	return s_indirectCount;
}

const uint32_t *vk_gpu_scene_visible_handles( void )
{
	return s_visible;
}

const vkGpuSceneDrawCmd_t *vk_gpu_scene_indirect_cmds( void )
{
	return s_indirect;
}

qboolean vk_gpu_scene_indirect_buffer_ready( void )
{
	return ( s_indirectMapped != NULL && s_indirectBuf != VK_NULL_HANDLE ) ? qtrue : qfalse;
}

void *vk_gpu_scene_indirect_buffer_handle( void )
{
	return (void *)s_indirectBuf;
}

qboolean vk_gpu_scene_object_buffer_ready( void )
{
	return ( s_objectMapped != NULL && s_objectBuf != VK_NULL_HANDLE ) ? qtrue : qfalse;
}

void *vk_gpu_scene_object_buffer_handle( void )
{
	return (void *)s_objectBuf;
}

uint32_t vk_gpu_scene_draw_count_published( void )
{
	return s_drawCountPublished;
}

const gpuSceneObject_t *vk_gpu_scene_object_by_handle( uint32_t handle )
{
	return GPUScene_FindByHandle( handle );
}

uint32_t vk_gpu_scene_pilot_register_bsp_surface( uint32_t surfaceIndex, uint32_t materialId,
	const vec3_t mins, const vec3_t maxs, uint32_t indexFirst, uint32_t indexCount,
	uint32_t renderPath )
{
	float axis[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
	vec3_t origin = { 0, 0, 0 };
	uint32_t meshId;
	uint32_t handle;

	if ( !r_gpuSceneBspPilot || !r_gpuSceneBspPilot->integer || !vk_gpu_scene_active() ) {
		return 0;
	}
	meshId = vk_gpu_scene_register_mesh_ex( materialId, mins, maxs, 1u /*static*/,
		indexFirst, indexCount, 0 );
	if ( !meshId ) {
		return 0;
	}
	handle = vk_gpu_scene_register_instance_ex( meshId, materialId, surfaceIndex + 1u,
		axis, origin, mins, maxs, 1u, VK_GPU_SRC_BSP_SURFACE, surfaceIndex, renderPath );
	if ( handle ) {
		s_bspPilotCount++;
	}
	return handle;
}

uint32_t vk_gpu_scene_pilot_register_rigid( uint32_t entityNum, uint32_t materialId,
	const float *axis, const vec3_t origin, const vec3_t mins, const vec3_t maxs,
	uint32_t indexFirst, uint32_t indexCount )
{
	uint32_t meshId;
	uint32_t handle;

	if ( !r_gpuSceneDynamicPilot || !r_gpuSceneDynamicPilot->integer || !vk_gpu_scene_active() ) {
		return 0;
	}
	meshId = vk_gpu_scene_register_mesh_ex( materialId, mins, maxs, 8u /*dynamic*/,
		indexFirst, indexCount, 0 );
	if ( !meshId ) {
		return 0;
	}
	handle = vk_gpu_scene_register_instance_ex( meshId, materialId, entityNum + 1u,
		axis, origin, mins, maxs, 8u, VK_GPU_SRC_ENTITY, entityNum, VK_GPU_PATH_FORWARD_FALLBACK );
	if ( handle ) {
		s_dynamicPilotCount++;
	}
	return handle;
}

void vk_gpu_scene_status_f( void )
{
	static const char *worldNames[] = { "classic_bsp", "terrain", "streamed", "hybrid" };
	vkWorldType_t req = vk_gpu_scene_world_type_requested();
	vkWorldType_t wt = vk_gpu_scene_world_type();

	ri.Printf( PRINT_ALL, "======== GPU Scene (Visibility M1) ========\n" );
	ri.Printf( PRINT_ALL, "active       : %s driven=%d\n",
		vk_gpu_scene_active() ? "yes" : "no",
		r_gpuDriven ? r_gpuDriven->integer : 0 );
	ri.Printf( PRINT_ALL, "world_req    : %s (%d)\n", worldNames[req], (int)req );
	ri.Printf( PRINT_ALL, "world_eff    : %s (%d)\n", worldNames[wt], (int)wt );
	ri.Printf( PRINT_ALL, "fallback     : %s\n", vk_gpu_scene_world_fallback_reason() );
	ri.Printf( PRINT_ALL, "generation   : %u frame=%u\n", s_generation, s_frameId );
	ri.Printf( PRINT_ALL, "meshes       : %u / %u\n", s_meshCount, VK_GPU_SCENE_MAX_MESHES );
	ri.Printf( PRINT_ALL, "instances    : %u / %u (freeSlots=%u)\n",
		s_instanceCount, VK_GPU_SCENE_MAX_INSTANCES, s_freeCount );
	ri.Printf( PRINT_ALL, "visible      : %u\n", s_visibleCount );
	ri.Printf( PRINT_ALL, "indirect     : %u published=%u (mapped=%s)\n",
		s_indirectCount, s_drawCountPublished, s_indirectMapped ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "rejects      : frustum=%u hiz=%u lod=%u stream=%u stale=%u invalid=%u\n",
		s_rejectFrustum, s_rejectHiz, s_rejectLod, s_rejectStream,
		s_staleCommandRejects, s_invalidCommandRejects );
	ri.Printf( PRINT_ALL, "pilots       : bsp=%u dynamic=%u fallbackObj=%u\n",
		s_bspPilotCount, s_dynamicPilotCount, s_fallbackObjects );
	ri.Printf( PRINT_ALL, "lists        : def=%u fwd=%u atest=%u shadow=%u xluc=%u weapon=%u depth=%u vel=%u dbg=%u\n",
		s_listCounts[VK_GPU_DRAW_LIST_DEFERRED_OPAQUE],
		s_listCounts[VK_GPU_DRAW_LIST_FORWARD_OPAQUE],
		s_listCounts[VK_GPU_DRAW_LIST_ALPHA_TEST],
		s_listCounts[VK_GPU_DRAW_LIST_SHADOW],
		s_listCounts[VK_GPU_DRAW_LIST_TRANSPARENT],
		s_listCounts[VK_GPU_DRAW_LIST_WEAPON],
		s_listCounts[VK_GPU_DRAW_LIST_DEPTH_PREPASS],
		s_listCounts[VK_GPU_DRAW_LIST_VELOCITY],
		s_listCounts[VK_GPU_DRAW_LIST_DEBUG] );
	ri.Printf( PRINT_ALL, "classic_bsp  : default; pilots opt-in only\n" );
	ri.Printf( PRINT_ALL, "============================================\n" );
	vk_hiz_status_f();
}

void vk_gpu_scene_layout_f( void )
{
	ri.Printf( PRINT_ALL, "======== GPU Scene Layout (M1 Phase 1) ========\n" );
	ri.Printf( PRINT_ALL, "gpuSceneObject_t sizeof=%zu align16=%s\n",
		sizeof( gpuSceneObject_t ),
		( sizeof( gpuSceneObject_t ) % 16u ) == 0u ? "yes" : "NO" );
	ri.Printf( PRINT_ALL, "  contract: current/previousModel, bounds*, objectId/Gen, mesh/material,\n"
		"           surface/path/flags/temporal, probes, pipeline/shadow/vis/instanceData\n" );
	ri.Printf( PRINT_ALL, "vkGpuSceneMesh_t sizeof=%zu\n", sizeof( vkGpuSceneMesh_t ) );
	ri.Printf( PRINT_ALL, "vkGpuSceneDrawCmd_t sizeof=%zu\n", sizeof( vkGpuSceneDrawCmd_t ) );
	ri.Printf( PRINT_ALL, "capacities: instances=%u meshes=%u indirect=%u\n",
		VK_GPU_SCENE_MAX_INSTANCES, VK_GPU_SCENE_MAX_MESHES, VK_GPU_SCENE_INDIRECT_MAX );
	ri.Printf( PRINT_ALL, "memory≈ %zu KiB objects + %zu KiB commands\n",
		( sizeof( gpuSceneObject_t ) * VK_GPU_SCENE_MAX_INSTANCES ) / 1024u,
		( sizeof( vkGpuSceneDrawCmd_t ) * VK_GPU_SCENE_INDIRECT_MAX ) / 1024u );
	ri.Printf( PRINT_ALL, "objectSSBO=%s uploadFrame=%u  indirectBuf=%s\n",
		vk_gpu_scene_object_buffer_ready() ? "yes" : "no", s_objectUploadFrame,
		vk_gpu_scene_indirect_buffer_ready() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "validate_layout=%s\n",
		vk_gpu_scene_validate_layout() ? "PASS" : "FAIL" );
	ri.Printf( PRINT_ALL, "==============================================\n" );
}

void vk_gpu_scene_object_status_f( void )
{
	uint32_t handle;
	const gpuSceneObject_t *o;

	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "usage: gpu_scene_object_status <object-handle>\n" );
		return;
	}
	handle = (uint32_t)atoi( ri.Cmd_Argv( 1 ) );
	o = vk_gpu_scene_object_by_handle( handle );
	if ( !o ) {
		ri.Printf( PRINT_ALL, "object %u not found (or inactive)\n", handle );
		return;
	}
	ri.Printf( PRINT_ALL, "======== GPU Object %u ========\n", handle );
	ri.Printf( PRINT_ALL, "objectId=%u objectGen=%u sceneGen=%u\n",
		o->objectId, o->objectGeneration, o->generation );
	ri.Printf( PRINT_ALL, "source=%u ref=%u mesh=%u material=%u path=%u\n",
		o->sourceKind, o->sourceRef, o->meshId, o->materialId, o->renderPath );
	ri.Printf( PRINT_ALL, "life=%u invalidate=%u reject=%u visibleAge=%u\n",
		o->lifecycleState, o->lastInvalidationReason, o->lastReject, o->visibleAge );
	ri.Printf( PRINT_ALL, "xfFrame=%u prevXfFrame=%u lastVis=%u lastSubmit=%u\n",
		o->transformFrame, o->prevTransformFrame, o->lastVisibleFrame, o->lastSubmittedFrame );
	ri.Printf( PRINT_ALL, "sphere=(%.1f %.1f %.1f) r=%.1f idx=%u count=%u\n",
		o->sphere[0], o->sphere[1], o->sphere[2], o->sphere[3],
		o->firstIndex, o->indexCount );
	ri.Printf( PRINT_ALL, "==============================\n" );
}

void vk_gpu_draw_status_f( void )
{
	ri.Printf( PRINT_ALL, "======== GPU Draw Status ========\n" );
	ri.Printf( PRINT_ALL, "commands=%u capacity=%u published=%u gen=%u\n",
		s_indirectCount, VK_GPU_SCENE_INDIRECT_MAX, s_drawCountPublished, s_generation );
	ri.Printf( PRINT_ALL, "staleRejects=%u invalidRejects=%u fallbackObj=%u\n",
		s_staleCommandRejects, s_invalidCommandRejects, s_fallbackObjects );
	ri.Printf( PRINT_ALL, "indirectBuf=%s drawCountBuf=%s objectSSBO=%s\n",
		s_indirectBuf != VK_NULL_HANDLE ? "yes" : "no",
		s_drawCountBuf != VK_NULL_HANDLE ? "yes" : "no",
		s_objectBuf != VK_NULL_HANDLE ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "=================================\n" );
}

void vk_gpu_frame_ownership_status_f( void )
{
	ri.Printf( PRINT_ALL, "======== GPU Frame Ownership ========\n" );
	ri.Printf( PRINT_ALL, "frameId=%u sceneUpdate=%u transform=%u visibility=%u indirect=%u objectSSBO=%u\n",
		s_frameId, s_sceneUpdateFrame, s_objectTransformFrame,
		s_visibilityFrame, s_indirectCommandFrame, s_objectUploadFrame );
	ri.Printf( PRINT_ALL, "prev transforms advance on end_frame (rendered frames only)\n" );
	ri.Printf( PRINT_ALL, "=====================================\n" );
}

void vk_gpu_draw_perf_f( void )
{
	ri.Printf( PRINT_ALL, "======== GPU Draw Perf ========\n" );
	ri.Printf( PRINT_ALL, "cullFrames=%u visible=%u cmds=%u frustumRej=%u hizRej=%u\n",
		s_cullFrames, s_visibleCount, s_indirectCount, s_rejectFrustum, s_rejectHiz );
	ri.Printf( PRINT_ALL, "===============================\n" );
}

uint32_t vk_gpu_scene_generation( void ) { return s_generation; }
uint32_t vk_gpu_scene_frame_id( void ) { return s_frameId; }
qboolean vk_gpu_scene_driven_active( void ) {
	return ( vk_gpu_scene_active() && r_gpuDriven && r_gpuDriven->integer ) ? qtrue : qfalse;
}
void vk_gpu_scene_telemetry( uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d ) {
	if ( a ) *a = s_instanceCount;
	if ( b ) *b = s_rejectFrustum;
	if ( c ) *c = s_rejectHiz;
	if ( d ) *d = s_indirectCount;
}
qboolean vk_gpu_scene_validate_layout( void ) {
	return ( sizeof( gpuSceneObject_t ) % 16u == 0u &&
		sizeof( vkGpuSceneDrawCmd_t ) >= 20u &&
		offsetof( gpuSceneObject_t, currentModel ) == 0u &&
		offsetof( gpuSceneObject_t, objectId ) > offsetof( gpuSceneObject_t, boundsMax ) ) ? qtrue : qfalse;
}

void vk_gpu_scene_merge_compatible_draws( uint32_t *outCmdsIn, uint32_t *outCmdsOut,
	uint32_t *outGroups, uint32_t *outSkipped )
{
	uint32_t i;
	uint32_t out = 0;
	uint32_t groups = 0;
	uint32_t skipped = 0;
	vkGpuSceneDrawCmd_t compacted[VK_GPU_SCENE_INDIRECT_MAX];
	const uint32_t inCount = s_indirectCount;

	if ( outCmdsIn ) {
		*outCmdsIn = inCount;
	}
	if ( inCount == 0u || !vk_gpu_scene_active() ) {
		if ( outCmdsOut ) {
			*outCmdsOut = 0u;
		}
		if ( outGroups ) {
			*outGroups = 0u;
		}
		if ( outSkipped ) {
			*outSkipped = 0u;
		}
		return;
	}

	compacted[0] = s_indirect[0];
	out = 1u;
	groups = 1u;

	for ( i = 1u; i < inCount; i++ ) {
		const vkGpuSceneDrawCmd_t *prev = &compacted[out - 1u];
		const vkGpuSceneDrawCmd_t *cur = &s_indirect[i];
		qboolean compatible;

		/* Same index range + vertex offset → same mesh draw; instanceCount accumulates. */
		compatible = ( prev->indexCount == cur->indexCount &&
			prev->firstIndex == cur->firstIndex &&
			prev->vertexOffset == cur->vertexOffset &&
			prev->indexCount > 0u ) ? qtrue : qfalse;

		if ( compatible ) {
			compacted[out - 1u].instanceCount += cur->instanceCount;
			/* Keep firstInstance as base handle for the group. */
			groups++;
		} else {
			if ( cur->indexCount == 0u ) {
				skipped++;
				continue;
			}
			if ( out < VK_GPU_SCENE_INDIRECT_MAX ) {
				compacted[out++] = *cur;
				groups++;
			} else {
				skipped++;
			}
		}
	}

	s_indirectCount = out;
	Com_Memcpy( s_indirect, compacted, sizeof( vkGpuSceneDrawCmd_t ) * out );
	if ( s_indirectMapped && out > 0u ) {
		Com_Memcpy( s_indirectMapped, s_indirect, sizeof( vkGpuSceneDrawCmd_t ) * out );
	}

	if ( outCmdsOut ) {
		*outCmdsOut = out;
	}
	if ( outGroups ) {
		*outGroups = groups;
	}
	if ( outSkipped ) {
		*outSkipped = skipped;
	}
}

