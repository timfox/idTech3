/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

MagicaVoxel .vox binary parse (SIZE / XYZI / RGBA). No renderer dependency.
===========================================================================
*/

#pragma once

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VOX_MAX_DIM       256
#define VOX_MAX_VOXELS    32768

typedef struct {
	int		sizeX;
	int		sizeY;
	int		sizeZ;
	int		numVoxels;
	byte	*xyzc;			/* numVoxels * 4: x,y,z,colorIndex */
	byte	palette[256][4];	/* RGBA; index 0 unused/transparent */
	qboolean	hasRgbaChunk;
} voxModel_t;

/* Parse MagicaVoxel file (first SIZE+XYZI model). Caller must R_Vox_Free. */
qboolean R_Vox_Parse( const byte *data, int size, voxModel_t *out );
void R_Vox_Free( voxModel_t *model );

#ifdef __cplusplus
}
#endif
