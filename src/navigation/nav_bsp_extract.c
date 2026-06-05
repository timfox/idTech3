/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

BSP geometry extraction for Recast navmesh generation.
Walks the BSP world surfaces, extracts triangle soup from
faces, patches, and triangle surfaces, then feeds the
geometry to the Recast rasterizer.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/qfiles.h"
#include "nav_bsp_extract.h"

#define NAV_MAX_TRIS  (256 * 1024)
#define NAV_MAX_VERTS (NAV_MAX_TRIS * 3)

static float navVerts[NAV_MAX_VERTS * 3];
static int   navTris[NAV_MAX_TRIS * 3];
static int   navVertCount = 0;
static int   navTriCount = 0;

void Nav_BSP_ClearGeometry(void) {
	navVertCount = 0;
	navTriCount = 0;
}

int Nav_BSP_AddVertex(float x, float y, float z) {
	if (navVertCount >= NAV_MAX_VERTS) return navVertCount - 1;
	int idx = navVertCount++;
	navVerts[idx * 3 + 0] = x;
	navVerts[idx * 3 + 1] = z;
	navVerts[idx * 3 + 2] = -y;
	return idx;
}

void Nav_BSP_AddTriangle(int v0, int v1, int v2) {
	if (navTriCount >= NAV_MAX_TRIS) return;
	int idx = navTriCount++;
	navTris[idx * 3 + 0] = v0;
	navTris[idx * 3 + 1] = v1;
	navTris[idx * 3 + 2] = v2;
}

float *Nav_BSP_GetVerts(void) { return navVerts; }
int   *Nav_BSP_GetTris(void) { return navTris; }
int    Nav_BSP_GetVertCount(void) { return navVertCount; }
int    Nav_BSP_GetTriCount(void) { return navTriCount; }

static void Nav_BSP_AddBrushTopQuad( const vec3_t mins, const vec3_t maxs ) {
	int v0, v1, v2, v3;

	v0 = Nav_BSP_AddVertex( mins[0], mins[1], maxs[2] );
	v1 = Nav_BSP_AddVertex( maxs[0], mins[1], maxs[2] );
	v2 = Nav_BSP_AddVertex( maxs[0], maxs[1], maxs[2] );
	v3 = Nav_BSP_AddVertex( mins[0], maxs[1], maxs[2] );
	Nav_BSP_AddTriangle( v0, v1, v2 );
	Nav_BSP_AddTriangle( v0, v2, v3 );
}

static qboolean Nav_BSP_BoundsFromBrush( dplane_t *planes, int numPlanes,
	dbrushside_t *sides, int numSides, int firstSide, int brushNumSides,
	vec3_t outMins, vec3_t outMaxs ) {
	int i, planeNum;
	vec3_t mins, maxs;
	qboolean haveMins[3], haveMaxs[3];

	if ( firstSide < 0 || brushNumSides < 6 || firstSide + brushNumSides > numSides ) {
		return qfalse;
	}

	VectorSet( mins, 0, 0, 0 );
	VectorSet( maxs, 0, 0, 0 );
	memset( haveMins, 0, sizeof( haveMins ) );
	memset( haveMaxs, 0, sizeof( haveMaxs ) );

	for ( i = 0; i < brushNumSides; i++ ) {
		dbrushside_t *side = &sides[firstSide + i];
		dplane_t *plane;

		planeNum = LittleLong( side->planeNum );
		if ( planeNum < 0 || planeNum >= numPlanes ) {
			return qfalse;
		}
		float nx, ny, nz, pdist;

		plane = &planes[planeNum];
		nx = LittleFloat( plane->normal[0] );
		ny = LittleFloat( plane->normal[1] );
		nz = LittleFloat( plane->normal[2] );
		pdist = LittleFloat( plane->dist );
		if ( nx == 1.0f && !haveMins[0] ) {
			mins[0] = pdist;
			haveMins[0] = qtrue;
		} else if ( nx == -1.0f && !haveMaxs[0] ) {
			maxs[0] = -pdist;
			haveMaxs[0] = qtrue;
		} else if ( ny == 1.0f && !haveMins[1] ) {
			mins[1] = pdist;
			haveMins[1] = qtrue;
		} else if ( ny == -1.0f && !haveMaxs[1] ) {
			maxs[1] = -pdist;
			haveMaxs[1] = qtrue;
		} else if ( nz == 1.0f && !haveMins[2] ) {
			mins[2] = pdist;
			haveMins[2] = qtrue;
		} else if ( nz == -1.0f && !haveMaxs[2] ) {
			maxs[2] = -pdist;
			haveMaxs[2] = qtrue;
		}
	}

	if ( !haveMins[0] || !haveMaxs[0] || !haveMins[1] || !haveMaxs[1] ||
		!haveMins[2] || !haveMaxs[2] ) {
		return qfalse;
	}
	if ( maxs[0] <= mins[0] || maxs[1] <= mins[1] || maxs[2] <= mins[2] ) {
		return qfalse;
	}

	VectorCopy( mins, outMins );
	VectorCopy( maxs, outMaxs );
	return qtrue;
}

qboolean Nav_BSP_ExtractFromSectorMap( int cellX, int cellY, float sectorSize ) {
	char mapName[MAX_QPATH];
	void *buf;
	int length;
	dheader_t header;
	byte *base;
	vec3_t worldOrigin;
	int i;
	lump_t *l;
	dplane_t *inPlanes;
	dbrushside_t *inSides;
	dbrush_t *inBrushes;
	int numPlanes, numSides, numBrushes;

	if ( sectorSize < 256.0f ) {
		sectorSize = 256.0f;
	}

	Nav_BSP_ClearGeometry();

	Com_sprintf( mapName, sizeof( mapName ), "maps/sector_%d_%d.bsp", cellX, cellY );
	length = FS_ReadFile( mapName, &buf );
	if ( length <= 0 || !buf ) {
		Com_DPrintf( "Nav: no sector BSP %s for extraction\n", mapName );
		return qfalse;
	}
	if ( (size_t)length < sizeof( dheader_t ) ) {
		FS_FreeFile( buf );
		return qfalse;
	}

	header = *(dheader_t *)buf;
	for ( i = 0; (size_t)i < sizeof( dheader_t ) / sizeof( int32_t ); i++ ) {
		( (int32_t *)&header )[i] = LittleLong( ( (int32_t *)&header )[i] );
	}
	if ( header.version != BSP_VERSION ) {
		FS_FreeFile( buf );
		return qfalse;
	}

	base = (byte *)buf;
	worldOrigin[0] = (float)cellX * sectorSize;
	worldOrigin[1] = (float)cellY * sectorSize;
	worldOrigin[2] = 0.0f;

	l = &header.lumps[LUMP_PLANES];
	inPlanes = (dplane_t *)( base + l->fileofs );
	numPlanes = l->filelen / sizeof( *inPlanes );

	l = &header.lumps[LUMP_BRUSHSIDES];
	inSides = (dbrushside_t *)( base + l->fileofs );
	numSides = l->filelen / sizeof( *inSides );

	l = &header.lumps[LUMP_BRUSHES];
	inBrushes = (dbrush_t *)( base + l->fileofs );
	numBrushes = l->filelen / sizeof( *inBrushes );

	for ( i = 0; i < numBrushes; i++ ) {
		vec3_t mins, maxs;
		vec3_t wmins, wmaxs;
		int firstSide = LittleLong( inBrushes[i].firstSide );
		int brushSides = LittleLong( inBrushes[i].numSides );

		if ( !Nav_BSP_BoundsFromBrush( inPlanes, numPlanes, inSides, numSides,
			firstSide, brushSides, mins, maxs ) ) {
			continue;
		}
		wmins[0] = mins[0] + worldOrigin[0];
		wmins[1] = mins[1] + worldOrigin[1];
		wmins[2] = mins[2] + worldOrigin[2];
		wmaxs[0] = maxs[0] + worldOrigin[0];
		wmaxs[1] = maxs[1] + worldOrigin[1];
		wmaxs[2] = maxs[2] + worldOrigin[2];
		Nav_BSP_AddBrushTopQuad( wmins, wmaxs );
	}

	FS_FreeFile( buf );

	Com_Printf( "Nav: extracted %d tris from %s\n", navTriCount, mapName );
	return navTriCount > 0 ? qtrue : qfalse;
}
