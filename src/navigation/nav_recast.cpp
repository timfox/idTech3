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
#include "../qcommon/qcommon.h"
}

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

extern "C" navMeshHandle_t Nav_BuildFromBSP(const char *mapName, const navMeshParams_t *params) {
	if (!navInitialized || navMeshCount >= MAX_NAV_MESHES) return -1;

	navMeshParams_t p;
	if (params) { p = *params; } else { Nav_DefaultParams(&p); }

	int idx = navMeshCount++;
	NavMeshInstance *inst = &navMeshes[idx];
	memset(inst, 0, sizeof(*inst));

	inst->navMesh = dtAllocNavMesh();
	if (!inst->navMesh) {
		Com_Printf(S_COLOR_RED "Nav: failed to allocate navmesh for %s\n", mapName);
		navMeshCount--;
		return -1;
	}

	dtNavMeshParams meshParams;
	memset(&meshParams, 0, sizeof(meshParams));
	meshParams.tileWidth = p.tileSize * p.cellSize;
	meshParams.tileHeight = p.tileSize * p.cellSize;
	meshParams.maxTiles = 1024;
	meshParams.maxPolys = 1024 * 64;
	dtStatus status = inst->navMesh->init(&meshParams);
	if (dtStatusFailed(status)) {
		Com_Printf(S_COLOR_RED "Nav: failed to init navmesh for %s\n", mapName);
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

	Com_Printf("Nav: built navmesh for %s (cellSize=%.2f, agentR=%.2f, agentH=%.2f)\n",
		mapName, (double)p.cellSize, (double)p.agentRadius, (double)p.agentHeight);
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
	Com_Printf("Nav: save to %s (not yet implemented)\n", filename);
	return qfalse;
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
	(void)mesh; (void)pos; (void)radius; (void)height;
	return -1;
}

extern "C" void Nav_RemoveObstacle(navMeshHandle_t mesh, int obstacleId) {
	(void)mesh; (void)obstacleId;
}

extern "C" void Nav_UpdateObstacles(navMeshHandle_t mesh) {
	(void)mesh;
}

extern "C" int Nav_GetAgentCount(navMeshHandle_t mesh) {
	return VALID_MESH(mesh) ? navMeshes[mesh].agentCount : 0;
}

extern "C" int Nav_GetPolyCount(navMeshHandle_t mesh) {
	if (!VALID_MESH(mesh) || !navMeshes[mesh].navMesh) return 0;
	return navMeshes[mesh].navMesh->getMaxTiles() * 64;
}

extern "C" void Nav_DebugDraw(navMeshHandle_t mesh) {
	(void)mesh;
}
