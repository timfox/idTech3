/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Minimal NanoVDB CPU decode (GridData 672B layout, float/half/double leaves).
No dependency on OpenVDB/NanoVDB headers — offsets match AcademySoftwareFoundation/openvdb.
===========================================================================
*/

#ifndef VK_NANOVDB_DECODE_H
#define VK_NANOVDB_DECODE_H

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NANOVDB_GRIDDATA_BYTES    672
#define NANOVDB_TREEDATA_BYTES    64
#define NANOVDB_FILE_HEADER_BYTES 16
#define NANOVDB_FILE_META_BYTES   176

typedef struct {
	int         indexMin[3];
	int         indexMax[3];
	int         dimX, dimY, dimZ;
	qboolean    decoded;
} vdbNanoIndexBBox_t;

/*
===============
VDB_NanoVDB_DecodeToDense
Fills @a dense (dimX*dimY*dimZ floats, zeroed by caller) from a NanoVDB buffer.
@a gridName may be NULL to load the first grid. Returns qfalse on parse/decode failure.
===============
*/
qboolean VDB_NanoVDB_ResolveGrid( const byte *buf, int bufLen, const char *gridName,
	const byte **outGrid );

qboolean VDB_NanoVDB_GetIndexDims( const byte *buf, int bufLen, const char *gridName,
	vdbNanoIndexBBox_t *outIndex );

qboolean VDB_NanoVDB_DecodeToDense( const byte *buf, int bufLen, const char *gridName,
	float *dense, int denseCount, vdbNanoIndexBBox_t *outIndex );

#ifdef __cplusplus
}
#endif

#endif /* VK_NANOVDB_DECODE_H */
