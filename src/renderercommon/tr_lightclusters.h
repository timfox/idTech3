/*
===========================================================================
Shared Light Clustering Types

Cluster grid parameters and limits shared across backends (GL/VK/GL2).
===========================================================================
*/

#pragma once

#include "tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Tunable clustering grid
#define LC_MAX_LIGHTS       1024      // hard cap on dynamic lights considered
#define LC_MAX_LIGHTS_PER_CLUSTER 64  // per-cluster list cap
#define LC_MAX_CLUSTERS     8192      // safety cap for allocations

typedef struct {
	int tilesX;
	int tilesY;
	int slicesZ;
	float zNear;
	float zFar;
	float invLogZ; // for logarithmic depth slicing, optional
} lc_grid_params_t;

typedef struct {
	int lightOffset; // start index into lightIndices buffer
	int lightCount;  // number of lights affecting this cluster
} lc_cluster_header_t;

#ifdef __cplusplus
} // extern "C"
#endif


