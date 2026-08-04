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
/// @param vertNormals optional per-vertex normals (numVerts * 3 floats; NULL = generate face normals)
qboolean R_MeshImport_FinalizeMD3Ex( model_t *mod, int lod, const char *name,
	float *verts, int numVerts, int *inds, int numIdx,
	const char *shaderName, const float *vertSt, const float *vertNormals );

typedef struct meshImportSurface_s {
	int firstTri;
	int numTris;
	const char *shaderName;
	const char *normalMap;
	const char *emissiveMap;
	const char *metallicMap;
	const char *roughnessMap;
	qboolean hasOpacity;
	qboolean hasOpacityThreshold;
	float opacity;
	float opacityThreshold;
} meshImportSurface_t;

/// Finalize triangle-expanded geometry while preserving authored material
/// ranges. Each range addresses consecutive triangles in @p verts/@p inds.
qboolean R_MeshImport_FinalizeMD3Multi( model_t *mod, int lod, const char *name,
	float *verts, int numVerts, int *inds, int numIdx,
	const meshImportSurface_t *surfaces, int numSurfaces,
	const float *vertSt, const float *vertNormals );

#endif /* TR_MODEL_MESH_IMPORT_H */
