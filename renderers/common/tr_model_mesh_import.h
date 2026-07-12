/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Shared mesh-import helpers (MD3 finalize) for ASCII/STL/DAE and FreeUSD loaders.
===========================================================================
*/

#ifndef TR_MODEL_MESH_IMPORT_H
#define TR_MODEL_MESH_IMPORT_H

#include "q_shared.h"

struct model_s;
typedef struct model_s model_t;

qboolean R_MeshImport_FinalizeMD3( model_t *mod, int lod, const char *name,
	float *verts, int numVerts, int *inds, int numIdx );

/// @param shaderName optional Q3 shader path (NULL = textures/common/white)
/// @param vertSt optional per-vertex ST (numVerts * 2 floats; NULL = 0,0)
qboolean R_MeshImport_FinalizeMD3Ex( model_t *mod, int lod, const char *name,
	float *verts, int numVerts, int *inds, int numIdx,
	const char *shaderName, const float *vertSt );

#endif /* TR_MODEL_MESH_IMPORT_H */
