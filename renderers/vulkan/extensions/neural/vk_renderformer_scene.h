#pragma once

#include "tr_local.h"

#define RF_MAX_TRIANGLES        16384u

typedef struct {
	float       center[4];
	float       normal[4];
	float       albedo[4];
} rfTriangleToken_t;

typedef struct {
	uint32_t    count;
	uint32_t    indices[4];
} rfGridCell_t;

typedef struct {
	rfTriangleToken_t  *tokens;
	uint32_t            triangleCount;
	rfGridCell_t       *cells;
	uint32_t            gridX;
	uint32_t            gridY;
	uint32_t            gridZ;
	float               worldMin[3];
	float               worldMax[3];
	float               cellSize[3];
	qboolean            valid;
} rfSceneData_t;

qboolean RF_Scene_BuildFromWorld( rfSceneData_t *out, const world_t *w, uint32_t maxTriangles,
	uint32_t gridX, uint32_t gridY, uint32_t gridZ );
void RF_Scene_Free( rfSceneData_t *sd );
