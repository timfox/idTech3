/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

C ABI between FreeUSD C++ loader and renderer model registration (C).
===========================================================================
*/

#ifndef TR_MODEL_FREEUSD_H
#define TR_MODEL_FREEUSD_H

#include "q_shared.h"

struct model_s;
typedef struct model_s model_t;

#define R_FREEUSD_SHADERNAME_MAX  MAX_QPATH

typedef struct freeusdMeshSurface_s {
	int firstTri;
	int numTris;
	char shaderName[R_FREEUSD_SHADERNAME_MAX];
	int hasOpacity;
	int hasOpacityThreshold;
	float opacity;
	float opacityThreshold;
} freeusdMeshSurface_t;

/// Renderer startup: register r_freeusd* cvars and log FreeUSD version when USE_FREEUSD.
void R_Freeusd_Init( void );

/// When 0, .usd/.usda use vertex-soup import only (FreeUSD path skipped).
qboolean R_Freeusd_MeshImportEnabled( void );

/// Load tessellatable UsdGeom.Mesh from a Quake VFS path into heap buffers.
/// Caller must ri.Free( *verts ), ri.Free( *inds ), *vertSt and *vertNormals when non-NULL.
/// @param shaderNameOut optional buffer (R_FREEUSD_SHADERNAME_MAX) for resolved Q3 shader path
qboolean R_Freeusd_BuildMeshBuffers( const char *qpath, float **verts, int *numVerts,
	int **inds, int *numIdx, float **vertSt, float **vertNormals,
	char *shaderNameOut, int shaderNameOutSize,
	freeusdMeshSurface_t **surfacesOut, int *numSurfacesOut );

qhandle_t R_RegisterFreeusdMesh( const char *name, model_t *mod );

#endif /* TR_MODEL_FREEUSD_H */
