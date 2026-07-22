/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

BSP30 planar face triangulation (ear clipping) — public API.
===========================================================================
*/

#ifndef TR_BSP30_TRIANGULATE_H
#define TR_BSP30_TRIANGULATE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Max vertices per BSP30 face we ear-clip without falling back to a fan. */
#define BSP30_TRIANGULATE_MAX_POINTS 256

/*
===============
R_Bsp30_TriangulateFace

xyz: numPoints * 3 floats (x,y,z) in ring order.
outIndices: receives (numPoints-2)*3 vertex indices into xyz.
Returns index count on success, or -1 if arguments are invalid.
On algorithm failure, writes a triangle-fan fallback and still returns
(numPoints-2)*3 so callers always get a drawable mesh.
===============
*/
int R_Bsp30_TriangulateFace( const float *xyz, int numPoints, int *outIndices,
	int maxIndices );

/*
===============
R_Bsp30_TriangleCentroidInside

Validation helper: every triangle centroid must lie inside the polygon.
Returns 1 if all pass, 0 otherwise.
===============
*/
int R_Bsp30_TriangleCentroidInside( const float *xyz, int numPoints,
	const int *indices, int numIndices );

#ifdef __cplusplus
}
#endif

#endif
