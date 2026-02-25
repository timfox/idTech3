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
