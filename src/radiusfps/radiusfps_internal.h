#pragma once

#include "radiusfps/radiusfps.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
	int vid;
	int orig;
} rfps_vid_pair_t;

typedef struct {
	int vid;
	int offset;
	int count;
	float cx;
	float cy;
	float cz;
	float rk;
} rfps_voxel_t;

struct radiusfps_workspace_s {
	int num_points;
	int nvox;
	float pmin[3];
	float L;
	float rk;
	float *px;
	float *py;
	float *pz;
	int *orig_idx;
	rfps_voxel_t *voxels;
	int num_voxels;
};

static inline void *Rfps_Alloc( size_t bytes )
{
	return bytes ? malloc( bytes ) : NULL;
}

static inline void Rfps_Free( void *p )
{
	free( p );
}

static inline float Rfps_Vec3Dist( float ax, float ay, float az, float bx, float by, float bz )
{
	const float dx = ax - bx;
	const float dy = ay - by;
	const float dz = az - bz;
	return sqrtf( dx * dx + dy * dy + dz * dz );
}

static inline int Rfps_BetterDistIndex( float d_cand, int i_cand, float d_best, int i_best )
{
	if ( d_cand > d_best ) {
		return 1;
	}
	if ( d_cand == d_best && i_cand < i_best ) {
		return 1;
	}
	return 0;
}

static inline int Rfps_SeedIndex( int n, unsigned int seed )
{
	uint32_t state;

	if ( n <= 0 ) {
		return 0;
	}
	state = seed ? seed : 1u;
	state = state * 1664525u + 1013904223u;
	return (int)( state % (uint32_t)n );
}

static int Rfps_CmpVidPair( const void *a, const void *b )
{
	const rfps_vid_pair_t *pa = (const rfps_vid_pair_t *)a;
	const rfps_vid_pair_t *pb = (const rfps_vid_pair_t *)b;
	if ( pa->vid < pb->vid ) {
		return -1;
	}
	if ( pa->vid > pb->vid ) {
		return 1;
	}
	return pa->orig - pb->orig;
}

static inline double Rfps_ElapsedMs( clock_t t0, clock_t t1 )
{
	return 1000.0 * (double)( t1 - t0 ) / (double)CLOCKS_PER_SEC;
}

#ifdef RADIUSFPS_HAVE_CUDA
#ifdef __cplusplus
extern "C" {
#endif
qboolean RadiusFPS_GpuSample( const float *points_xyz, int num_points, int num_samples,
	int *out_indices, const radiusfps_config_t *cfg, radiusfps_workspace_t *ws,
	radiusfps_profile_t *profile );
qboolean RadiusFPS_CudaDeviceReady( void );
#ifdef __cplusplus
}
#endif
#endif
