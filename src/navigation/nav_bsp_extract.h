/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
BSP geometry extraction for Recast navmesh.
===========================================================================
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void   Nav_BSP_ClearGeometry(void);
int    Nav_BSP_AddVertex(float x, float y, float z);
void   Nav_BSP_AddTriangle(int v0, int v1, int v2);
float *Nav_BSP_GetVerts(void);
int   *Nav_BSP_GetTris(void);
int    Nav_BSP_GetVertCount(void);
int    Nav_BSP_GetTriCount(void);

qboolean Nav_BSP_ExtractFromSectorMap( int cellX, int cellY, float sectorSize );

#ifdef __cplusplus
}
#endif
