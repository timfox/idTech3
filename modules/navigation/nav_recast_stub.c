/*
===========================================================================
Navigation no-op stub when USE_RECAST_NAV=OFF (core profile).
===========================================================================
*/

#include "nav_recast.h"
#include "nav_bsp_extract.h"
#include <string.h>

void Nav_Init( void ) {}
void Nav_Shutdown( void ) {}
void Nav_RegisterCvars( void ) {}

navMeshHandle_t Nav_BuildFromBSP( const char *mapName, const navMeshParams_t *params ) {
	(void)mapName;
	(void)params;
	return -1;
}

navMeshHandle_t Nav_LoadFromFile( const char *filename ) {
	(void)filename;
	return -1;
}

qboolean Nav_SaveToFile( navMeshHandle_t handle, const char *filename ) {
	(void)handle;
	(void)filename;
	return qfalse;
}

void Nav_DestroyMesh( navMeshHandle_t handle ) {
	(void)handle;
}

qboolean Nav_FindPath( navMeshHandle_t mesh, const vec3_t start, const vec3_t end, navPath_t *path ) {
	(void)mesh;
	(void)start;
	(void)end;
	(void)path;
	return qfalse;
}

qboolean Nav_FindNearestPoint( navMeshHandle_t mesh, const vec3_t pos, vec3_t nearest, float range ) {
	(void)mesh;
	(void)pos;
	(void)nearest;
	(void)range;
	return qfalse;
}

qboolean Nav_Raycast( navMeshHandle_t mesh, const vec3_t start, const vec3_t end, vec3_t hitPos, float *hitDist ) {
	(void)mesh;
	(void)start;
	(void)end;
	(void)hitPos;
	(void)hitDist;
	return qfalse;
}

navAgentHandle_t Nav_AddAgent( navMeshHandle_t mesh, const vec3_t pos, const navAgentParams_t *params ) {
	(void)mesh;
	(void)pos;
	(void)params;
	return -1;
}

void Nav_RemoveAgent( navAgentHandle_t agent ) {
	(void)agent;
}

void Nav_SetAgentTarget( navAgentHandle_t agent, const vec3_t target ) {
	(void)agent;
	(void)target;
}

void Nav_GetAgentState( navAgentHandle_t agent, navAgentState_t *state ) {
	(void)agent;
	if ( state ) {
		memset( state, 0, sizeof( *state ) );
	}
}

void Nav_UpdateCrowd( navMeshHandle_t mesh, float dt ) {
	(void)mesh;
	(void)dt;
}

int Nav_AddObstacle( navMeshHandle_t mesh, const vec3_t pos, float radius, float height ) {
	(void)mesh;
	(void)pos;
	(void)radius;
	(void)height;
	return -1;
}

void Nav_RemoveObstacle( navMeshHandle_t mesh, int obstacleId ) {
	(void)mesh;
	(void)obstacleId;
}

void Nav_UpdateObstacles( navMeshHandle_t mesh ) {
	(void)mesh;
}

int Nav_GetAgentCount( navMeshHandle_t mesh ) {
	(void)mesh;
	return 0;
}

int Nav_GetPolyCount( navMeshHandle_t mesh ) {
	(void)mesh;
	return 0;
}

void Nav_DebugDraw( navMeshHandle_t mesh ) {
	(void)mesh;
}

navMeshHandle_t Nav_CreateOpenWorldMesh( void ) {
	return -1;
}

navMeshHandle_t Nav_GetOpenWorldMesh( void ) {
	return -1;
}

qboolean Nav_LoadSectorTile( navMeshHandle_t mesh, int cellX, int cellY ) {
	(void)mesh;
	(void)cellX;
	(void)cellY;
	return qfalse;
}

void Nav_UnloadSectorTile( navMeshHandle_t mesh, int cellX, int cellY ) {
	(void)mesh;
	(void)cellX;
	(void)cellY;
}

qboolean Nav_BakeSectorTile( int cellX, int cellY, float sectorSize, const navMeshParams_t *params ) {
	(void)cellX;
	(void)cellY;
	(void)sectorSize;
	(void)params;
	return qfalse;
}

qboolean Nav_BakeSectorTileToPath( const char *bspPath, const char *navOutPath,
	int cellX, int cellY, float sectorSize, const navMeshParams_t *params ) {
	(void)bspPath;
	(void)navOutPath;
	(void)cellX;
	(void)cellY;
	(void)sectorSize;
	(void)params;
	return qfalse;
}

qboolean Nav_ValidateTileFileAtPoint( const char *navPath, const vec3_t worldPos, float maxHorizDist ) {
	(void)navPath;
	(void)worldPos;
	(void)maxHorizDist;
	return qfalse;
}

void Nav_BSP_ClearGeometry( void ) {}
int Nav_BSP_AddVertex( float x, float y, float z ) {
	(void)x;
	(void)y;
	(void)z;
	return 0;
}
void Nav_BSP_AddTriangle( int v0, int v1, int v2 ) {
	(void)v0;
	(void)v1;
	(void)v2;
}
