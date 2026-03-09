/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

OpenVDB / NanoVDB integration for volumetric data.
Loads .vdb and .nvdb files for use as fog density volumes,
cloud shapes, explosion sequences, and other volumetric effects.

Supports:
  - NanoVDB (.nvdb) — lightweight header-only, GPU-friendly
  - OpenVDB (.vdb) — full library, optional compile-time dependency
  - Integration with the volumetric fog system
  - Lua API for loading and querying grids
===========================================================================
*/

#ifndef VK_VDB_H
#define VK_VDB_H

#include "../../qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VDB_MAX_GRIDS       16
#define VDB_INVALID_HANDLE  (-1)

typedef int vdbHandle_t;

typedef enum {
	VDB_GRID_FLOAT,
	VDB_GRID_VEC3,
	VDB_GRID_INT32,
	VDB_GRID_UNKNOWN
} vdbGridType_t;

typedef struct {
	float       worldMin[3];
	float       worldMax[3];
	float       voxelSize;
	int         dimX, dimY, dimZ;
	int         activeVoxels;
	vdbGridType_t type;
	char        name[64];
} vdbGridInfo_t;

void        VDB_Init( void );
void        VDB_Shutdown( void );

vdbHandle_t VDB_Load( const char *filename, const char *gridName );
void        VDB_Free( vdbHandle_t handle );
qboolean    VDB_GetInfo( vdbHandle_t handle, vdbGridInfo_t *info );

float       VDB_SampleFloat( vdbHandle_t handle, float x, float y, float z );
void        VDB_SampleVec3( vdbHandle_t handle, float x, float y, float z, float *outX, float *outY, float *outZ );

qboolean    VDB_UploadToGPU( vdbHandle_t handle );
qboolean    VDB_BindAsFogDensity( vdbHandle_t handle );
vdbHandle_t VDB_GetBoundFogDensityHandle( void );
int         VDB_GetGridCount( void );

#ifdef __cplusplus
}
#endif

#endif /* VK_VDB_H */
