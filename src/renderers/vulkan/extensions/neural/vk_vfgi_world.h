#pragma once

#include "tr_local.h"

#define VFGI_MAX_VERTICES       1048576u
#define VFGI_FEATURE_DIM        4
#define VFGI_CELL_MAX_INDICES   4u

typedef struct {
	float       pos[4];
	float       feat[VFGI_FEATURE_DIM];
} vfgiVertexRecord_t;

typedef struct {
	uint32_t    count;
	uint32_t    indices[VFGI_CELL_MAX_INDICES];
} vfgiGridCell_t;

typedef struct {
	vfgiVertexRecord_t *vertices;
	uint32_t            vertexCount;
	vfgiGridCell_t     *cells;
	uint32_t            gridX;
	uint32_t            gridY;
	uint32_t            gridZ;
	float               worldMin[3];
	float               worldMax[3];
	float               cellSize[3];
	qboolean            valid;
} vfgiWorldData_t;

qboolean VFGI_World_BuildFromMap( vfgiWorldData_t *out, const world_t *w, uint32_t maxVertices,
	float quantUnits, uint32_t gridX, uint32_t gridY, uint32_t gridZ, qboolean proceduralFeatures );
void VFGI_World_Free( vfgiWorldData_t *wd );
