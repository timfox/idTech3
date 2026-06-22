/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

glTF GPU tangent topology: per-vertex incident triangle list for Vulkan
PBR vertex shader mode 2 (MikkT-inspired averaged tangent from deformed
geometry). Constants must match gen_vert.tmpl.
===========================================================================
*/

#pragma once

#include "../common/tr_types.h"
#include "tr_model_gltf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Must match GLSL GLTF_GPU_ADJ_TRIS_MAX / GLTF_GPU_TOPO_WORDS_PER_VERT */
#define GLTF_GPU_ADJ_TRIS_MAX		8
#define GLTF_GPU_TOPO_WORDS_PER_VERT	( 1 + GLTF_GPU_ADJ_TRIS_MAX )

/* Per-vertex pull buffer: bind-pose pos(3)+nrm(3)+uv(2)+jw packed as 16 uint32 (see tr_gltf_topo.c) */
#define GLTF_GPU_PULL_UINTS_PER_VERT	16

/*
===============
R_BuildGLTFPrimitiveTopo
===============
Fills topoOut[numVerts * GLTF_GPU_TOPO_WORDS_PER_VERT] uint32s:
  per vertex v: [0]=incident tri count (<= GLTF_GPU_ADJ_TRIS_MAX),
  [1..count]=first index (0,3,6,...) into the primitive's index array for each incident triangle.
topoOut must be zeroed by caller or this function clears it internally.
*/
void R_BuildGLTFPrimitiveTopo( const uint32_t *indices, int numIndices, int numVerts, uint32_t *topoOut );

/*
===============
R_GLTFTopoDrawBlobUints
===============
Total uint32_t count for the dynamic SSBO blob: header + padded indices + topo + padded pull.
*/
int R_GLTFTopoDrawBlobUints( int numIndices, int numVerts );

/*
===============
R_GLTFTopoPackDrawBlob
===============
Packs uint32 blob: header, index copy, per-vertex topo, per-vertex pull (float bits as uint).
Returns topoBase word index via outTopoBase (for asserts / logging).
*/
void R_GLTFTopoPackDrawBlob( const uint32_t *indices, int numIndices, int numVerts,
	const uint32_t *topo, const gltfVertex_t *vertices, uint32_t *out, int *outTopoBase );

#ifdef __cplusplus
}
#endif
