/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Compute/CPU meshlet bake + frustum cull (chocolate). No mesh shaders required.
See docs/MESHLETS.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk_meshlets.h"

static cvar_t *r_meshlets;
static qboolean s_cmds;
static int s_bakeCount;
static int s_cullVisible;
static int s_cullTotal;

static void Meshlets_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"[VK][meshlets] active=%d bakeCalls=%d lastCull visible=%d / total=%d\n",
		R_Meshlets_Active() ? 1 : 0, s_bakeCount, s_cullVisible, s_cullTotal );
}

void R_Meshlets_Init( void )
{
	r_meshlets = ri.Cvar_Get( "r_meshlets", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_meshlets, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_meshlets,
		"Meshlet bake + CPU frustum cull for dense static meshes (Nanite-lite). Default 0." );
	ri.Cvar_SetGroup( r_meshlets, CVG_RENDERER );

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "meshlet_status", Meshlets_Status_f );
		s_cmds = qtrue;
	}
	if ( r_meshlets->integer ) {
		ri.Printf( PRINT_ALL, "[VK][meshlets] r_meshlets=1 (CPU frustum cull; MDI path optional)\n" );
	}
}

void R_Meshlets_Shutdown( void )
{
}

qboolean R_Meshlets_Active( void )
{
	return ( r_meshlets && r_meshlets->integer ) ? qtrue : qfalse;
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
