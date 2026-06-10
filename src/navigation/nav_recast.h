/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Navigation mesh system using Recast/Detour (zlib license).
Provides navmesh generation from BSP geometry, pathfinding,
crowd simulation, and dynamic obstacle support.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

#define NAV_MAX_PATH_NODES   256
#define NAV_MAX_AGENTS       128
#define NAV_MAX_OBSTACLES    256

typedef int navMeshHandle_t;
typedef int navAgentHandle_t;

typedef struct navMeshParams_s {
	float cellSize;
	float cellHeight;
	float agentHeight;
	float agentRadius;
	float agentMaxClimb;
	float agentMaxSlope;
	int   regionMinSize;
	int   regionMergeSize;
	float edgeMaxLen;
	float edgeMaxError;
	int   vertsPerPoly;
	float detailSampleDist;
	float detailSampleMaxError;
	int   tileSize;
} navMeshParams_t;

typedef struct navPath_s {
	vec3_t  points[NAV_MAX_PATH_NODES];
	int     numPoints;
	qboolean valid;
} navPath_t;

typedef struct navAgentParams_s {
	float radius;
	float height;
	float maxAcceleration;
	float maxSpeed;
	float collisionQueryRange;
	float pathOptimizationRange;
	float separationWeight;
	int   obstacleAvoidanceType;
} navAgentParams_t;

typedef struct navAgentState_s {
	vec3_t      position;
	vec3_t      velocity;
	vec3_t      target;
	qboolean    active;
	qboolean    reachedTarget;
} navAgentState_t;

void        Nav_Init(void);
void        Nav_Shutdown(void);
void        Nav_RegisterCvars(void);

navMeshHandle_t Nav_BuildFromBSP(const char *mapName, const navMeshParams_t *params);
navMeshHandle_t Nav_LoadFromFile(const char *filename);
qboolean        Nav_SaveToFile(navMeshHandle_t handle, const char *filename);
void            Nav_DestroyMesh(navMeshHandle_t handle);

qboolean    Nav_FindPath(navMeshHandle_t mesh, const vec3_t start, const vec3_t end, navPath_t *path);
qboolean    Nav_FindNearestPoint(navMeshHandle_t mesh, const vec3_t pos, vec3_t nearest, float range);
qboolean    Nav_Raycast(navMeshHandle_t mesh, const vec3_t start, const vec3_t end, vec3_t hitPos, float *hitDist);

navAgentHandle_t Nav_AddAgent(navMeshHandle_t mesh, const vec3_t pos, const navAgentParams_t *params);
void             Nav_RemoveAgent(navAgentHandle_t agent);
void             Nav_SetAgentTarget(navAgentHandle_t agent, const vec3_t target);
void             Nav_GetAgentState(navAgentHandle_t agent, navAgentState_t *state);
void             Nav_UpdateCrowd(navMeshHandle_t mesh, float dt);

int         Nav_AddObstacle(navMeshHandle_t mesh, const vec3_t pos, float radius, float height);
void        Nav_RemoveObstacle(navMeshHandle_t mesh, int obstacleId);
void        Nav_UpdateObstacles(navMeshHandle_t mesh);

int         Nav_GetAgentCount(navMeshHandle_t mesh);
int         Nav_GetPolyCount(navMeshHandle_t mesh);

void        Nav_DebugDraw(navMeshHandle_t mesh);

navMeshHandle_t Nav_CreateOpenWorldMesh( void );
navMeshHandle_t Nav_GetOpenWorldMesh( void );
qboolean        Nav_LoadSectorTile( navMeshHandle_t mesh, int cellX, int cellY );
void            Nav_UnloadSectorTile( navMeshHandle_t mesh, int cellX, int cellY );
qboolean        Nav_BakeSectorTile( int cellX, int cellY, float sectorSize, const navMeshParams_t *params );
qboolean        Nav_BakeSectorTileToPath( const char *bspPath, const char *navOutPath,
	int cellX, int cellY, float sectorSize, const navMeshParams_t *params );
qboolean        Nav_ValidateTileFileAtPoint( const char *navPath, const vec3_t worldPos, float maxHorizDist );

#ifdef __cplusplus
}
#endif
