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
#include "vk_hiz.h"
#include "vk_meshlets.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include "vk_pass_registry.h"
#include "vk_raster_ultra.h"
#include "vk_terrain.h"

#include <math.h>

static cvar_t *r_gpuScene;
static cvar_t *r_gpuSceneCull;
static cvar_t *r_gpuSceneIndirect;
static cvar_t *r_gpuSceneDebug;
static cvar_t *r_gpuSceneWorldType;
static cvar_t *r_gpuSceneHlod;
static cvar_t *r_gpuSceneLodHysteresis;

static char s_worldFallbackReason[96];

static vkGpuSceneInstance_t s_instances[VK_GPU_SCENE_MAX_INSTANCES];
static vkGpuSceneMesh_t     s_meshes[VK_GPU_SCENE_MAX_MESHES];
static uint32_t s_instanceCount;
static uint32_t s_meshCount;
static uint32_t s_nextHandle = 1;
static uint32_t s_generation = 1;

static uint32_t s_visible[VK_GPU_SCENE_MAX_INSTANCES];
static uint32_t s_visibleCount;
static vkGpuSceneDrawCmd_t s_indirect[VK_GPU_SCENE_INDIRECT_MAX];
static uint32_t s_indirectCount;

static uint32_t s_rejectFrustum;
static uint32_t s_rejectHiz;
static uint32_t s_rejectLod;
static uint32_t s_rejectStream;
static uint32_t s_cullFrames;
static qboolean s_cmds;
static qboolean s_logged;

/* Host-visible indirect buffer (optional GPU draw later). */
static VkBuffer s_indirectBuf;
static VkDeviceMemory s_indirectMem;
static void *s_indirectMapped;

static void GPUScene_PackTransform( float out12[12], const float axis[3][3], const vec3_t origin )
{
	out12[0] = axis[0][0]; out12[1] = axis[0][1]; out12[2] = axis[0][2]; out12[3] = origin[0];
	out12[4] = axis[1][0]; out12[5] = axis[1][1]; out12[6] = axis[1][2]; out12[7] = origin[1];
	out12[8] = axis[2][0]; out12[9] = axis[2][1]; out12[10] = axis[2][2]; out12[11] = origin[2];
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
		if ( s_instances[i].handle == handle ) {
			return &s_instances[i];
		}
	}
	return NULL;
}

static void GPUScene_DestroyIndirectBuf( void )
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
}

static qboolean GPUScene_EnsureIndirectBuf( void )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo mai;
	const VkDeviceSize size = sizeof( vkGpuSceneDrawCmd_t ) * VK_GPU_SCENE_INDIRECT_MAX;

	if ( s_indirectMapped ) {
		return qtrue;
	}
	if ( vk.device == VK_NULL_HANDLE ) {
		return qfalse;
	}

	Com_Memset( &bci, 0, sizeof( bci ) );
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = size;
	bci.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if ( qvkCreateBuffer( vk.device, &bci, NULL, &s_indirectBuf ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetBufferMemoryRequirements( vk.device, s_indirectBuf, &req );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	if ( qvkAllocateMemory( vk.device, &mai, NULL, &s_indirectMem ) != VK_SUCCESS ) {
		GPUScene_DestroyIndirectBuf();
		return qfalse;
	}
	qvkBindBufferMemory( vk.device, s_indirectBuf, s_indirectMem, 0 );
	if ( qvkMapMemory( vk.device, s_indirectMem, 0, size, 0, &s_indirectMapped ) != VK_SUCCESS ) {
		GPUScene_DestroyIndirectBuf();
		return qfalse;
	}
	return qtrue;
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
	ri.Cvar_CheckRange( r_gpuSceneDebug, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_gpuSceneDebug,
		"GPU scene debug:\n"
		" 0 off\n"
		" 1 bounds\n"
		" 2 rejection reasons\n"
		" 3 LOD/HLOD\n"
		" 4 indirect counts" );
}

void vk_gpu_scene_init( void )
{
	vk_gpu_scene_register_cvars();
	vk_hiz_init();

	s_instanceCount = 0;
	s_meshCount = 0;
	s_visibleCount = 0;
	s_indirectCount = 0;
	s_nextHandle = 1;
	s_generation = 1;
	s_rejectFrustum = s_rejectHiz = s_rejectLod = s_rejectStream = 0;
	s_cullFrames = 0;

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "gpu_scene_status", vk_gpu_scene_status_f );
		s_cmds = qtrue;
	}

	if ( r_gpuScene && r_gpuScene->integer ) {
		GPUScene_EnsureIndirectBuf();
		if ( !s_logged ) {
			ri.Printf( PRINT_ALL,
				"[VK][GPUScene] Raster Ultra 1.6 active: worldType=%d "
				"(classic BSP preserved when type=0 / no stream metadata)\n",
				r_gpuSceneWorldType ? r_gpuSceneWorldType->integer : 0 );
			s_logged = qtrue;
		}
	}
}

void vk_gpu_scene_shutdown( void )
{
	GPUScene_DestroyIndirectBuf();
	vk_hiz_shutdown();
	s_instanceCount = 0;
	s_meshCount = 0;
	s_visibleCount = 0;
	s_indirectCount = 0;
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
	s_visibleCount = 0;
	s_indirectCount = 0;
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
}

void vk_gpu_scene_begin_frame( void )
{
	if ( !vk_gpu_scene_active() ) {
		return;
	}
	s_visibleCount = 0;
	s_indirectCount = 0;
	s_rejectFrustum = s_rejectHiz = s_rejectLod = s_rejectStream = 0;
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
	VectorCopy( mins, m->mins );
	VectorCopy( maxs, m->maxs );
	s_meshCount++;
	return m->meshId;
}

uint32_t vk_gpu_scene_register_instance( uint32_t meshId, uint32_t materialId, uint32_t objectId,
	const float *axis, const vec3_t origin, const vec3_t mins, const vec3_t maxs,
	uint32_t flags )
{
	vkGpuSceneInstance_t *inst;
	float ax[3][3];
	int i;

	if ( !vk_gpu_scene_active() || s_instanceCount >= VK_GPU_SCENE_MAX_INSTANCES ) {
		return 0;
	}
	if ( !axis || !origin || !mins || !maxs ) {
		return 0;
	}

	for ( i = 0; i < 3; i++ ) {
		ax[i][0] = axis[i * 3 + 0];
		ax[i][1] = axis[i * 3 + 1];
		ax[i][2] = axis[i * 3 + 2];
	}

	inst = &s_instances[s_instanceCount];
	Com_Memset( inst, 0, sizeof( *inst ) );
	inst->handle = s_nextHandle++;
	if ( s_nextHandle == 0 ) {
		s_nextHandle = 1;
	}
	inst->meshId = meshId;
	inst->materialId = materialId;
	inst->objectId = objectId ? objectId : inst->handle;
	GPUScene_PackTransform( inst->transform, ax, origin );
	Com_Memcpy( inst->prevTransform, inst->transform, sizeof( inst->prevTransform ) );
	VectorCopy( mins, inst->mins );
	VectorCopy( maxs, inst->maxs );
	GPUScene_SphereFromBounds( inst->sphere, mins, maxs );
	inst->flags = flags;
	inst->streamState = VK_GPU_SCENE_STREAM_RESIDENT;
	inst->generation = s_generation;
	inst->visibleAge = 0;
	s_instanceCount++;
	return inst->handle;
}

void vk_gpu_scene_update_instance_transform( uint32_t handle, const float *axis, const vec3_t origin )
{
	vkGpuSceneInstance_t *inst = GPUScene_FindByHandle( handle );
	float ax[3][3];
	int i;

	if ( !inst || !axis || !origin ) {
		return;
	}
	for ( i = 0; i < 3; i++ ) {
		ax[i][0] = axis[i * 3 + 0];
		ax[i][1] = axis[i * 3 + 1];
		ax[i][2] = axis[i * 3 + 2];
	}
	GPUScene_PackTransform( inst->transform, ax, origin );
}

void vk_gpu_scene_set_prev_transforms( void )
{
	uint32_t i;
	for ( i = 0; i < s_instanceCount; i++ ) {
		Com_Memcpy( s_instances[i].prevTransform, s_instances[i].transform,
			sizeof( s_instances[i].prevTransform ) );
	}
}

static qboolean GPUScene_SphereInFrustum( const float sphere[4] )
{
	cplane_t *frust;
	int i;
	vec3_t c;

	c[0] = sphere[0];
	c[1] = sphere[1];
	c[2] = sphere[2];
	for ( i = 0; i < 4; i++ ) {
		frust = &backEnd.viewParms.frustum[i];
		if ( DotProduct( c, frust->normal ) - frust->dist < -sphere[3] ) {
			return qfalse;
		}
	}
	return qtrue;
}

void vk_gpu_scene_cull_and_build_indirect( void )
{
	uint32_t i;
	const qboolean doCull = ( !r_gpuSceneCull || r_gpuSceneCull->integer ) ? qtrue : qfalse;
	const qboolean doIndirect = ( r_gpuSceneIndirect && r_gpuSceneIndirect->integer ) ? qtrue : qfalse;

	if ( !vk_gpu_scene_active() ) {
		return;
	}

	s_cullFrames++;
	s_visibleCount = 0;
	s_indirectCount = 0;

	/* Classic / hybrid: never claim BSP surfaces via this path. */
	if ( vk_gpu_scene_world_type() == VK_WORLD_TYPE_CLASSIC_BSP && s_instanceCount == 0 ) {
		return;
	}

	vk_hiz_build();

	for ( i = 0; i < s_instanceCount; i++ ) {
		vkGpuSceneInstance_t *inst = &s_instances[i];
		qboolean wasVis = ( inst->visibleAge > 0 ) ? qtrue : qfalse;
		vec3_t wmins, wmaxs;

		if ( inst->generation != s_generation ) {
			inst->lastReject = VK_GPU_SCENE_REJECT_OVERFLOW;
			continue;
		}
		if ( inst->streamState == VK_GPU_SCENE_STREAM_EVICTED ) {
			inst->lastReject = VK_GPU_SCENE_REJECT_STREAM;
			s_rejectStream++;
			inst->visibleAge = 0;
			continue;
		}

		if ( doCull && !GPUScene_SphereInFrustum( inst->sphere ) ) {
			/* Min visibility duration — keep briefly after leaving frustum edge. */
			if ( inst->visibleAge > 0 && inst->visibleAge < 2 ) {
				/* allow one more frame */
			} else {
				inst->lastReject = VK_GPU_SCENE_REJECT_FRUSTUM;
				s_rejectFrustum++;
				inst->visibleAge = 0;
				continue;
			}
		}

		VectorCopy( inst->mins, wmins );
		VectorCopy( inst->maxs, wmaxs );
		/* Transform local bounds roughly by translation (sphere already world). */
		wmins[0] = inst->sphere[0] - inst->sphere[3];
		wmins[1] = inst->sphere[1] - inst->sphere[3];
		wmins[2] = inst->sphere[2] - inst->sphere[3];
		wmaxs[0] = inst->sphere[0] + inst->sphere[3];
		wmaxs[1] = inst->sphere[1] + inst->sphere[3];
		wmaxs[2] = inst->sphere[2] + inst->sphere[3];

		if ( doCull && !vk_hiz_aabb_visible( wmins, wmaxs, wasVis, inst->visibleAge ) ) {
			inst->lastReject = VK_GPU_SCENE_REJECT_HIZ;
			s_rejectHiz++;
			inst->visibleAge = 0;
			continue;
		}

		/* HLOD: distant static aggregates may drop fine instances when flag set. */
		if ( r_gpuSceneHlod && r_gpuSceneHlod->integer && ( inst->flags & 2u ) ) {
			vec3_t center;
			float dist;
			center[0] = inst->sphere[0];
			center[1] = inst->sphere[1];
			center[2] = inst->sphere[2];
			dist = Distance( backEnd.viewParms.or.origin, center );
			if ( dist < 2048.0f && ( inst->flags & 1u ) == 0 ) {
				/* Prefer fine geo near camera; skip HLOD proxy when close. */
				inst->lastReject = VK_GPU_SCENE_REJECT_LOD;
				s_rejectLod++;
				continue;
			}
		}

		inst->lastReject = VK_GPU_SCENE_REJECT_NONE;
		inst->visibleAge++;
		if ( s_visibleCount < VK_GPU_SCENE_MAX_INSTANCES ) {
			s_visible[s_visibleCount++] = inst->handle;
		}

		if ( doIndirect && s_indirectCount < VK_GPU_SCENE_INDIRECT_MAX ) {
			vkGpuSceneDrawCmd_t *cmd = &s_indirect[s_indirectCount++];
			const vkGpuSceneMesh_t *mesh = NULL;
			uint32_t mi;

			for ( mi = 0; mi < s_meshCount; mi++ ) {
				if ( s_meshes[mi].meshId == inst->meshId ) {
					mesh = &s_meshes[mi];
					break;
				}
			}
			cmd->indexCount = mesh ? mesh->indexCount : 0;
			cmd->instanceCount = 1;
			cmd->firstIndex = mesh ? mesh->indexFirst : 0;
			cmd->vertexOffset = 0;
			cmd->firstInstance = inst->handle;
		}
	}

	if ( doIndirect && s_indirectMapped && s_indirectCount > 0 ) {
		Com_Memcpy( s_indirectMapped, s_indirect, sizeof( vkGpuSceneDrawCmd_t ) * s_indirectCount );
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

static VkBuffer GPUScene_IndirectVkBuffer( void )
{
	return s_indirectBuf;
}

qboolean vk_gpu_scene_indirect_buffer_ready( void )
{
	(void)GPUScene_IndirectVkBuffer;
	return ( s_indirectMapped != NULL && s_indirectBuf != VK_NULL_HANDLE ) ? qtrue : qfalse;
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

void vk_gpu_scene_status_f( void )
{
	static const char *worldNames[] = { "classic_bsp", "terrain", "streamed", "hybrid" };
	vkWorldType_t req = vk_gpu_scene_world_type_requested();
	vkWorldType_t wt = vk_gpu_scene_world_type();

	ri.Printf( PRINT_ALL, "======== GPU Scene (Raster Ultra 1.6/1.14) ========\n" );
	ri.Printf( PRINT_ALL, "active       : %s\n", vk_gpu_scene_active() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "world_req    : %s (%d)\n", worldNames[req], (int)req );
	ri.Printf( PRINT_ALL, "world_eff    : %s (%d)\n", worldNames[wt], (int)wt );
	ri.Printf( PRINT_ALL, "fallback     : %s\n", vk_gpu_scene_world_fallback_reason() );
	ri.Printf( PRINT_ALL, "terrain_meta : %s\n", vk_gpu_scene_terrain_metadata_present() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "terrain_res  : %s\n", vk_gpu_scene_terrain_resources_ready() ? "ready" : "idle" );
	ri.Printf( PRINT_ALL, "generation   : %u\n", s_generation );
	ri.Printf( PRINT_ALL, "meshes       : %u / %u\n", s_meshCount, VK_GPU_SCENE_MAX_MESHES );
	ri.Printf( PRINT_ALL, "instances    : %u / %u\n", s_instanceCount, VK_GPU_SCENE_MAX_INSTANCES );
	ri.Printf( PRINT_ALL, "visible      : %u\n", s_visibleCount );
	ri.Printf( PRINT_ALL, "indirect     : %u (mapped=%s)\n",
		s_indirectCount, s_indirectMapped ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "rejects      : frustum=%u hiz=%u lod=%u stream=%u\n",
		s_rejectFrustum, s_rejectHiz, s_rejectLod, s_rejectStream );
	ri.Printf( PRINT_ALL, "cull_frames  : %u\n", s_cullFrames );
	ri.Printf( PRINT_ALL, "meshlets     : companion r_meshlets (stable cache + MDI)\n" );
	ri.Printf( PRINT_ALL, "classic_bsp  : default; terrain never replaces BSP without metadata\n" );
	ri.Printf( PRINT_ALL, "frame_gen    : off | RT: locked under Raster Ultra\n" );
	ri.Printf( PRINT_ALL, "===================================================\n" );
	vk_hiz_status_f();
}
