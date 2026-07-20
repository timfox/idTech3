/*
===========================================================================
Cinematic Engine Platform 1.0 — Unified scene registry (Environment Slice).
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_scene_platform.h"
#include "vk_gpu_scene.h"

static cvar_t *r_scenePlatform;
static cvar_t *r_sceneLiveEdit;
static cvar_t *r_sceneInvalidateDebug;

static vkSceneNode_t s_nodes[VK_SCENE_MAX_NODES];
static vkScenePlatformState_t s_state;
static vkSceneInvalidationEvent_t s_invLog[VK_SCENE_INVALIDATION_LOG];
static uint32_t s_invLogHead;
static uint32_t s_nextIndex = 1;
static uint32_t s_idGeneration = 1;
static uint32_t s_pendingDirty;
static qboolean s_cmds;
static vkSceneId_t s_rootId;
static vkSceneId_t s_bspWorldId;

static void Scene_IdentityAxis( float axis[3][3] )
{
	Com_Memset( axis, 0, sizeof( float ) * 9 );
	axis[0][0] = axis[1][1] = axis[2][2] = 1.0f;
}

static vkSceneId_t Scene_MakeId( vkSceneNodeKind_t kind, uint32_t index )
{
	return ( (uint64_t)( kind & 0xffu ) << 56 ) |
		( (uint64_t)( s_idGeneration & 0xffffu ) << 40 ) |
		( (uint64_t)( index & 0xffffffffu ) );
}

static uint32_t Scene_IndexFromId( vkSceneId_t id )
{
	return (uint32_t)( id & 0xffffffffu );
}

static int Scene_SlotForId( vkSceneId_t id )
{
	uint32_t idx = Scene_IndexFromId( id );
	uint32_t i;

	if ( idx == 0u || idx >= VK_SCENE_MAX_NODES ) {
		return -1;
	}
	/* Dense storage: slot == index for allocated nodes. */
	for ( i = 0; i < s_state.nodeCount; i++ ) {
		if ( s_nodes[i].alive && s_nodes[i].id == id ) {
			return (int)i;
		}
	}
	return -1;
}

void vk_scene_platform_register_cvars( void )
{
	if ( r_scenePlatform ) {
		return;
	}

	r_scenePlatform = ri.Cvar_Get( "r_scenePlatform", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_scenePlatform, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_scenePlatform,
		"Cinematic Engine Platform scene registry (latched).\n"
		"Stable IDs + live-edit invalidation. Does not replace classic BSP." );
	ri.Cvar_SetGroup( r_scenePlatform, CVG_RENDERER );

	r_sceneLiveEdit = ri.Cvar_Get( "r_sceneLiveEdit", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_sceneLiveEdit, "0", "1", CV_INTEGER );

	r_sceneInvalidateDebug = ri.Cvar_Get( "r_sceneInvalidateDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_sceneInvalidateDebug, "0", "1", CV_INTEGER );
}

void vk_scene_platform_init( void )
{
	vk_scene_platform_register_cvars();
	Com_Memset( &s_state, 0, sizeof( s_state ) );
	Com_Memset( s_nodes, 0, sizeof( s_nodes ) );
	Com_Memset( s_invLog, 0, sizeof( s_invLog ) );
	s_invLogHead = 0;
	s_nextIndex = 1;
	s_idGeneration = 1;
	s_pendingDirty = 0;
	s_rootId = 0;
	s_bspWorldId = 0;
	s_state.liveEdit = ( r_sceneLiveEdit && r_sceneLiveEdit->integer ) ? qtrue : qfalse;

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "scene_status", vk_scene_platform_status_f );
		ri.Cmd_AddCommand( "scene_node_status", vk_scene_platform_node_status_f );
		ri.Cmd_AddCommand( "scene_invalidate_debug", vk_scene_platform_invalidate_debug_f );
		ri.Cmd_AddCommand( "scene_set_origin", vk_scene_platform_node_status_f );
		ri.Cmd_AddCommand( "scene_set_visible", vk_scene_platform_node_status_f );
		ri.Cmd_AddCommand( "light_status", vk_scene_platform_light_status_f );
		ri.Cmd_AddCommand( "light_spawn_area", vk_scene_platform_light_status_f );
		ri.Cmd_AddCommand( "light_set_color", vk_scene_platform_light_status_f );
		ri.Cmd_AddCommand( "light_set_radius", vk_scene_platform_light_status_f );
		s_cmds = qtrue;
	}

	if ( r_scenePlatform && r_scenePlatform->integer ) {
		ri.Printf( PRINT_ALL,
			"[VK] Scene Platform 1.0: active (stable IDs, live-edit=%s, BSP ownership preserved)\n",
			s_state.liveEdit ? "on" : "off" );
	}
}

void vk_scene_platform_shutdown( void )
{
	if ( s_cmds ) {
		ri.Cmd_RemoveCommand( "scene_status" );
		ri.Cmd_RemoveCommand( "scene_node_status" );
		ri.Cmd_RemoveCommand( "scene_invalidate_debug" );
		ri.Cmd_RemoveCommand( "scene_set_origin" );
		ri.Cmd_RemoveCommand( "scene_set_visible" );
		ri.Cmd_RemoveCommand( "light_status" );
		ri.Cmd_RemoveCommand( "light_spawn_area" );
		ri.Cmd_RemoveCommand( "light_set_color" );
		ri.Cmd_RemoveCommand( "light_set_radius" );
		s_cmds = qfalse;
	}
	Com_Memset( &s_state, 0, sizeof( s_state ) );
	Com_Memset( s_nodes, 0, sizeof( s_nodes ) );
}

qboolean vk_scene_platform_active( void )
{
	return ( r_scenePlatform && r_scenePlatform->integer ) ? qtrue : qfalse;
}

const vkScenePlatformState_t *vk_scene_platform_state( void )
{
	return &s_state;
}

void vk_scene_platform_begin_frame( void )
{
	if ( !vk_scene_platform_active() ) {
		return;
	}
	s_state.liveEdit = ( r_sceneLiveEdit && r_sceneLiveEdit->integer ) ? qtrue : qfalse;
	vk_scene_platform_ensure_world_nodes();
}

void vk_scene_platform_on_world_load( void )
{
	s_idGeneration++;
	if ( s_idGeneration == 0u ) {
		s_idGeneration = 1u;
	}
	Com_Memset( s_nodes, 0, sizeof( s_nodes ) );
	s_state.nodeCount = 0;
	s_state.aliveCount = 0;
	s_nextIndex = 1;
	s_rootId = 0;
	s_bspWorldId = 0;
	s_state.sceneRevision++;
	s_state.compileRevision++;
	s_pendingDirty = 0;
	if ( vk_scene_platform_active() ) {
		vk_scene_platform_ensure_world_nodes();
		ri.Printf( PRINT_ALL, "[VK][Scene] world load — registry reset (rev=%u compile=%u)\n",
			s_state.sceneRevision, s_state.compileRevision );
	}
}

void vk_scene_platform_on_world_unload( void )
{
	Com_Memset( s_nodes, 0, sizeof( s_nodes ) );
	s_state.nodeCount = 0;
	s_state.aliveCount = 0;
	s_rootId = 0;
	s_bspWorldId = 0;
	s_state.sceneRevision++;
}

void vk_scene_platform_on_vid_restart( void )
{
	s_state.sceneRevision++;
	s_pendingDirty |= VK_SCENE_DIRTY_SHADOW | VK_SCENE_DIRTY_GI | VK_SCENE_DIRTY_PROBE;
}

void vk_scene_platform_ensure_world_nodes( void )
{
	if ( !vk_scene_platform_active() ) {
		return;
	}
	if ( s_rootId == 0 ) {
		s_rootId = vk_scene_platform_create_node( VK_SCENE_KIND_ROOT, "scene_root", 0 );
	}
	if ( s_bspWorldId == 0 ) {
		s_bspWorldId = vk_scene_platform_create_node( VK_SCENE_KIND_BSP_WORLD, "classic_bsp", s_rootId );
	}
}

vkSceneId_t vk_scene_platform_create_node( vkSceneNodeKind_t kind, const char *name,
	vkSceneId_t parent )
{
	vkSceneNode_t *n;
	uint32_t index;

	if ( s_state.nodeCount >= VK_SCENE_MAX_NODES || s_nextIndex >= VK_SCENE_MAX_NODES ) {
		ri.Printf( PRINT_WARNING, "[VK][Scene] node table full (%u)\n", s_state.nodeCount );
		return 0;
	}

	index = s_nextIndex++;
	n = &s_nodes[s_state.nodeCount++];
	Com_Memset( n, 0, sizeof( *n ) );
	n->id = Scene_MakeId( kind, index );
	n->parent = parent;
	n->kind = kind;
	n->alive = qtrue;
	n->visible = qtrue;
	n->lightIndex = ~0u;
	n->gpuHandle = 0;
	n->revision = 1;
	Scene_IdentityAxis( n->localAxis );
	Scene_IdentityAxis( n->worldAxis );
	if ( name && name[0] ) {
		Q_strncpyz( n->name, name, sizeof( n->name ) );
	} else {
		Com_sprintf( n->name, sizeof( n->name ), "%s_%u", vk_scene_platform_kind_name( kind ), index );
	}
	s_state.aliveCount++;
	s_state.sceneRevision++;
	return n->id;
}

qboolean vk_scene_platform_find( vkSceneId_t id, vkSceneNode_t **out )
{
	int slot = Scene_SlotForId( id );
	if ( slot < 0 ) {
		if ( out ) {
			*out = NULL;
		}
		return qfalse;
	}
	if ( out ) {
		*out = &s_nodes[slot];
	}
	return qtrue;
}

const vkSceneNode_t *vk_scene_platform_get( vkSceneId_t id )
{
	vkSceneNode_t *n = NULL;
	if ( !vk_scene_platform_find( id, &n ) ) {
		return NULL;
	}
	return n;
}

void vk_scene_platform_note_invalidation( vkSceneId_t id, uint32_t flags )
{
	vkSceneInvalidationEvent_t *ev;

	if ( !flags ) {
		return;
	}
	s_pendingDirty |= flags;
	s_state.invalidationCount++;
	s_state.lastInvalidationFlags = flags;
	ev = &s_invLog[s_invLogHead % VK_SCENE_INVALIDATION_LOG];
	ev->id = id;
	ev->flags = flags;
	ev->frame = tr.frameCount;
	s_invLogHead++;

	if ( r_sceneInvalidateDebug && r_sceneInvalidateDebug->integer ) {
		ri.Printf( PRINT_ALL, "[VK][Scene] invalidate id=0x%llx flags=0x%x frame=%u\n",
			(unsigned long long)id, flags, tr.frameCount );
	}
}

uint32_t vk_scene_platform_consume_pending_dirty( void )
{
	uint32_t d = s_pendingDirty;
	s_pendingDirty = 0;
	return d;
}

qboolean vk_scene_platform_edit_transform( vkSceneId_t id, const vec3_t origin,
	const float *axis9 )
{
	vkSceneNode_t *n = NULL;
	uint32_t flags;
	int i;

	if ( !vk_scene_platform_active() || !s_state.liveEdit ) {
		return qfalse;
	}
	if ( !vk_scene_platform_find( id, &n ) || !origin ) {
		return qfalse;
	}
	if ( n->kind == VK_SCENE_KIND_BSP_WORLD ) {
		ri.Printf( PRINT_WARNING, "[VK][Scene] refuse transform edit on classic BSP world node\n" );
		return qfalse;
	}

	VectorCopy( n->worldOrigin, n->prevWorldOrigin );
	VectorCopy( origin, n->localOrigin );
	VectorCopy( origin, n->worldOrigin );
	if ( axis9 ) {
		for ( i = 0; i < 3; i++ ) {
			n->localAxis[i][0] = axis9[i * 3 + 0];
			n->localAxis[i][1] = axis9[i * 3 + 1];
			n->localAxis[i][2] = axis9[i * 3 + 2];
			n->worldAxis[i][0] = axis9[i * 3 + 0];
			n->worldAxis[i][1] = axis9[i * 3 + 1];
			n->worldAxis[i][2] = axis9[i * 3 + 2];
		}
	}
	n->revision++;
	n->dirty |= VK_SCENE_DIRTY_TRANSFORM | VK_SCENE_DIRTY_BOUNDS;
	s_state.editCount++;
	s_state.sceneRevision++;

	flags = VK_SCENE_DIRTY_TRANSFORM | VK_SCENE_DIRTY_BOUNDS;
	if ( n->kind == VK_SCENE_KIND_LIGHT ) {
		flags |= VK_SCENE_DIRTY_LIGHT | VK_SCENE_DIRTY_SHADOW | VK_SCENE_DIRTY_GI | VK_SCENE_DIRTY_VOLUME;
	} else if ( n->kind == VK_SCENE_KIND_PROBE ) {
		flags |= VK_SCENE_DIRTY_PROBE;
	} else {
		flags |= VK_SCENE_DIRTY_SHADOW | VK_SCENE_DIRTY_GI;
	}
	vk_scene_platform_note_invalidation( id, flags );

	if ( n->gpuHandle != 0u && vk_gpu_scene_active() ) {
		float flat[9];
		for ( i = 0; i < 3; i++ ) {
			flat[i * 3 + 0] = n->worldAxis[i][0];
			flat[i * 3 + 1] = n->worldAxis[i][1];
			flat[i * 3 + 2] = n->worldAxis[i][2];
		}
		vk_gpu_scene_update_instance_transform( n->gpuHandle, flat, n->worldOrigin );
	}
	return qtrue;
}

qboolean vk_scene_platform_edit_visibility( vkSceneId_t id, qboolean visible )
{
	vkSceneNode_t *n = NULL;

	if ( !vk_scene_platform_active() || !s_state.liveEdit ) {
		return qfalse;
	}
	if ( !vk_scene_platform_find( id, &n ) ) {
		return qfalse;
	}
	n->visible = visible;
	n->revision++;
	n->dirty |= VK_SCENE_DIRTY_VISIBILITY;
	s_state.editCount++;
	vk_scene_platform_note_invalidation( id,
		VK_SCENE_DIRTY_VISIBILITY | VK_SCENE_DIRTY_SHADOW | VK_SCENE_DIRTY_GI );
	return qtrue;
}

qboolean vk_scene_platform_edit_material( vkSceneId_t id, uint32_t materialId )
{
	vkSceneNode_t *n = NULL;
	uint32_t flags;

	if ( !vk_scene_platform_active() || !s_state.liveEdit ) {
		return qfalse;
	}
	if ( !vk_scene_platform_find( id, &n ) ) {
		return qfalse;
	}
	n->materialId = materialId;
	n->revision++;
	n->dirty |= VK_SCENE_DIRTY_MATERIAL;
	s_state.editCount++;
	flags = VK_SCENE_DIRTY_MATERIAL;
	/* Emission/opacity unknown here — conservatively touch probes only when light-like. */
	if ( n->kind == VK_SCENE_KIND_LIGHT ) {
		flags |= VK_SCENE_DIRTY_GI | VK_SCENE_DIRTY_PROBE;
	}
	vk_scene_platform_note_invalidation( id, flags );
	return qtrue;
}

qboolean vk_scene_platform_link_gpu_instance( vkSceneId_t id, uint32_t gpuHandle )
{
	vkSceneNode_t *n = NULL;
	if ( !vk_scene_platform_find( id, &n ) ) {
		return qfalse;
	}
	n->gpuHandle = gpuHandle;
	return qtrue;
}

qboolean vk_scene_platform_link_light( vkSceneId_t id, uint32_t lightIndex )
{
	vkSceneNode_t *n = NULL;
	if ( !vk_scene_platform_find( id, &n ) ) {
		return qfalse;
	}
	n->kind = VK_SCENE_KIND_LIGHT;
	n->lightIndex = lightIndex;
	n->lightAuthored = qfalse;
	return qtrue;
}

qboolean vk_scene_platform_edit_light_color( vkSceneId_t id, const vec3_t color )
{
	vkSceneNode_t *n = NULL;
	if ( !vk_scene_platform_active() || !s_state.liveEdit ) {
		return qfalse;
	}
	if ( !vk_scene_platform_find( id, &n ) || !color || n->kind != VK_SCENE_KIND_LIGHT ) {
		return qfalse;
	}
	VectorCopy( color, n->lightColor );
	n->lightHasColor = qtrue;
	n->revision++;
	n->dirty |= VK_SCENE_DIRTY_LIGHT;
	s_state.editCount++;
	vk_scene_platform_note_invalidation( id,
		VK_SCENE_DIRTY_LIGHT | VK_SCENE_DIRTY_SHADOW | VK_SCENE_DIRTY_GI | VK_SCENE_DIRTY_VOLUME );
	return qtrue;
}

qboolean vk_scene_platform_edit_light_radius( vkSceneId_t id, float radius )
{
	vkSceneNode_t *n = NULL;
	if ( !vk_scene_platform_active() || !s_state.liveEdit ) {
		return qfalse;
	}
	if ( !vk_scene_platform_find( id, &n ) || n->kind != VK_SCENE_KIND_LIGHT ) {
		return qfalse;
	}
	n->lightRadius = radius > 0.0f ? radius : 0.0f;
	n->lightHasRadius = qtrue;
	n->revision++;
	n->dirty |= VK_SCENE_DIRTY_LIGHT | VK_SCENE_DIRTY_BOUNDS;
	s_state.editCount++;
	vk_scene_platform_note_invalidation( id,
		VK_SCENE_DIRTY_LIGHT | VK_SCENE_DIRTY_SHADOW | VK_SCENE_DIRTY_GI | VK_SCENE_DIRTY_VOLUME );
	return qtrue;
}

qboolean vk_scene_platform_edit_light_area( vkSceneId_t id, float halfW, float halfH )
{
	vkSceneNode_t *n = NULL;
	if ( !vk_scene_platform_active() || !s_state.liveEdit ) {
		return qfalse;
	}
	if ( !vk_scene_platform_find( id, &n ) || n->kind != VK_SCENE_KIND_LIGHT ) {
		return qfalse;
	}
	n->lightHalfW = halfW > 0.0f ? halfW : 0.01f;
	n->lightHalfH = halfH > 0.0f ? halfH : 0.01f;
	n->lightHasArea = qtrue;
	n->revision++;
	n->dirty |= VK_SCENE_DIRTY_LIGHT | VK_SCENE_DIRTY_BOUNDS;
	s_state.editCount++;
	vk_scene_platform_note_invalidation( id,
		VK_SCENE_DIRTY_LIGHT | VK_SCENE_DIRTY_SHADOW | VK_SCENE_DIRTY_GI );
	return qtrue;
}

void vk_scene_platform_apply_light_override( uint32_t lightIndex, dlight_t *dl )
{
	uint32_t i;
	if ( !dl || !vk_scene_platform_active() ) {
		return;
	}
	for ( i = 0; i < s_state.nodeCount; i++ ) {
		vkSceneNode_t *n = &s_nodes[i];
		if ( !n->alive || n->kind != VK_SCENE_KIND_LIGHT || n->lightAuthored ) {
			continue;
		}
		if ( n->lightIndex != lightIndex ) {
			continue;
		}
		if ( !n->visible ) {
			dl->radius = 0.0f;
			VectorClear( dl->color );
			return;
		}
		VectorCopy( n->worldOrigin, dl->origin );
		if ( n->lightHasColor ) {
			VectorCopy( n->lightColor, dl->color );
		}
		if ( n->lightHasRadius ) {
			dl->radius = n->lightRadius;
		}
		if ( n->lightHasArea ) {
			dl->area = qtrue;
			dl->linear = qfalse;
			dl->areaHalfWidth = n->lightHalfW;
			dl->areaHalfHeight = n->lightHalfH;
			VectorSet( dl->areaRight, n->worldAxis[0][0], n->worldAxis[0][1], n->worldAxis[0][2] );
			VectorSet( dl->areaUp, n->worldAxis[2][0], n->worldAxis[2][1], n->worldAxis[2][2] );
			if ( VectorLength( dl->areaRight ) < 1e-4f ) {
				VectorSet( dl->areaRight, 1.0f, 0.0f, 0.0f );
			}
			if ( VectorLength( dl->areaUp ) < 1e-4f ) {
				VectorSet( dl->areaUp, 0.0f, 0.0f, 1.0f );
			}
			VectorNormalize( dl->areaRight );
			VectorNormalize( dl->areaUp );
		}
		return;
	}
}

uint32_t vk_scene_platform_append_authored_lights( float *base, uint32_t n, uint32_t max_pack )
{
	uint32_t i;
	const uint32_t headerFloats = 8u;
	const uint32_t strideFloats = 16u;

	if ( !base || !vk_scene_platform_active() || max_pack == 0u ) {
		return n;
	}

	for ( i = 0; i < s_state.nodeCount && n < max_pack; i++ ) {
		const vkSceneNode_t *node = &s_nodes[i];
		float *rec;
		vec3_t halfU, halfV;
		float diag;
		float colorScale = 1.0f;

		if ( !node->alive || node->kind != VK_SCENE_KIND_LIGHT || !node->lightAuthored ) {
			continue;
		}
		if ( !node->visible ) {
			continue;
		}

		rec = base + headerFloats + n * strideFloats;
		rec[0] = node->worldOrigin[0];
		rec[1] = node->worldOrigin[1];
		rec[2] = node->worldOrigin[2];
		rec[3] = node->lightHasRadius ? node->lightRadius : 256.0f;
		rec[4] = node->lightHasColor ? node->lightColor[0] * colorScale : 1.0f;
		rec[5] = node->lightHasColor ? node->lightColor[1] * colorScale : 1.0f;
		rec[6] = node->lightHasColor ? node->lightColor[2] * colorScale : 1.0f;

		if ( node->lightHasArea ) {
			float hw = node->lightHalfW > 0.0f ? node->lightHalfW : 16.0f;
			float hh = node->lightHalfH > 0.0f ? node->lightHalfH : 16.0f;
			vec3_t right, up;
			VectorSet( right, node->worldAxis[0][0], node->worldAxis[0][1], node->worldAxis[0][2] );
			VectorSet( up, node->worldAxis[2][0], node->worldAxis[2][1], node->worldAxis[2][2] );
			if ( VectorLength( right ) < 1e-4f ) {
				VectorSet( right, 1.0f, 0.0f, 0.0f );
			}
			if ( VectorLength( up ) < 1e-4f ) {
				VectorSet( up, 0.0f, 0.0f, 1.0f );
			}
			VectorNormalize( right );
			VectorNormalize( up );
			VectorScale( right, hw, halfU );
			VectorScale( up, hh, halfV );
			diag = sqrtf( hw * hw + hh * hh );
			if ( rec[3] < diag * 2.0f ) {
				rec[3] = diag * 2.0f;
			}
			rec[7] = 2.0f;
			rec[8] = halfU[0];
			rec[9] = halfU[1];
			rec[10] = halfU[2];
			rec[11] = 0.0f;
			rec[12] = halfV[0];
			rec[13] = halfV[1];
			rec[14] = halfV[2];
			rec[15] = 0.0f;
		} else {
			rec[7] = 0.0f;
			rec[8] = rec[9] = rec[10] = 0.0f;
			rec[11] = -1.0f;
			rec[12] = -1.0f;
			rec[13] = rec[3];
			rec[14] = 0.0f;
			rec[15] = 0.0f;
		}
		n++;
	}
	return n;
}

vkSceneId_t vk_scene_platform_spawn_area_light( const vec3_t origin, float halfW, float halfH,
	const vec3_t color, float radius )
{
	vkSceneId_t id;
	vkSceneNode_t *n = NULL;

	if ( !vk_scene_platform_active() ) {
		return 0;
	}
	vk_scene_platform_ensure_world_nodes();
	id = vk_scene_platform_create_node( VK_SCENE_KIND_LIGHT, "area_light", s_rootId );
	if ( !vk_scene_platform_find( id, &n ) ) {
		return 0;
	}
	VectorCopy( origin, n->localOrigin );
	VectorCopy( origin, n->worldOrigin );
	n->lightAuthored = qtrue;
	n->lightIndex = ~0u;
	n->lightHasArea = qtrue;
	n->lightHalfW = halfW > 0.0f ? halfW : 16.0f;
	n->lightHalfH = halfH > 0.0f ? halfH : 16.0f;
	n->lightHasColor = qtrue;
	VectorCopy( color, n->lightColor );
	n->lightHasRadius = qtrue;
	n->lightRadius = radius > 0.0f ? radius : 256.0f;
	n->visible = qtrue;
	vk_scene_platform_note_invalidation( id,
		VK_SCENE_DIRTY_LIGHT | VK_SCENE_DIRTY_SHADOW | VK_SCENE_DIRTY_GI | VK_SCENE_DIRTY_VOLUME );
	s_state.editCount++;
	return id;
}

const char *vk_scene_platform_kind_name( vkSceneNodeKind_t k )
{
	static const char *names[] = {
		"none", "root", "bsp_world", "static_prop", "dynamic_prop", "light",
		"camera", "volume", "probe", "decal", "emitter", "usd_prim", "district"
	};
	if ( k < 0 || k >= VK_SCENE_KIND_COUNT ) {
		return "invalid";
	}
	return names[k];
}

void vk_scene_platform_status_f( void )
{
	uint32_t i;

	ri.Printf( PRINT_ALL, "=== Scene Platform (Cinematic Engine 1.0 Environment Slice) ===\n" );
	ri.Printf( PRINT_ALL, "active         : %s liveEdit=%s\n",
		vk_scene_platform_active() ? "yes" : "no",
		s_state.liveEdit ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "nodes          : alive=%u stored=%u rev=%u compile=%u edits=%u\n",
		s_state.aliveCount, s_state.nodeCount, s_state.sceneRevision,
		s_state.compileRevision, s_state.editCount );
	ri.Printf( PRINT_ALL, "invalidations  : total=%u lastFlags=0x%x pending=0x%x\n",
		s_state.invalidationCount, s_state.lastInvalidationFlags, s_pendingDirty );
	ri.Printf( PRINT_ALL, "ownership      : classic BSP = %s (not GPU-scene driven)\n",
		s_bspWorldId ? "registered" : "pending" );
	ri.Printf( PRINT_ALL, "gpu_scene      : %s\n", vk_gpu_scene_active() ? "active" : "inactive" );
	ri.Printf( PRINT_ALL, "commands       : scene_node_status <idHex>, scene_invalidate_debug\n" );
	ri.Printf( PRINT_ALL, "               : scene_set_origin <idHex> x y z\n" );
	ri.Printf( PRINT_ALL, "               : scene_set_visible <idHex> 0|1\n" );
	ri.Printf( PRINT_ALL, "               : light_status [idHex], light_spawn_area, light_set_color/radius\n" );

	for ( i = 0; i < s_state.nodeCount && i < 12u; i++ ) {
		const vkSceneNode_t *n = &s_nodes[i];
		if ( !n->alive ) {
			continue;
		}
		ri.Printf( PRINT_ALL, "  [%u] id=0x%llx kind=%-12s name=%-20s vis=%d gpu=%u dirty=0x%x\n",
			i, (unsigned long long)n->id, vk_scene_platform_kind_name( n->kind ),
			n->name, n->visible ? 1 : 0, n->gpuHandle, n->dirty );
	}
	if ( s_state.nodeCount > 12u ) {
		ri.Printf( PRINT_ALL, "  ... %u more\n", s_state.nodeCount - 12u );
	}
}

void vk_scene_platform_node_status_f( void )
{
	vkSceneId_t id;
	const vkSceneNode_t *n;
	const char *cmd = ri.Cmd_Argv( 0 );

	/* Console edit wrappers share this entry for argc routing. */
	if ( !Q_stricmp( cmd, "scene_set_origin" ) ) {
		vec3_t o;
		if ( ri.Cmd_Argc() < 5 ) {
			ri.Printf( PRINT_ALL, "Usage: scene_set_origin <idHex> x y z\n" );
			return;
		}
		id = (vkSceneId_t)strtoull( ri.Cmd_Argv( 1 ), NULL, 0 );
		o[0] = (float)atof( ri.Cmd_Argv( 2 ) );
		o[1] = (float)atof( ri.Cmd_Argv( 3 ) );
		o[2] = (float)atof( ri.Cmd_Argv( 4 ) );
		if ( vk_scene_platform_edit_transform( id, o, NULL ) ) {
			ri.Printf( PRINT_ALL, "scene_set_origin: ok\n" );
		} else {
			ri.Printf( PRINT_ALL, "scene_set_origin: failed\n" );
		}
		return;
	}
	if ( !Q_stricmp( cmd, "scene_set_visible" ) ) {
		if ( ri.Cmd_Argc() < 3 ) {
			ri.Printf( PRINT_ALL, "Usage: scene_set_visible <idHex> 0|1\n" );
			return;
		}
		id = (vkSceneId_t)strtoull( ri.Cmd_Argv( 1 ), NULL, 0 );
		if ( vk_scene_platform_edit_visibility( id, atoi( ri.Cmd_Argv( 2 ) ) ? qtrue : qfalse ) ) {
			ri.Printf( PRINT_ALL, "scene_set_visible: ok\n" );
		} else {
			ri.Printf( PRINT_ALL, "scene_set_visible: failed\n" );
		}
		return;
	}

	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "Usage: scene_node_status <idHex>\n" );
		return;
	}
	id = (vkSceneId_t)strtoull( ri.Cmd_Argv( 1 ), NULL, 0 );
	n = vk_scene_platform_get( id );
	if ( !n ) {
		ri.Printf( PRINT_ALL, "scene_node_status: id 0x%llx not found\n", (unsigned long long)id );
		return;
	}
	ri.Printf( PRINT_ALL, "=== scene node 0x%llx ===\n", (unsigned long long)id );
	ri.Printf( PRINT_ALL, "kind=%s name=%s parent=0x%llx rev=%u\n",
		vk_scene_platform_kind_name( n->kind ), n->name,
		(unsigned long long)n->parent, n->revision );
	ri.Printf( PRINT_ALL, "origin=%.2f %.2f %.2f visible=%d layer=%u\n",
		n->worldOrigin[0], n->worldOrigin[1], n->worldOrigin[2],
		n->visible ? 1 : 0, n->layer );
	ri.Printf( PRINT_ALL, "material=%u lightIndex=%u gpuHandle=%u dirty=0x%x cell=%u\n",
		n->materialId, n->lightIndex, n->gpuHandle, n->dirty, n->streamingCell );
}

void vk_scene_platform_invalidate_debug_f( void )
{
	uint32_t i;
	uint32_t count = s_invLogHead < VK_SCENE_INVALIDATION_LOG ? s_invLogHead : VK_SCENE_INVALIDATION_LOG;

	ri.Printf( PRINT_ALL, "=== Scene invalidation log (newest last, %u) ===\n", count );
	for ( i = 0; i < count; i++ ) {
		uint32_t idx = ( s_invLogHead - count + i ) % VK_SCENE_INVALIDATION_LOG;
		const vkSceneInvalidationEvent_t *ev = &s_invLog[idx];
		ri.Printf( PRINT_ALL, "  frame=%u id=0x%llx flags=0x%x\n",
			ev->frame, (unsigned long long)ev->id, ev->flags );
	}
	ri.Printf( PRINT_ALL, "pending dirty mask=0x%x\n", s_pendingDirty );
}

void vk_scene_platform_light_status_f( void )
{
	const char *cmd = ri.Cmd_Argv( 0 );
	vkSceneId_t id;
	const vkSceneNode_t *n;
	uint32_t i;

	if ( !Q_stricmp( cmd, "light_spawn_area" ) ) {
		vec3_t o, c;
		float hw, hh, rad;
		if ( ri.Cmd_Argc() < 9 ) {
			ri.Printf( PRINT_ALL,
				"Usage: light_spawn_area x y z halfW halfH r g b [radius]\n" );
			return;
		}
		o[0] = (float)atof( ri.Cmd_Argv( 1 ) );
		o[1] = (float)atof( ri.Cmd_Argv( 2 ) );
		o[2] = (float)atof( ri.Cmd_Argv( 3 ) );
		hw = (float)atof( ri.Cmd_Argv( 4 ) );
		hh = (float)atof( ri.Cmd_Argv( 5 ) );
		c[0] = (float)atof( ri.Cmd_Argv( 6 ) );
		c[1] = (float)atof( ri.Cmd_Argv( 7 ) );
		c[2] = (float)atof( ri.Cmd_Argv( 8 ) );
		rad = ( ri.Cmd_Argc() >= 10 ) ? (float)atof( ri.Cmd_Argv( 9 ) ) : 256.0f;
		id = vk_scene_platform_spawn_area_light( o, hw, hh, c, rad );
		if ( id ) {
			ri.Printf( PRINT_ALL, "light_spawn_area: id=0x%llx\n", (unsigned long long)id );
		} else {
			ri.Printf( PRINT_ALL, "light_spawn_area: failed (enable r_scenePlatform)\n" );
		}
		return;
	}
	if ( !Q_stricmp( cmd, "light_set_color" ) ) {
		vec3_t c;
		if ( ri.Cmd_Argc() < 5 ) {
			ri.Printf( PRINT_ALL, "Usage: light_set_color <idHex> r g b\n" );
			return;
		}
		id = (vkSceneId_t)strtoull( ri.Cmd_Argv( 1 ), NULL, 0 );
		c[0] = (float)atof( ri.Cmd_Argv( 2 ) );
		c[1] = (float)atof( ri.Cmd_Argv( 3 ) );
		c[2] = (float)atof( ri.Cmd_Argv( 4 ) );
		ri.Printf( PRINT_ALL, "light_set_color: %s\n",
			vk_scene_platform_edit_light_color( id, c ) ? "ok" : "failed" );
		return;
	}
	if ( !Q_stricmp( cmd, "light_set_radius" ) ) {
		if ( ri.Cmd_Argc() < 3 ) {
			ri.Printf( PRINT_ALL, "Usage: light_set_radius <idHex> radius\n" );
			return;
		}
		id = (vkSceneId_t)strtoull( ri.Cmd_Argv( 1 ), NULL, 0 );
		ri.Printf( PRINT_ALL, "light_set_radius: %s\n",
			vk_scene_platform_edit_light_radius( id, (float)atof( ri.Cmd_Argv( 2 ) ) ) ? "ok" : "failed" );
		return;
	}

	ri.Printf( PRINT_ALL, "=== Light status (scene platform) ===\n" );
	if ( ri.Cmd_Argc() >= 2 ) {
		id = (vkSceneId_t)strtoull( ri.Cmd_Argv( 1 ), NULL, 0 );
		n = vk_scene_platform_get( id );
		if ( !n || n->kind != VK_SCENE_KIND_LIGHT ) {
			ri.Printf( PRINT_ALL, "light_status: id 0x%llx not a light\n", (unsigned long long)id );
			return;
		}
		ri.Printf( PRINT_ALL, "id=0x%llx authored=%d dlightIdx=%u visible=%d\n",
			(unsigned long long)id, n->lightAuthored ? 1 : 0, n->lightIndex, n->visible ? 1 : 0 );
		ri.Printf( PRINT_ALL, "origin=%.1f %.1f %.1f\n",
			n->worldOrigin[0], n->worldOrigin[1], n->worldOrigin[2] );
		ri.Printf( PRINT_ALL, "color=%s (%.3f %.3f %.3f) radius=%s (%.1f)\n",
			n->lightHasColor ? "set" : "-",
			n->lightColor[0], n->lightColor[1], n->lightColor[2],
			n->lightHasRadius ? "set" : "-", n->lightRadius );
		ri.Printf( PRINT_ALL, "area=%s half=%.1f x %.1f dirty=0x%x\n",
			n->lightHasArea ? "yes" : "no", n->lightHalfW, n->lightHalfH, n->dirty );
		return;
	}
	for ( i = 0; i < s_state.nodeCount; i++ ) {
		n = &s_nodes[i];
		if ( !n->alive || n->kind != VK_SCENE_KIND_LIGHT ) {
			continue;
		}
		ri.Printf( PRINT_ALL, "  id=0x%llx %s area=%d color=(%.2f %.2f %.2f) rad=%.0f\n",
			(unsigned long long)n->id,
			n->lightAuthored ? "authored" : "linked",
			n->lightHasArea ? 1 : 0,
			n->lightColor[0], n->lightColor[1], n->lightColor[2],
			n->lightRadius );
	}
}
