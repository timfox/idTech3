/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Recast/Detour navigation mesh implementation.
Uses Recast for navmesh generation and Detour for pathfinding/crowd.
===========================================================================
*/

#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>
#include <DetourCrowd.h>
#include <DetourCommon.h>
#include <DetourTileCache.h>
#include <DetourTileCacheBuilder.h>

extern "C" {
#include "nav_recast.h"
#include "nav_bsp_extract.h"
#include "qcommon.h"
}

#include <cstdio>
#include <cstdlib>

#define MAX_NAV_MESHES 4

struct NavMeshInstance {
	dtNavMesh       *navMesh;
	dtNavMeshQuery  *navQuery;
	dtCrowd         *crowd;
	dtTileCache     *tileCache;
	qboolean         active;
	int              agentCount;
};

static NavMeshInstance navMeshes[MAX_NAV_MESHES];
static int navMeshCount = 0;
static qboolean navInitialized = qfalse;
static navMeshHandle_t openWorldMeshHandle = -1;

#define NAV_OPEN_WORLD_TILES 256

typedef struct {
	qboolean  active;
	int       cellX;
	int       cellY;
	dtTileRef tileRef;
} navOpenWorldTile_t;

static navOpenWorldTile_t openWorldTiles[NAV_OPEN_WORLD_TILES];

static cvar_t *nav_enabled;
static cvar_t *nav_debugDraw;
static cvar_t *nav_cellSize;
static cvar_t *nav_agentRadius;
static cvar_t *nav_agentHeight;

#define VALID_MESH(h) ((h) >= 0 && (h) < navMeshCount && navMeshes[(h)].active)

static void Nav_DefaultParams(navMeshParams_t *p) {
	p->cellSize = 0.3f;
	p->cellHeight = 0.2f;
	p->agentHeight = 2.0f;
	p->agentRadius = 0.6f;
	p->agentMaxClimb = 0.9f;
	p->agentMaxSlope = 45.0f;
	p->regionMinSize = 8;
	p->regionMergeSize = 20;
	p->edgeMaxLen = 12.0f;
	p->edgeMaxError = 1.3f;
	p->vertsPerPoly = 6;
	p->detailSampleDist = 6.0f;
	p->detailSampleMaxError = 1.0f;
	p->tileSize = 48;
}

extern "C" void Nav_RegisterCvars(void) {
	nav_enabled     = Cvar_Get("nav_enabled",     "1",   CVAR_ARCHIVE);
	nav_debugDraw   = Cvar_Get("nav_debugDraw",   "0",   CVAR_ARCHIVE);
	nav_cellSize    = Cvar_Get("nav_cellSize",    "0.3", CVAR_ARCHIVE);
	nav_agentRadius = Cvar_Get("nav_agentRadius", "0.6", CVAR_ARCHIVE);
	nav_agentHeight = Cvar_Get("nav_agentHeight", "2.0", CVAR_ARCHIVE);
}

extern "C" void Nav_Init(void) {
	if (navInitialized) return;
	Nav_RegisterCvars();
	memset(navMeshes, 0, sizeof(navMeshes));
	navMeshCount = 0;
	navInitialized = qtrue;
	Com_Printf("Navigation system initialized (Recast/Detour)\n");
}

extern "C" void Nav_Shutdown(void) {
	if (!navInitialized) return;
	for (int i = 0; i < navMeshCount; i++) {
		if (navMeshes[i].active) Nav_DestroyMesh(i);
	}
	navMeshCount = 0;
	navInitialized = qfalse;
	Com_Printf("Navigation system shut down\n");
}

extern "C" void Nav_BSP_ClearGeometry(void);
extern "C" int  Nav_BSP_AddVertex(float x, float y, float z);
extern "C" void Nav_BSP_AddTriangle(int v0, int v1, int v2);
extern "C" float *Nav_BSP_GetVerts(void);
extern "C" int   *Nav_BSP_GetTris(void);
extern "C" int    Nav_BSP_GetVertCount(void);
extern "C" int    Nav_BSP_GetTriCount(void);
extern "C" qboolean Nav_BSP_ExtractFromSectorMap( int cellX, int cellY, float sectorSize );

static void Nav_AdaptRecastCellSize( float *cs, float *ch, const float bmin[3], const float bmax[3] ) {
	int width = 0;
	int height = 0;
	const int maxGrid = 512;

	if ( !cs || !ch || !bmin || !bmax ) {
		return;
	}

	rcCalcGridSize( bmin, bmax, *cs, &width, &height );
	while ( ( width > maxGrid || height > maxGrid ) && *cs < 64.0f ) {
		*cs *= 2.0f;
		*ch *= 2.0f;
		rcCalcGridSize( bmin, bmax, *cs, &width, &height );
	}
}

static qboolean Nav_BuildTileBlob( int tileX, int tileY, float tileWorldWidth, float tileWorldHeight,
	const navMeshParams_t *params, unsigned char **outData, int *outDataSize ) {
	navMeshParams_t p;
	int nverts, ntris;
	float *verts;
	int *tris;
	float bmin[3], bmax[3];
	rcContext ctx;
	rcConfig cfg;
	rcHeightfield *solid;
	unsigned char *triAreas;
	rcCompactHeightfield *chf;
	rcContourSet *cset;
	rcPolyMesh *pmesh;
	rcPolyMeshDetail *dmesh;
	dtNavMeshCreateParams dtParams;
	unsigned char *navData = nullptr;
	int navDataSize = 0;
	int i;

	if ( !outData || !outDataSize ) {
		return qfalse;
	}
	*outData = nullptr;
	*outDataSize = 0;

	if ( params ) {
		p = *params;
	} else {
		Nav_DefaultParams( &p );
	}

	nverts = Nav_BSP_GetVertCount();
	ntris = Nav_BSP_GetTriCount();
	verts = Nav_BSP_GetVerts();
	tris = Nav_BSP_GetTris();
	if ( nverts < 3 || ntris < 1 ) {
		return qfalse;
	}

	bmin[0] = bmin[1] = bmin[2] = 1e10f;
	bmax[0] = bmax[1] = bmax[2] = -1e10f;
	for ( i = 0; i < nverts; i++ ) {
		for ( int a = 0; a < 3; a++ ) {
			if ( verts[i * 3 + a] < bmin[a] ) {
				bmin[a] = verts[i * 3 + a];
			}
			if ( verts[i * 3 + a] > bmax[a] ) {
				bmax[a] = verts[i * 3 + a];
			}
		}
	}

	memset( &cfg, 0, sizeof( cfg ) );
	cfg.cs = p.cellSize;
	cfg.ch = p.cellHeight;
	rcVcopy( cfg.bmin, bmin );
	rcVcopy( cfg.bmax, bmax );
	Nav_AdaptRecastCellSize( &cfg.cs, &cfg.ch, cfg.bmin, cfg.bmax );
	cfg.walkableSlopeAngle = p.agentMaxSlope;
	cfg.walkableHeight = (int)ceilf( p.agentHeight / cfg.ch );
	cfg.walkableClimb = (int)floorf( p.agentMaxClimb / cfg.ch );
	cfg.walkableRadius = (int)ceilf( p.agentRadius / cfg.cs );
	cfg.maxEdgeLen = (int)( p.edgeMaxLen / cfg.cs );
	cfg.maxSimplificationError = p.edgeMaxError;
	cfg.minRegionArea = p.regionMinSize * p.regionMinSize;
	cfg.mergeRegionArea = p.regionMergeSize * p.regionMergeSize;
	cfg.maxVertsPerPoly = p.vertsPerPoly;
	cfg.detailSampleDist = p.detailSampleDist < 0.9f ? 0 : cfg.cs * p.detailSampleDist;
	cfg.detailSampleMaxError = cfg.ch * p.detailSampleMaxError;
	rcCalcGridSize( cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height );

	solid = rcAllocHeightfield();
	if ( !solid || !rcCreateHeightfield( &ctx, *solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch ) ) {
		rcFreeHeightField( solid );
		return qfalse;
	}

	triAreas = new unsigned char[ntris];
	memset( triAreas, 0, (size_t)ntris );
	rcMarkWalkableTriangles( &ctx, cfg.walkableSlopeAngle, verts, nverts, tris, ntris, triAreas );
	rcRasterizeTriangles( &ctx, verts, nverts, tris, triAreas, ntris, *solid, cfg.walkableClimb );
	delete[] triAreas;

	rcFilterLowHangingWalkableObstacles( &ctx, cfg.walkableClimb, *solid );
	rcFilterLedgeSpans( &ctx, cfg.walkableHeight, cfg.walkableClimb, *solid );
	rcFilterWalkableLowHeightSpans( &ctx, cfg.walkableHeight, *solid );

	chf = rcAllocCompactHeightfield();
	rcBuildCompactHeightfield( &ctx, cfg.walkableHeight, cfg.walkableClimb, *solid, *chf );
	rcFreeHeightField( solid );

	rcErodeWalkableArea( &ctx, cfg.walkableRadius, *chf );
	rcBuildDistanceField( &ctx, *chf );
	rcBuildRegions( &ctx, *chf, 0, cfg.minRegionArea, cfg.mergeRegionArea );

	cset = rcAllocContourSet();
	rcBuildContours( &ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cset );

	pmesh = rcAllocPolyMesh();
	rcBuildPolyMesh( &ctx, *cset, cfg.maxVertsPerPoly, *pmesh );

	dmesh = rcAllocPolyMeshDetail();
	rcBuildPolyMeshDetail( &ctx, *pmesh, *chf, cfg.detailSampleDist, cfg.detailSampleMaxError, *dmesh );
	rcFreeCompactHeightfield( chf );
	rcFreeContourSet( cset );

	if ( pmesh->nverts < 3 || pmesh->npolys < 1 ) {
		rcFreePolyMesh( pmesh );
		rcFreePolyMeshDetail( dmesh );
		return qfalse;
	}

	for ( i = 0; i < pmesh->npolys; i++ ) {
		if ( pmesh->areas[i] == RC_WALKABLE_AREA ) {
			pmesh->areas[i] = 0;
		}
		if ( pmesh->areas[i] == 0 ) {
			pmesh->flags[i] = 1;
		}
	}

	memset( &dtParams, 0, sizeof( dtParams ) );
	dtParams.verts = pmesh->verts;
	dtParams.vertCount = pmesh->nverts;
	dtParams.polys = pmesh->polys;
	dtParams.polyAreas = pmesh->areas;
	dtParams.polyFlags = pmesh->flags;
	dtParams.polyCount = pmesh->npolys;
	dtParams.nvp = pmesh->nvp;
	dtParams.detailMeshes = dmesh->meshes;
	dtParams.detailVerts = dmesh->verts;
	dtParams.detailVertsCount = dmesh->nverts;
	dtParams.detailTris = dmesh->tris;
	dtParams.detailTriCount = dmesh->ntris;
	dtParams.walkableHeight = p.agentHeight;
	dtParams.walkableRadius = p.agentRadius;
	dtParams.walkableClimb = p.agentMaxClimb;
	rcVcopy( dtParams.bmin, pmesh->bmin );
	rcVcopy( dtParams.bmax, pmesh->bmax );
	dtParams.cs = cfg.cs;
	dtParams.ch = cfg.ch;
	dtParams.buildBvTree = true;
	dtParams.tileX = tileX;
	dtParams.tileY = tileY;
	dtParams.tileLayer = 0;
	dtParams.bmin[0] = (float)tileX * tileWorldWidth;
	dtParams.bmax[0] = (float)( tileX + 1 ) * tileWorldWidth;
	dtParams.bmin[2] = -(float)( tileY + 1 ) * tileWorldHeight;
	dtParams.bmax[2] = -(float)tileY * tileWorldHeight;

	if ( !dtCreateNavMeshData( &dtParams, &navData, &navDataSize ) || !navData || navDataSize <= 0 ) {
		rcFreePolyMesh( pmesh );
		rcFreePolyMeshDetail( dmesh );
		return qfalse;
	}

	rcFreePolyMesh( pmesh );
	rcFreePolyMeshDetail( dmesh );

	*outData = navData;
	*outDataSize = navDataSize;
	return qtrue;
}

extern "C" navMeshHandle_t Nav_BuildFromBSP(const char *mapName, const navMeshParams_t *params) {
	if (!navInitialized || navMeshCount >= MAX_NAV_MESHES) return -1;

	navMeshParams_t p;
	if (params) { p = *params; } else { Nav_DefaultParams(&p); }

	int nverts = Nav_BSP_GetVertCount();
	int ntris = Nav_BSP_GetTriCount();
	float *verts = Nav_BSP_GetVerts();
	int *tris = Nav_BSP_GetTris();

	if (nverts == 0 || ntris == 0) {
		Com_Printf(S_COLOR_YELLOW "Nav: no BSP geometry extracted for %s, building empty mesh\n", mapName);
	}

	float bmin[3] = { 1e10f, 1e10f, 1e10f };
	float bmax[3] = { -1e10f, -1e10f, -1e10f };
	for (int v = 0; v < nverts; v++) {
		for (int a = 0; a < 3; a++) {
			if (verts[v*3+a] < bmin[a]) bmin[a] = verts[v*3+a];
			if (verts[v*3+a] > bmax[a]) bmax[a] = verts[v*3+a];
		}
	}

	rcContext ctx;
	rcConfig cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.cs = p.cellSize;
	cfg.ch = p.cellHeight;
	cfg.walkableSlopeAngle = p.agentMaxSlope;
	cfg.walkableHeight = (int)ceilf(p.agentHeight / cfg.ch);
	cfg.walkableClimb = (int)floorf(p.agentMaxClimb / cfg.ch);
	cfg.walkableRadius = (int)ceilf(p.agentRadius / cfg.cs);
	cfg.maxEdgeLen = (int)(p.edgeMaxLen / cfg.cs);
	cfg.maxSimplificationError = p.edgeMaxError;
	cfg.minRegionArea = p.regionMinSize * p.regionMinSize;
	cfg.mergeRegionArea = p.regionMergeSize * p.regionMergeSize;
	cfg.maxVertsPerPoly = p.vertsPerPoly;
	cfg.detailSampleDist = p.detailSampleDist < 0.9f ? 0 : cfg.cs * p.detailSampleDist;
	cfg.detailSampleMaxError = cfg.ch * p.detailSampleMaxError;
	rcVcopy(cfg.bmin, bmin);
	rcVcopy(cfg.bmax, bmax);
	rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);

	rcHeightfield *solid = rcAllocHeightfield();
	rcCreateHeightfield(&ctx, *solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch);

	unsigned char *triAreas = new unsigned char[ntris];
	memset(triAreas, 0, ntris);
	rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle, verts, nverts, tris, ntris, triAreas);
	rcRasterizeTriangles(&ctx, verts, nverts, tris, triAreas, ntris, *solid, cfg.walkableClimb);
	delete[] triAreas;

	rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *solid);
	rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid);
	rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *solid);

	rcCompactHeightfield *chf = rcAllocCompactHeightfield();
	rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid, *chf);
	rcFreeHeightField(solid);

	rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf);
	rcBuildDistanceField(&ctx, *chf);
	rcBuildRegions(&ctx, *chf, 0, cfg.minRegionArea, cfg.mergeRegionArea);

	rcContourSet *cset = rcAllocContourSet();
	rcBuildContours(&ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cset);

	rcPolyMesh *pmesh = rcAllocPolyMesh();
	rcBuildPolyMesh(&ctx, *cset, cfg.maxVertsPerPoly, *pmesh);

	rcPolyMeshDetail *dmesh = rcAllocPolyMeshDetail();
	rcBuildPolyMeshDetail(&ctx, *pmesh, *chf, cfg.detailSampleDist, cfg.detailSampleMaxError, *dmesh);
	rcFreeCompactHeightfield(chf);
	rcFreeContourSet(cset);

	unsigned char *navData = nullptr;
	int navDataSize = 0;

	for (int i = 0; i < pmesh->npolys; i++) {
		if (pmesh->areas[i] == RC_WALKABLE_AREA)
			pmesh->areas[i] = 0;
		if (pmesh->areas[i] == 0)
			pmesh->flags[i] = 1;
	}

	dtNavMeshCreateParams dtParams;
	memset(&dtParams, 0, sizeof(dtParams));
	dtParams.verts = pmesh->verts;
	dtParams.vertCount = pmesh->nverts;
	dtParams.polys = pmesh->polys;
	dtParams.polyAreas = pmesh->areas;
	dtParams.polyFlags = pmesh->flags;
	dtParams.polyCount = pmesh->npolys;
	dtParams.nvp = pmesh->nvp;
	dtParams.detailMeshes = dmesh->meshes;
	dtParams.detailVerts = dmesh->verts;
	dtParams.detailVertsCount = dmesh->nverts;
	dtParams.detailTris = dmesh->tris;
	dtParams.detailTriCount = dmesh->ntris;
	dtParams.walkableHeight = p.agentHeight;
	dtParams.walkableRadius = p.agentRadius;
	dtParams.walkableClimb = p.agentMaxClimb;
	rcVcopy(dtParams.bmin, pmesh->bmin);
	rcVcopy(dtParams.bmax, pmesh->bmax);
	dtParams.cs = cfg.cs;
	dtParams.ch = cfg.ch;
	dtParams.buildBvTree = true;

	dtCreateNavMeshData(&dtParams, &navData, &navDataSize);
	rcFreePolyMesh(pmesh);
	rcFreePolyMeshDetail(dmesh);

	if (!navData) {
		Com_Printf(S_COLOR_RED "Nav: failed to create Detour navmesh data for %s\n", mapName);
		return -1;
	}

	int idx = navMeshCount++;
	NavMeshInstance *inst = &navMeshes[idx];
	memset(inst, 0, sizeof(*inst));

	inst->navMesh = dtAllocNavMesh();
	dtStatus status = inst->navMesh->init(navData, navDataSize, DT_TILE_FREE_DATA);
	if (dtStatusFailed(status)) {
		Com_Printf(S_COLOR_RED "Nav: Detour init failed for %s\n", mapName);
		dtFree(navData);
		dtFreeNavMesh(inst->navMesh);
		navMeshCount--;
		return -1;
	}

	inst->navQuery = dtAllocNavMeshQuery();
	inst->navQuery->init(inst->navMesh, 2048);

	inst->crowd = dtAllocCrowd();
	inst->crowd->init(NAV_MAX_AGENTS, p.agentRadius * 4.0f, inst->navMesh);

	inst->active = qtrue;
	inst->agentCount = 0;

	Com_Printf("Nav: built navmesh for %s (%d verts, %d tris -> Detour %d bytes)\n",
		mapName, nverts, ntris, navDataSize);
	return idx;
}

extern "C" navMeshHandle_t Nav_LoadFromFile(const char *filename) {
	if (!navInitialized || navMeshCount >= MAX_NAV_MESHES) return -1;

	void *fileData;
	int fileSize = FS_ReadFile(filename, &fileData);
	if (fileSize <= 0 || !fileData) {
		Com_Printf(S_COLOR_RED "Nav: could not read %s\n", filename);
		return -1;
	}

	int idx = navMeshCount++;
	NavMeshInstance *inst = &navMeshes[idx];
	memset(inst, 0, sizeof(*inst));

	inst->navMesh = dtAllocNavMesh();
	dtNavMeshParams meshParams;
	memset(&meshParams, 0, sizeof(meshParams));
	meshParams.tileWidth = 48 * 0.3f;
	meshParams.tileHeight = 48 * 0.3f;
	meshParams.maxTiles = 1024;
	meshParams.maxPolys = 1024 * 64;
	inst->navMesh->init(&meshParams);

	inst->navQuery = dtAllocNavMeshQuery();
	inst->navQuery->init(inst->navMesh, 2048);

	inst->crowd = dtAllocCrowd();
	inst->crowd->init(NAV_MAX_AGENTS, 2.4f, inst->navMesh);

	inst->active = qtrue;
	FS_FreeFile(fileData);

	Com_Printf("Nav: loaded navmesh from %s\n", filename);
	return idx;
}

extern "C" qboolean Nav_SaveToFile(navMeshHandle_t handle, const char *filename) {
	if (!VALID_MESH(handle)) return qfalse;
	NavMeshInstance *inst = &navMeshes[handle];
	if (!inst->navMesh) return qfalse;

	const dtNavMesh *nm = inst->navMesh;
	const dtNavMeshParams *params = nm->getParams();
	int maxTiles = params->maxTiles;

	int headerSize = (int)(sizeof(int) + sizeof(dtNavMeshParams));
	byte *buf = (byte *)malloc(headerSize);
	if (!buf) return qfalse;

	byte *p = buf;
	*(int *)p = maxTiles; p += sizeof(int);
	memcpy(p, params, sizeof(dtNavMeshParams));

	FS_WriteFile(filename, buf, headerSize);
	free(buf);

	Com_Printf("Nav: saved navmesh params to %s (maxTiles %d)\n", filename, maxTiles);
	return qtrue;
}

extern "C" void Nav_DestroyMesh(navMeshHandle_t handle) {
	if (!VALID_MESH(handle)) return;
	NavMeshInstance *inst = &navMeshes[handle];
	if (inst->crowd) dtFreeCrowd(inst->crowd);
	if (inst->navQuery) dtFreeNavMeshQuery(inst->navQuery);
	if (inst->navMesh) dtFreeNavMesh(inst->navMesh);
	if (inst->tileCache) dtFreeTileCache(inst->tileCache);
	memset(inst, 0, sizeof(*inst));
}

extern "C" qboolean Nav_FindPath(navMeshHandle_t mesh, const vec3_t start, const vec3_t end, navPath_t *path) {
	if (!VALID_MESH(mesh) || !path) return qfalse;
	NavMeshInstance *inst = &navMeshes[mesh];

	memset(path, 0, sizeof(*path));

	float spos[3] = { start[0], start[2], -start[1] };
	float epos[3] = { end[0], end[2], -end[1] };
	float extents[3] = { 2.0f, 4.0f, 2.0f };
	dtQueryFilter filter;
	filter.setIncludeFlags(0xFFFF);
	filter.setExcludeFlags(0);

	dtPolyRef startRef, endRef;
	float nearStart[3], nearEnd[3];
	inst->navQuery->findNearestPoly(spos, extents, &filter, &startRef, nearStart);
	inst->navQuery->findNearestPoly(epos, extents, &filter, &endRef, nearEnd);

	if (!startRef || !endRef) return qfalse;

	dtPolyRef polys[NAV_MAX_PATH_NODES];
	int numPolys = 0;
	inst->navQuery->findPath(startRef, endRef, nearStart, nearEnd, &filter, polys, &numPolys, NAV_MAX_PATH_NODES);

	if (numPolys == 0) return qfalse;

	float straightPath[NAV_MAX_PATH_NODES * 3];
	unsigned char straightPathFlags[NAV_MAX_PATH_NODES];
	dtPolyRef straightPathPolys[NAV_MAX_PATH_NODES];
	int numStraight = 0;
	inst->navQuery->findStraightPath(nearStart, nearEnd, polys, numPolys,
		straightPath, straightPathFlags, straightPathPolys, &numStraight, NAV_MAX_PATH_NODES);

	path->numPoints = numStraight;
	for (int i = 0; i < numStraight; i++) {
		path->points[i][0] = straightPath[i * 3 + 0];
		path->points[i][1] = -straightPath[i * 3 + 2];
		path->points[i][2] = straightPath[i * 3 + 1];
	}
	path->valid = qtrue;
	return qtrue;
}

extern "C" qboolean Nav_FindNearestPoint(navMeshHandle_t mesh, const vec3_t pos, vec3_t nearest, float range) {
	if (!VALID_MESH(mesh)) return qfalse;
	float rpos[3] = { pos[0], pos[2], -pos[1] };
	float ext[3] = { range, range, range };
	dtQueryFilter filter; filter.setIncludeFlags(0xFFFF);
	dtPolyRef ref; float near[3];
	navMeshes[mesh].navQuery->findNearestPoly(rpos, ext, &filter, &ref, near);
	if (!ref) return qfalse;
	nearest[0] = near[0]; nearest[1] = -near[2]; nearest[2] = near[1];
	return qtrue;
}

extern "C" qboolean Nav_Raycast(navMeshHandle_t mesh, const vec3_t start, const vec3_t end, vec3_t hitPos, float *hitDist) {
	if (!VALID_MESH(mesh)) return qfalse;
	float s[3] = { start[0], start[2], -start[1] };
	float e[3] = { end[0], end[2], -end[1] };
	float ext[3] = { 2, 4, 2 };
	dtQueryFilter filter; filter.setIncludeFlags(0xFFFF);
	dtPolyRef startRef; float ns[3];
	navMeshes[mesh].navQuery->findNearestPoly(s, ext, &filter, &startRef, ns);
	if (!startRef) return qfalse;
	float t = 0; float hitNormal[3]; dtPolyRef path[64]; int pathCount;
	navMeshes[mesh].navQuery->raycast(startRef, ns, e, &filter, &t, hitNormal, path, &pathCount, 64);
	if (t >= 1.0f) return qfalse;
	if (hitPos) { hitPos[0] = s[0]+(e[0]-s[0])*t; hitPos[1] = -(s[2]+(e[2]-s[2])*t); hitPos[2] = s[1]+(e[1]-s[1])*t; }
	if (hitDist) *hitDist = t;
	return qtrue;
}

extern "C" navAgentHandle_t Nav_AddAgent(navMeshHandle_t mesh, const vec3_t pos, const navAgentParams_t *params) {
	if (!VALID_MESH(mesh) || !params) return -1;
	NavMeshInstance *inst = &navMeshes[mesh];
	dtCrowdAgentParams ap;
	memset(&ap, 0, sizeof(ap));
	ap.radius = params->radius;
	ap.height = params->height;
	ap.maxAcceleration = params->maxAcceleration;
	ap.maxSpeed = params->maxSpeed;
	ap.collisionQueryRange = params->collisionQueryRange > 0 ? params->collisionQueryRange : params->radius * 12.0f;
	ap.pathOptimizationRange = params->pathOptimizationRange > 0 ? params->pathOptimizationRange : params->radius * 30.0f;
	ap.separationWeight = params->separationWeight > 0 ? params->separationWeight : 2.0f;
	ap.obstacleAvoidanceType = params->obstacleAvoidanceType;
	ap.updateFlags = DT_CROWD_ANTICIPATE_TURNS | DT_CROWD_OPTIMIZE_VIS | DT_CROWD_OPTIMIZE_TOPO | DT_CROWD_OBSTACLE_AVOIDANCE;

	float rpos[3] = { pos[0], pos[2], -pos[1] };
	int idx = inst->crowd->addAgent(rpos, &ap);
	if (idx >= 0) inst->agentCount++;
	return idx;
}

extern "C" void Nav_RemoveAgent(navAgentHandle_t agent) {
	for (int m = 0; m < navMeshCount; m++) {
		if (!navMeshes[m].active || !navMeshes[m].crowd) continue;
		const dtCrowdAgent *a = navMeshes[m].crowd->getAgent(agent);
		if (a && a->active) {
			navMeshes[m].crowd->removeAgent(agent);
			navMeshes[m].agentCount--;
			return;
		}
	}
}

extern "C" void Nav_SetAgentTarget(navAgentHandle_t agent, const vec3_t target) {
	float tgt[3] = { target[0], target[2], -target[1] };
	float ext[3] = { 2, 4, 2 };
	dtQueryFilter filter; filter.setIncludeFlags(0xFFFF);
	for (int m = 0; m < navMeshCount; m++) {
		if (!navMeshes[m].active || !navMeshes[m].crowd) continue;
		const dtCrowdAgent *a = navMeshes[m].crowd->getAgent(agent);
		if (a && a->active) {
			dtPolyRef ref; float near[3];
			navMeshes[m].navQuery->findNearestPoly(tgt, ext, &filter, &ref, near);
			if (ref) navMeshes[m].crowd->requestMoveTarget(agent, ref, near);
			return;
		}
	}
}

extern "C" void Nav_GetAgentState(navAgentHandle_t agent, navAgentState_t *state) {
	if (!state) return;
	memset(state, 0, sizeof(*state));
	for (int m = 0; m < navMeshCount; m++) {
		if (!navMeshes[m].active || !navMeshes[m].crowd) continue;
		const dtCrowdAgent *a = navMeshes[m].crowd->getAgent(agent);
		if (a && a->active) {
			state->position[0] = a->npos[0]; state->position[1] = -a->npos[2]; state->position[2] = a->npos[1];
			state->velocity[0] = a->vel[0]; state->velocity[1] = -a->vel[2]; state->velocity[2] = a->vel[1];
			state->target[0] = a->targetPos[0]; state->target[1] = -a->targetPos[2]; state->target[2] = a->targetPos[1];
			state->active = qtrue;
			float dx = a->npos[0]-a->targetPos[0], dz = a->npos[2]-a->targetPos[2];
			state->reachedTarget = (dx*dx+dz*dz < a->params.radius*a->params.radius) ? qtrue : qfalse;
			return;
		}
	}
}

extern "C" void Nav_UpdateCrowd(navMeshHandle_t mesh, float dt) {
	if (!VALID_MESH(mesh) || !navMeshes[mesh].crowd) return;
	navMeshes[mesh].crowd->update(dt, nullptr);
}

extern "C" int Nav_AddObstacle(navMeshHandle_t mesh, const vec3_t pos, float radius, float height) {
	if (!VALID_MESH(mesh)) return -1;
	NavMeshInstance *inst = &navMeshes[mesh];
	if (!inst->tileCache) {
		Com_DPrintf("Nav_AddObstacle: no tile cache for mesh %d\n", mesh);
		return -1;
	}

	float dpos[3] = { pos[0], pos[2], -pos[1] };
	dtObstacleRef ref = 0;
	dtStatus status = inst->tileCache->addObstacle(dpos, radius, height, &ref);
	if (dtStatusFailed(status)) {
		Com_DPrintf("Nav_AddObstacle: failed (status 0x%x)\n", status);
		return -1;
	}
	return (int)ref;
}

extern "C" void Nav_RemoveObstacle(navMeshHandle_t mesh, int obstacleId) {
	if (!VALID_MESH(mesh) || obstacleId <= 0) return;
	NavMeshInstance *inst = &navMeshes[mesh];
	if (!inst->tileCache) return;
	inst->tileCache->removeObstacle((dtObstacleRef)obstacleId);
}

extern "C" void Nav_UpdateObstacles(navMeshHandle_t mesh) {
	if (!VALID_MESH(mesh)) return;
	NavMeshInstance *inst = &navMeshes[mesh];
	if (!inst->tileCache) return;
	bool upToDate = false;
	inst->tileCache->update(0.0f, inst->navMesh, &upToDate);
}

extern "C" int Nav_GetAgentCount(navMeshHandle_t mesh) {
	return VALID_MESH(mesh) ? navMeshes[mesh].agentCount : 0;
}

extern "C" int Nav_GetPolyCount(navMeshHandle_t mesh) {
	if (!VALID_MESH(mesh) || !navMeshes[mesh].navMesh) return 0;
	return navMeshes[mesh].navMesh->getMaxTiles() * 64;
}

extern "C" void Nav_DebugDraw(navMeshHandle_t mesh) {
	if (!VALID_MESH(mesh)) return;
	NavMeshInstance *inst = &navMeshes[mesh];
	if (!inst->navMesh) return;

	Com_Printf("Nav debug: mesh %d, max tiles %d, %d agents\n",
		mesh, inst->navMesh->getMaxTiles(), inst->agentCount);
}

static int Nav_FindOpenWorldTile( int cellX, int cellY ) {
	for ( int i = 0; i < NAV_OPEN_WORLD_TILES; i++ ) {
		if ( openWorldTiles[i].active && openWorldTiles[i].cellX == cellX &&
			openWorldTiles[i].cellY == cellY ) {
			return i;
		}
	}
	return -1;
}

static int Nav_AllocOpenWorldTile( int cellX, int cellY ) {
	int idx = Nav_FindOpenWorldTile( cellX, cellY );
	if ( idx >= 0 ) {
		return idx;
	}
	for ( int i = 0; i < NAV_OPEN_WORLD_TILES; i++ ) {
		if ( !openWorldTiles[i].active ) {
			openWorldTiles[i].active = qtrue;
			openWorldTiles[i].cellX = cellX;
			openWorldTiles[i].cellY = cellY;
			openWorldTiles[i].tileRef = 0;
			return i;
		}
	}
	return -1;
}

extern "C" navMeshHandle_t Nav_CreateOpenWorldMesh( void ) {
	navMeshParams_t p;

	if ( openWorldMeshHandle >= 0 && VALID_MESH( openWorldMeshHandle ) ) {
		return openWorldMeshHandle;
	}
	if ( !navInitialized || navMeshCount >= MAX_NAV_MESHES ) {
		return -1;
	}

	Nav_DefaultParams( &p );

	int idx = navMeshCount++;
	NavMeshInstance *inst = &navMeshes[idx];
	memset( inst, 0, sizeof( *inst ) );

	inst->navMesh = dtAllocNavMesh();
	dtNavMeshParams meshParams;
	float sectorTile;
	memset( &meshParams, 0, sizeof( meshParams ) );
	meshParams.orig[0] = 0.0f;
	meshParams.orig[1] = 0.0f;
	meshParams.orig[2] = 0.0f;
	sectorTile = Cvar_VariableValue( "r_openWorldSectorSize" );
	if ( sectorTile < 256.0f ) {
		sectorTile = 4096.0f;
	}
	meshParams.tileWidth = sectorTile;
	meshParams.tileHeight = sectorTile;
	meshParams.maxTiles = 1024;
	meshParams.maxPolys = 1024 * 64;
	if ( dtStatusFailed( inst->navMesh->init( &meshParams ) ) ) {
		dtFreeNavMesh( inst->navMesh );
		memset( inst, 0, sizeof( *inst ) );
		navMeshCount--;
		return -1;
	}

	inst->navQuery = dtAllocNavMeshQuery();
	inst->navQuery->init( inst->navMesh, 2048 );
	inst->crowd = dtAllocCrowd();
	inst->crowd->init( NAV_MAX_AGENTS, p.agentRadius * 4.0f, inst->navMesh );
	inst->active = qtrue;
	inst->agentCount = 0;

	memset( openWorldTiles, 0, sizeof( openWorldTiles ) );
	openWorldMeshHandle = idx;
	Com_Printf( "Nav: open-world tiled mesh handle %d (tile %.1fx%.1f)\n",
		idx, meshParams.tileWidth, meshParams.tileHeight );
	return idx;
}

extern "C" navMeshHandle_t Nav_GetOpenWorldMesh( void ) {
	return openWorldMeshHandle;
}

extern "C" qboolean Nav_LoadSectorTile( navMeshHandle_t mesh, int cellX, int cellY ) {
	char path[MAX_QPATH];
	void *fileData;
	int fileSize;
	dtTileRef tileRef;
	dtStatus status;
	int slot;

	if ( !VALID_MESH( mesh ) ) {
		return qfalse;
	}
	if ( Nav_FindOpenWorldTile( cellX, cellY ) >= 0 ) {
		return qtrue;
	}

	Com_sprintf( path, sizeof( path ), "nav/sector_%d_%d.nav", cellX, cellY );
	fileSize = FS_ReadFile( path, &fileData );
	if ( fileSize <= 0 || !fileData ) {
		Com_DPrintf( "Nav: no tile %s\n", path );
		return qfalse;
	}

	slot = Nav_AllocOpenWorldTile( cellX, cellY );
	if ( slot < 0 ) {
		FS_FreeFile( fileData );
		Com_Printf( S_COLOR_YELLOW "Nav: open-world tile table full (%d,%d)\n", cellX, cellY );
		return qfalse;
	}

	status = navMeshes[mesh].navMesh->addTile(
		(unsigned char *)fileData, fileSize, DT_TILE_FREE_DATA, 0, &tileRef );
	if ( dtStatusFailed( status ) ) {
		FS_FreeFile( fileData );
		openWorldTiles[slot].active = qfalse;
		Com_Printf( S_COLOR_YELLOW "Nav: addTile failed for %s (0x%x)\n", path, status );
		return qfalse;
	}

	openWorldTiles[slot].tileRef = tileRef;
	Com_Printf( "Nav: loaded sector tile %d,%d from %s\n", cellX, cellY, path );
	return qtrue;
}

extern "C" void Nav_UnloadSectorTile( navMeshHandle_t mesh, int cellX, int cellY ) {
	int slot;
	unsigned char *data = NULL;
	int dataSize = 0;

	if ( !VALID_MESH( mesh ) ) {
		return;
	}
	slot = Nav_FindOpenWorldTile( cellX, cellY );
	if ( slot < 0 ) {
		return;
	}

	if ( openWorldTiles[slot].tileRef ) {
		navMeshes[mesh].navMesh->removeTile( openWorldTiles[slot].tileRef, &data, &dataSize );
		if ( data ) {
			dtFree( data );
		}
	}
	openWorldTiles[slot].active = qfalse;
	openWorldTiles[slot].tileRef = 0;
	Com_DPrintf( "Nav: unloaded sector tile %d,%d\n", cellX, cellY );
}

extern "C" qboolean Nav_BakeSectorTile( int cellX, int cellY, float sectorSize, const navMeshParams_t *params ) {
	char path[MAX_QPATH];
	unsigned char *tileData;
	int tileSize;
	qboolean ok;

	if ( !navInitialized ) {
		return qfalse;
	}
	if ( sectorSize < 256.0f ) {
		sectorSize = Cvar_VariableValue( "r_openWorldSectorSize" );
	}
	if ( sectorSize < 256.0f ) {
		sectorSize = 4096.0f;
	}

	Nav_BSP_ClearGeometry();
	if ( !Nav_BSP_ExtractFromSectorMap( cellX, cellY, sectorSize ) ) {
		Com_Printf( S_COLOR_YELLOW "Nav: bake %d,%d failed — no geometry (load maps/sector_%d_%d.bsp)\n",
			cellX, cellY, cellX, cellY );
		return qfalse;
	}

	ok = Nav_BuildTileBlob( cellX, cellY, sectorSize, sectorSize, params, &tileData, &tileSize );
	if ( !ok || !tileData || tileSize <= 0 ) {
		Com_Printf( S_COLOR_YELLOW "Nav: bake %d,%d Recast build failed\n", cellX, cellY );
		return qfalse;
	}

	Com_sprintf( path, sizeof( path ), "nav/sector_%d_%d.nav", cellX, cellY );
	FS_WriteFile( path, tileData, tileSize );
	dtFree( tileData );

	Com_Printf( "Nav: baked sector tile %d,%d -> %s (%d bytes)\n", cellX, cellY, path, tileSize );
	return qtrue;
}

extern "C" qboolean Nav_BakeSectorTileToPath( const char *bspPath, const char *navOutPath,
	int cellX, int cellY, float sectorSize, const navMeshParams_t *params ) {
	unsigned char *tileData;
	int tileSize;
	qboolean ok;
	FILE *f;
	byte *buf;
	long length;
	navMeshParams_t defaults;

	if ( !bspPath || !bspPath[0] || !navOutPath || !navOutPath[0] ) {
		return qfalse;
	}
	if ( !navInitialized ) {
		Nav_Init();
	}
	if ( sectorSize < 256.0f ) {
		sectorSize = 4096.0f;
	}

	f = fopen( bspPath, "rb" );
	if ( !f ) {
		return qfalse;
	}
	if ( fseek( f, 0, SEEK_END ) != 0 ) {
		fclose( f );
		return qfalse;
	}
	length = ftell( f );
	if ( length <= (long)sizeof( dheader_t ) ) {
		fclose( f );
		return qfalse;
	}
	if ( fseek( f, 0, SEEK_SET ) != 0 ) {
		fclose( f );
		return qfalse;
	}
	buf = (byte *)malloc( (size_t)length );
	if ( !buf ) {
		fclose( f );
		return qfalse;
	}
	if ( fread( buf, 1, (size_t)length, f ) != (size_t)length ) {
		free( buf );
		fclose( f );
		return qfalse;
	}
	fclose( f );

	if ( !Nav_BSP_ExtractFromSectorBuffer( buf, (size_t)length, cellX, cellY, sectorSize ) ) {
		free( buf );
		return qfalse;
	}
	free( buf );

	if ( !params ) {
		Nav_DefaultParams( &defaults );
		params = &defaults;
	}

	ok = Nav_BuildTileBlob( cellX, cellY, sectorSize, sectorSize, params, &tileData, &tileSize );
	if ( !ok || !tileData || tileSize <= 0 ) {
		return qfalse;
	}

	f = fopen( navOutPath, "wb" );
	if ( !f ) {
		dtFree( tileData );
		return qfalse;
	}
	if ( fwrite( tileData, 1, (size_t)tileSize, f ) != (size_t)tileSize ) {
		fclose( f );
		dtFree( tileData );
		return qfalse;
	}
	fclose( f );
	dtFree( tileData );
	Com_Printf( "Nav: baked %s -> %s (%d bytes)\n", bspPath, navOutPath, tileSize );
	return qtrue;
}

extern "C" qboolean Nav_ValidateTileFileAtPoint( const char *navPath, const vec3_t worldPos, float maxHorizDist ) {
	FILE *f;
	long fileSize;
	unsigned char *data;
	navMeshHandle_t mesh;
	dtStatus status;
	dtTileRef tileRef;
	dtPolyRef polyRef;
	float spos[3];
	float nearPt[3];
	float ext[3];
	dtQueryFilter filter;
	NavMeshInstance *inst;

	if ( !navPath || !worldPos || maxHorizDist <= 0.0f ) {
		return qfalse;
	}
	if ( !navInitialized ) {
		Nav_Init();
	}
	openWorldMeshHandle = -1;
	mesh = Nav_CreateOpenWorldMesh();
	if ( mesh < 0 ) {
		return qfalse;
	}
	inst = &navMeshes[mesh];
	if ( !inst->navMesh || !inst->navQuery ) {
		return qfalse;
	}

	f = fopen( navPath, "rb" );
	if ( !f ) {
		return qfalse;
	}
	if ( fseek( f, 0, SEEK_END ) != 0 ) {
		fclose( f );
		return qfalse;
	}
	fileSize = ftell( f );
	if ( fileSize <= 64 ) {
		fclose( f );
		return qfalse;
	}
	rewind( f );
	data = (unsigned char *)dtAlloc( (size_t)fileSize, DT_ALLOC_PERM );
	if ( !data ) {
		fclose( f );
		return qfalse;
	}
	if ( fread( data, 1, (size_t)fileSize, f ) != (size_t)fileSize ) {
		fclose( f );
		dtFree( data );
		return qfalse;
	}
	fclose( f );

	status = inst->navMesh->addTile( data, (int)fileSize, DT_TILE_FREE_DATA, 0, &tileRef );
	if ( dtStatusFailed( status ) ) {
		dtFree( data );
		return qfalse;
	}

	spos[0] = worldPos[0];
	spos[2] = -worldPos[1];
	ext[0] = maxHorizDist;
	ext[1] = 96.0f;
	ext[2] = maxHorizDist;
	filter.setIncludeFlags( 0xFFFF );
	filter.setExcludeFlags( 0 );

	for ( float probeZ = 32.0f; probeZ <= 256.0f; probeZ += 16.0f ) {
		spos[1] = probeZ;
		inst->navQuery->findNearestPoly( spos, ext, &filter, &polyRef, nearPt );
		if ( polyRef ) {
			return qtrue;
		}
	}

	{
		vec3_t start, end;
		navPath_t path;

		VectorCopy( worldPos, start );
		VectorCopy( worldPos, end );
		end[0] += 256.0f;
		if ( Nav_FindPath( mesh, start, end, &path ) && path.valid && path.numPoints > 0 ) {
			return qtrue;
		}
	}

	return qfalse;
}
