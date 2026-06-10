#pragma once

/*
===========================================================================
RadiusFPS — efficient farthest point sampling via spherical voxel pruning.
Yu et al., arXiv:2606.06255 (RadiusFPS / RadiusFPS-G).

Exact FPS distance-update rule under fixed seed and deterministic tie-breaking
(smallest original point index on equal distance).
===========================================================================
*/

#include "qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RADIUSFPS_DEFAULT_NVOX 16
#define RADIUSFPS_BBOX_EXPAND    1.000001f

typedef enum {
	RADIUSFPS_BACKEND_REFERENCE = 0,
	RADIUSFPS_BACKEND_CPU       = 1,
	RADIUSFPS_BACKEND_GPU       = 2
} radiusfps_backend_t;

typedef struct {
	int nvox;
	unsigned int seed;
	radiusfps_backend_t backend;
	qboolean radius_prune;
	qboolean point_skip;
} radiusfps_config_t;

typedef struct {
	double preprocess_ms;
	double init_ms;
	double iterate_ms;
	double total_ms;
	int num_voxels;
	int num_points;
	int num_samples;
} radiusfps_profile_t;

typedef struct radiusfps_workspace_s radiusfps_workspace_t;

void RadiusFPS_DefaultConfig( radiusfps_config_t *cfg );

/* Build spatial index (voxelization). Returns qfalse on failure. */
qboolean RadiusFPS_BuildWorkspace( const float *points_xyz, int num_points,
	const radiusfps_config_t *cfg, radiusfps_workspace_t **out_ws );

void RadiusFPS_FreeWorkspace( radiusfps_workspace_t *ws );

/*
 * Sample M indices into out_indices (caller allocates num_samples ints).
 * points_xyz: interleaved [x0,y0,z0, x1,y1,z1, ...].
 * ws may be NULL — built temporarily when backend needs it.
 */
qboolean RadiusFPS_Sample( const float *points_xyz, int num_points, int num_samples,
	int *out_indices, const radiusfps_config_t *cfg, radiusfps_workspace_t *ws,
	radiusfps_profile_t *profile );

/* Vanilla O(N*M) FPS — validation baseline. */
qboolean RadiusFPS_ReferenceSample( const float *points_xyz, int num_points, int num_samples,
	int *out_indices, unsigned int seed, radiusfps_profile_t *profile );

qboolean RadiusFPS_GpuAvailable( void );
const char *RadiusFPS_BackendName( radiusfps_backend_t backend );

#ifdef __cplusplus
}
#endif
