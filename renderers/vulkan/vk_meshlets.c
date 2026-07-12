/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Compute/CPU meshlet bake + frustum cull (chocolate). No mesh shaders required.
Bake-at-load cache stores local-space AABBs; cull transforms to world.
See docs/MESHLETS.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk_meshlets.h"

#define MESHLET_CACHE_SLOTS 256

typedef struct {
	const void *key;
	int count;
	meshlet_t meshlets[MESHLET_MAX_PER_SURFACE];
} meshlet_cache_entry_t;

static cvar_t *r_meshlets;
static cvar_t *r_meshletsMdi;
static qboolean s_cmds;
static int s_bakeCount;
static int s_cacheHits;
static int s_cacheMisses;
static int s_cullVisible;
static int s_cullTotal;
static int s_mdiCount;
static int s_mdiTris;
static meshlet_cache_entry_t s_cache[MESHLET_CACHE_SLOTS];
static meshlet_draw_cmd_t s_mdiCmds[MESHLET_MAX_PER_SURFACE];

static void Meshlets_Status_f( void )
{
	int used = 0;
	int i;

	for ( i = 0; i < MESHLET_CACHE_SLOTS; i++ ) {
		if ( s_cache[i].key ) {
			used++;
		}
	}
	ri.Printf( PRINT_ALL,
		"[VK][meshlets] active=%d bakeCalls=%d cache hits=%d misses=%d slots=%d/%d\n"
		"  lastCull visible=%d / total=%d mdi=%d cmds=%d tris=%d\n",
		R_Meshlets_Active() ? 1 : 0, s_bakeCount, s_cacheHits, s_cacheMisses,
		used, MESHLET_CACHE_SLOTS, s_cullVisible, s_cullTotal,
		( r_meshletsMdi && r_meshletsMdi->integer ) ? 1 : 0,
		s_mdiCount, s_mdiTris );
}

void R_Meshlets_Init( void )
{
	r_meshlets = ri.Cvar_Get( "r_meshlets", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_meshlets, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_meshlets,
		"Meshlet bake + CPU frustum cull for dense static meshes (Nanite-lite). Default 0." );
	ri.Cvar_SetGroup( r_meshlets, CVG_RENDERER );

	r_meshletsMdi = ri.Cvar_Get( "r_meshletsMdi", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_meshletsMdi, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_meshletsMdi,
		"Pack VkDrawIndexedIndirectCommand list from visible meshlets (MDI scaffold; draw still all-or-nothing until GPU path)." );
	ri.Cvar_SetGroup( r_meshletsMdi, CVG_RENDERER );

	Com_Memset( s_cache, 0, sizeof( s_cache ) );
	s_bakeCount = s_cacheHits = s_cacheMisses = 0;
	s_cullVisible = s_cullTotal = 0;
	s_mdiCount = s_mdiTris = 0;

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "meshlet_status", Meshlets_Status_f );
		s_cmds = qtrue;
	}
	if ( r_meshlets->integer ) {
		ri.Printf( PRINT_ALL, "[VK][meshlets] r_meshlets=1 (bake-at-load cache + CPU frustum cull%s)\n",
			( r_meshletsMdi && r_meshletsMdi->integer ) ? "; MDI pack on" : "" );
	}
}

void R_Meshlets_Shutdown( void )
{
	Com_Memset( s_cache, 0, sizeof( s_cache ) );
}

qboolean R_Meshlets_Active( void )
{
	return ( r_meshlets && r_meshlets->integer ) ? qtrue : qfalse;
}

void R_Meshlets_InvalidateCache( void )
{
	Com_Memset( s_cache, 0, sizeof( s_cache ) );
	s_cacheHits = s_cacheMisses = 0;
}

int R_Meshlets_Bake( const vec3_t *positions, int numVerts, const int *indexes, int numIndexes,
	meshlet_t *out, int maxOut )
{
	int mcount = 0;
	int cursor = 0;

	if ( !positions || !indexes || !out || maxOut <= 0 || numIndexes < 3 ) {
		return 0;
	}

	s_bakeCount++;
	while ( cursor + 3 <= numIndexes && mcount < maxOut ) {
		meshlet_t *m = &out[mcount];
		int triBudget = MESHLET_MAX_TRIS;
		int vertBudget = MESHLET_MAX_VERTS;
		int localVerts = 0;
		int localTris = 0;
		int start = cursor;

		ClearBounds( m->mins, m->maxs );
		m->firstIndex = (uint16_t)cursor;
		m->firstVert = 0;
		m->vertCount = 0;

		while ( cursor + 2 < numIndexes && localTris < triBudget && localVerts < vertBudget ) {
			int a = indexes[cursor];
			int b = indexes[cursor + 1];
			int c = indexes[cursor + 2];
			if ( a < 0 || b < 0 || c < 0 || a >= numVerts || b >= numVerts || c >= numVerts ) {
				cursor += 3;
				continue;
			}
			AddPointToBounds( positions[a], m->mins, m->maxs );
			AddPointToBounds( positions[b], m->mins, m->maxs );
			AddPointToBounds( positions[c], m->mins, m->maxs );
			localTris++;
			localVerts += 3;
			cursor += 3;
		}
		m->indexCount = (uint16_t)( cursor - start );
		m->vertCount = (uint16_t)( localVerts > MESHLET_MAX_VERTS ? MESHLET_MAX_VERTS : localVerts );
		if ( m->indexCount >= 3 ) {
			mcount++;
		} else {
			break;
		}
	}
	return mcount;
}

static meshlet_cache_entry_t *Meshlets_FindSlot( const void *key, qboolean create )
{
	uintptr_t h;
	int i;

	if ( !key ) {
		return NULL;
	}
	h = (uintptr_t)key;
	for ( i = 0; i < MESHLET_CACHE_SLOTS; i++ ) {
		int idx = (int)( ( h + (uintptr_t)i ) % (uintptr_t)MESHLET_CACHE_SLOTS );
		if ( s_cache[idx].key == key ) {
			return &s_cache[idx];
		}
		if ( create && s_cache[idx].key == NULL ) {
			s_cache[idx].key = key;
			s_cache[idx].count = 0;
			return &s_cache[idx];
		}
	}
	if ( create ) {
		/* Evict hashed slot */
		int idx = (int)( h % (uintptr_t)MESHLET_CACHE_SLOTS );
		s_cache[idx].key = key;
		s_cache[idx].count = 0;
		return &s_cache[idx];
	}
	return NULL;
}

int R_Meshlets_CacheLocal( const void *key, const vec3_t *positions, int numVerts,
	const int *indexes, int numIndexes )
{
	meshlet_cache_entry_t *e;

	if ( !R_Meshlets_Active() || !key ) {
		return 0;
	}
	e = Meshlets_FindSlot( key, qtrue );
	if ( !e ) {
		return 0;
	}
	e->count = R_Meshlets_Bake( positions, numVerts, indexes, numIndexes,
		e->meshlets, MESHLET_MAX_PER_SURFACE );
	return e->count;
}

int R_Meshlets_Lookup( const void *key, const meshlet_t **outMeshlets )
{
	meshlet_cache_entry_t *e = Meshlets_FindSlot( key, qfalse );

	if ( !e || e->count <= 0 ) {
		s_cacheMisses++;
		if ( outMeshlets ) {
			*outMeshlets = NULL;
		}
		return 0;
	}
	s_cacheHits++;
	if ( outMeshlets ) {
		*outMeshlets = e->meshlets;
	}
	return e->count;
}

static qboolean Meshlet_AABBInPlane( const vec3_t mins, const vec3_t maxs, const cplane_t *plane )
{
	int k;
	float maxDot = -1e30f;
	for ( k = 0; k < 8; k++ ) {
		vec3_t p;
		float d;
		p[0] = ( k & 1 ) ? maxs[0] : mins[0];
		p[1] = ( k & 2 ) ? maxs[1] : mins[1];
		p[2] = ( k & 4 ) ? maxs[2] : mins[2];
		d = DotProduct( p, plane->normal ) - plane->dist;
		if ( d > maxDot ) {
			maxDot = d;
		}
	}
	return ( maxDot >= 0.0f ) ? qtrue : qfalse;
}

int R_Meshlets_CullViewFrustum( const meshlet_t *meshlets, int count, int *visible, int maxVisible )
{
	int i, n = 0;
	int p;

	s_cullTotal = count;
	s_cullVisible = 0;
	if ( !meshlets || !visible || maxVisible <= 0 ) {
		return 0;
	}
	for ( i = 0; i < count && n < maxVisible; i++ ) {
		qboolean inside = qtrue;
		for ( p = 0; p < 5; p++ ) {
			if ( !Meshlet_AABBInPlane( meshlets[i].mins, meshlets[i].maxs, &tr.viewParms.frustum[p] ) ) {
				inside = qfalse;
				break;
			}
		}
		if ( inside ) {
			visible[n++] = i;
		}
	}
	s_cullVisible = n;
	return n;
}

int R_Meshlets_CullViewFrustumXform( const meshlet_t *meshlets, int count,
	const float entityAxis[3][3], const vec3_t entityOrigin, int *visible, int maxVisible )
{
	int i, n = 0;
	int p, c;

	s_cullTotal = count;
	s_cullVisible = 0;
	if ( !meshlets || !visible || maxVisible <= 0 ) {
		return 0;
	}
	for ( i = 0; i < count && n < maxVisible; i++ ) {
		vec3_t wmins, wmaxs, corners[8];
		qboolean inside = qtrue;

		ClearBounds( wmins, wmaxs );
		for ( c = 0; c < 8; c++ ) {
			vec3_t local, world;
			local[0] = ( c & 1 ) ? meshlets[i].maxs[0] : meshlets[i].mins[0];
			local[1] = ( c & 2 ) ? meshlets[i].maxs[1] : meshlets[i].mins[1];
			local[2] = ( c & 4 ) ? meshlets[i].maxs[2] : meshlets[i].mins[2];
			VectorRotate( local, (const vec3_t *)entityAxis, world );
			VectorAdd( world, entityOrigin, world );
			AddPointToBounds( world, wmins, wmaxs );
			VectorCopy( world, corners[c] );
		}
		for ( p = 0; p < 5; p++ ) {
			if ( !Meshlet_AABBInPlane( wmins, wmaxs, &tr.viewParms.frustum[p] ) ) {
				inside = qfalse;
				break;
			}
		}
		if ( inside ) {
			visible[n++] = i;
		}
		(void)corners;
	}
	s_cullVisible = n;
	return n;
}

int R_Meshlets_PackIndirect( const meshlet_t *meshlets, const int *visible, int visibleCount,
	meshlet_draw_cmd_t *outCmds, int maxCmds )
{
	int i;
	int n = 0;
	int tris = 0;

	s_mdiCount = 0;
	s_mdiTris = 0;
	if ( !meshlets || !visible || !outCmds || maxCmds <= 0 || visibleCount <= 0 ) {
		return 0;
	}
	if ( !r_meshletsMdi || !r_meshletsMdi->integer ) {
		return 0;
	}

	for ( i = 0; i < visibleCount && n < maxCmds; i++ ) {
		int idx = visible[i];
		meshlet_draw_cmd_t *cmd;

		if ( idx < 0 ) {
			continue;
		}
		cmd = &outCmds[n];
		cmd->indexCount = meshlets[idx].indexCount;
		cmd->instanceCount = 1u;
		cmd->firstIndex = meshlets[idx].firstIndex;
		cmd->vertexOffset = 0;
		cmd->firstInstance = 0;
		tris += (int)( meshlets[idx].indexCount / 3u );
		n++;
	}
	s_mdiCount = n;
	s_mdiTris = tris;
	if ( n > 0 && n <= MESHLET_MAX_PER_SURFACE ) {
		Com_Memcpy( s_mdiCmds, outCmds, (size_t)n * sizeof( meshlet_draw_cmd_t ) );
	}
	return n;
}

qboolean R_Meshlets_WantMdi( void )
{
	return ( R_Meshlets_Active() && r_meshletsMdi && r_meshletsMdi->integer ) ? qtrue : qfalse;
}
