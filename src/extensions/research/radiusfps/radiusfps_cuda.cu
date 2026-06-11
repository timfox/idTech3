/*
===========================================================================
RadiusFPS-G CUDA implementation (fusion kernels + device distance updates).
Optional: build with -DUSE_RADIUSFPS_CUDA=ON.
===========================================================================
*/

#include "radiusfps/radiusfps.h"
#include "radiusfps/radiusfps_internal.h"

#include <cuda_runtime.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RFPS_CUDA_CHECK(call) do { \
	cudaError_t err__ = (call); \
	if ( err__ != cudaSuccess ) { \
		fprintf( stderr, "[RadiusFPS-G] CUDA error %s:%d: %s\n", __FILE__, __LINE__, cudaGetErrorString( err__ ) ); \
		return qfalse; \
	} \
} while ( 0 )

struct rfps_gpu_voxel_t {
	int offset;
	int count;
	float cx;
	float cy;
	float cz;
	float rk;
};

__device__ __forceinline__ float RfpsDeviceDist3( float ax, float ay, float az, float bx, float by, float bz )
{
	const float dx = ax - bx;
	const float dy = ay - by;
	const float dz = az - bz;
	return sqrtf( dx * dx + dy * dy + dz * dz );
}

__global__ void RfpsKernelInitDist( const float *px, const float *py, const float *pz,
	int n, int seed_slot, float *distp )
{
	const int i = (int)( blockIdx.x * blockDim.x + threadIdx.x );

	if ( i >= n ) {
		return;
	}

	distp[i] = RfpsDeviceDist3(
		px[i], py[i], pz[i],
		px[seed_slot], py[seed_slot], pz[seed_slot] );
}

__global__ void RfpsKernelVoxelMax( const float *distp, const rfps_gpu_voxel_t *voxels,
	int num_voxels, float *distv )
{
	const int k = (int)blockIdx.x;
	int q;
	float vmax = -1.0f;

	if ( k >= num_voxels ) {
		return;
	}

	for ( q = voxels[k].offset; q < voxels[k].offset + voxels[k].count; q++ ) {
		const float d = distp[q];
		if ( d > vmax ) {
			vmax = d;
		}
	}
	distv[k] = vmax;
}

/* Fusion kernel 1: hierarchical argmax (voxel then point) on a single block. */
__global__ void RfpsKernelFusion1( const float *distp, const float *distv,
	const rfps_gpu_voxel_t *voxels, const int *orig_idx, int num_voxels,
	int *out_sample_slot )
{
	__shared__ int s_best_voxel;
	__shared__ int s_best_slot;
	const int tid = (int)threadIdx.x;
	int k;

	if ( tid == 0 ) {
		float best_v = -1.0f;
		int best_k = 0;

		for ( k = 0; k < num_voxels; k++ ) {
			const float dv = distv[k];
			if ( dv > best_v || ( dv == best_v && k < best_k ) ) {
				best_v = dv;
				best_k = k;
			}
		}
		s_best_voxel = best_k;

		{
			const rfps_gpu_voxel_t vk = voxels[best_k];
			float best_p = -1.0f;
			int best_s = vk.offset;
			int best_o = orig_idx[vk.offset];
			int q;

			for ( q = 0; q < vk.count; q++ ) {
				const int slot = vk.offset + q;
				const float dp = distp[slot];
				const int orig = orig_idx[slot];
				if ( dp > best_p || ( dp == best_p && orig < best_o ) ) {
					best_p = dp;
					best_s = slot;
					best_o = orig;
				}
			}
			s_best_slot = best_s;
		}
		*out_sample_slot = s_best_slot;
	}
}

/* One block per active voxel — radius prune + point skip + dist sync. */
__global__ void RfpsKernelFusion2( const float *px, const float *py, const float *pz,
	float *distp, float *distv, const rfps_gpu_voxel_t *voxels,
	float sjx, float sjy, float sjz, int radius_prune, int point_skip )
{
	const int k = (int)blockIdx.x;
	const rfps_gpu_voxel_t vk = voxels[k];
	float vmax = -1.0f;
	int q;

	if ( k >= gridDim.x ) {
		return;
	}

	if ( radius_prune ) {
		float lb;
		const float center_d = RfpsDeviceDist3( sjx, sjy, sjz, vk.cx, vk.cy, vk.cz );
		lb = center_d - vk.rk;
		if ( lb < 0.0f ) {
			lb = 0.0f;
		}
		if ( lb >= distv[k] ) {
			return;
		}
	}

	for ( q = (int)threadIdx.x; q < vk.count; q += (int)blockDim.x ) {
		const int slot = vk.offset + q;
		float dold = distp[slot];
		float dfinal = dold;

		if ( point_skip ) {
			const float dx = fabsf( px[slot] - sjx );
			const float dy = fabsf( py[slot] - sjy );
			const float dz = fabsf( pz[slot] - sjz );
			if ( dx < dold && dy < dold && dz < dold ) {
				const float dnew = RfpsDeviceDist3( px[slot], py[slot], pz[slot], sjx, sjy, sjz );
				if ( dnew < dold ) {
					distp[slot] = dnew;
					dfinal = dnew;
				}
			}
		} else {
			const float dnew = RfpsDeviceDist3( px[slot], py[slot], pz[slot], sjx, sjy, sjz );
			if ( dnew < dold ) {
				distp[slot] = dnew;
				dfinal = dnew;
			}
		}
	}

	__syncthreads();

	if ( threadIdx.x == 0 ) {
		for ( q = 0; q < vk.count; q++ ) {
			const float d = distp[vk.offset + q];
			if ( d > vmax ) {
				vmax = d;
			}
		}
		distv[k] = vmax;
	}
}

static qboolean Rfps_UploadWorkspace( const radiusfps_workspace_t *ws,
	float **d_px, float **d_py, float **d_pz, int **d_orig,
	rfps_gpu_voxel_t **d_voxels, float **d_distp, float **d_distv )
{
	const size_t pn = (size_t)ws->num_points;
	const size_t vn = (size_t)ws->num_voxels;
	rfps_gpu_voxel_t *hv;
	size_t i;

	RFPS_CUDA_CHECK( cudaMalloc( d_px, pn * sizeof( float ) ) );
	RFPS_CUDA_CHECK( cudaMalloc( d_py, pn * sizeof( float ) ) );
	RFPS_CUDA_CHECK( cudaMalloc( d_pz, pn * sizeof( float ) ) );
	RFPS_CUDA_CHECK( cudaMalloc( d_orig, pn * sizeof( int ) ) );
	RFPS_CUDA_CHECK( cudaMalloc( d_distp, pn * sizeof( float ) ) );
	RFPS_CUDA_CHECK( cudaMalloc( d_distv, vn * sizeof( float ) ) );
	RFPS_CUDA_CHECK( cudaMalloc( d_voxels, vn * sizeof( rfps_gpu_voxel_t ) ) );

	RFPS_CUDA_CHECK( cudaMemcpy( *d_px, ws->px, pn * sizeof( float ), cudaMemcpyHostToDevice ) );
	RFPS_CUDA_CHECK( cudaMemcpy( *d_py, ws->py, pn * sizeof( float ), cudaMemcpyHostToDevice ) );
	RFPS_CUDA_CHECK( cudaMemcpy( *d_pz, ws->pz, pn * sizeof( float ), cudaMemcpyHostToDevice ) );
	RFPS_CUDA_CHECK( cudaMemcpy( *d_orig, ws->orig_idx, pn * sizeof( int ), cudaMemcpyHostToDevice ) );

	hv = (rfps_gpu_voxel_t *)malloc( vn * sizeof( *hv ) );
	if ( !hv ) {
	 return qfalse;
	}
	for ( i = 0; i < vn; i++ ) {
		hv[i].offset = ws->voxels[i].offset;
		hv[i].count = ws->voxels[i].count;
		hv[i].cx = ws->voxels[i].cx;
		hv[i].cy = ws->voxels[i].cy;
		hv[i].cz = ws->voxels[i].cz;
		hv[i].rk = ws->voxels[i].rk;
	}
	RFPS_CUDA_CHECK( cudaMemcpy( *d_voxels, hv, vn * sizeof( rfps_gpu_voxel_t ), cudaMemcpyHostToDevice ) );
	free( hv );
	return qtrue;
}

static void Rfps_FreeDevice( float *d_px, float *d_py, float *d_pz, int *d_orig,
	rfps_gpu_voxel_t *d_voxels, float *d_distp, float *d_distv )
{
	cudaFree( d_px );
	cudaFree( d_py );
	cudaFree( d_pz );
	cudaFree( d_orig );
	cudaFree( d_voxels );
	cudaFree( d_distp );
	cudaFree( d_distv );
}

extern "C" qboolean RadiusFPS_CudaDeviceReady( void )
{
	int count = 0;
	if ( cudaGetDeviceCount( &count ) != cudaSuccess || count <= 0 ) {
		return qfalse;
	}
	return qtrue;
}

extern "C" qboolean RadiusFPS_GpuSample( const float *points_xyz, int num_points, int num_samples,
	int *out_indices, const radiusfps_config_t *cfg, radiusfps_workspace_t *ws,
	radiusfps_profile_t *profile )
{
	radiusfps_workspace_t *local_ws = NULL;
	int owned_ws = 0;
	clock_t t_pre0 = clock();
	clock_t t_pre1;
	clock_t t_iter0;
	clock_t t_iter1;
	float *d_px = NULL;
	float *d_py = NULL;
	float *d_pz = NULL;
	int *d_orig = NULL;
	rfps_gpu_voxel_t *d_voxels = NULL;
	float *d_distp = NULL;
	float *d_distv = NULL;
	int *d_sample_slot = NULL;
	int seed_slot;
	int num_voxels = 0;
	int m;
	const int tpb = 256;
	qboolean ok = qfalse;

	if ( !ws ) {
		if ( !RadiusFPS_BuildWorkspace( points_xyz, num_points, cfg, &local_ws ) ) {
			return qfalse;
		}
		ws = local_ws;
		owned_ws = 1;
	}
	t_pre1 = clock();
	num_voxels = ws->num_voxels;

	if ( !Rfps_UploadWorkspace( ws, &d_px, &d_py, &d_pz, &d_orig, &d_voxels, &d_distp, &d_distv ) ) {
		goto cleanup;
	}
	RFPS_CUDA_CHECK( cudaMalloc( &d_sample_slot, sizeof( int ) ) );

	seed_slot = 0;
	{
		const int start_orig = Rfps_SeedIndex( num_points, cfg ? cfg->seed : 1u );
		int i;

		for ( i = 0; i < num_points; i++ ) {
			if ( ws->orig_idx[i] == start_orig ) {
				seed_slot = i;
				break;
			}
		}
	}
	out_indices[0] = ws->orig_idx[seed_slot];

	t_iter0 = clock();
	{
		const int blocks = ( num_points + tpb - 1 ) / tpb;
		RfpsKernelInitDist<<<blocks, tpb>>>( d_px, d_py, d_pz, num_points, seed_slot, d_distp );
		RfpsKernelVoxelMax<<<num_voxels, 1>>>( d_distp, d_voxels, num_voxels, d_distv );
		RFPS_CUDA_CHECK( cudaDeviceSynchronize() );
	}

	for ( m = 1; m < num_samples; m++ ) {
		int sample_slot = 0;
		float hpx;
		float hpy;
		float hpz;
		int horig;

		RfpsKernelFusion1<<<1, 1>>>( d_distp, d_distv, d_voxels, d_orig, num_voxels, d_sample_slot );
		RFPS_CUDA_CHECK( cudaMemcpy( &sample_slot, d_sample_slot, sizeof( int ), cudaMemcpyDeviceToHost ) );

		RFPS_CUDA_CHECK( cudaMemcpy( &hpx, d_px + sample_slot, sizeof( float ), cudaMemcpyDeviceToHost ) );
		RFPS_CUDA_CHECK( cudaMemcpy( &hpy, d_py + sample_slot, sizeof( float ), cudaMemcpyDeviceToHost ) );
		RFPS_CUDA_CHECK( cudaMemcpy( &hpz, d_pz + sample_slot, sizeof( float ), cudaMemcpyDeviceToHost ) );
		RFPS_CUDA_CHECK( cudaMemcpy( &horig, d_orig + sample_slot, sizeof( int ), cudaMemcpyDeviceToHost ) );
		out_indices[m] = horig;

		RfpsKernelFusion2<<<num_voxels, tpb>>>(
			d_px, d_py, d_pz, d_distp, d_distv, d_voxels,
			hpx, hpy, hpz,
			cfg && cfg->radius_prune ? 1 : 0,
			cfg && cfg->point_skip ? 1 : 0 );
		RFPS_CUDA_CHECK( cudaDeviceSynchronize() );
	}
	t_iter1 = clock();

	ok = qtrue;

cleanup:
	cudaFree( d_sample_slot );
	if ( d_px ) {
		Rfps_FreeDevice( d_px, d_py, d_pz, d_orig, d_voxels, d_distp, d_distv );
	}
	if ( profile ) {
		memset( profile, 0, sizeof( *profile ) );
		profile->num_points = num_points;
		profile->num_samples = num_samples;
		profile->num_voxels = num_voxels;
		profile->preprocess_ms = owned_ws ? ( 1000.0 * (double)( t_pre1 - t_pre0 ) / (double)CLOCKS_PER_SEC ) : 0.0;
		profile->iterate_ms = 1000.0 * (double)( t_iter1 - t_iter0 ) / (double)CLOCKS_PER_SEC;
		profile->total_ms = profile->preprocess_ms + profile->iterate_ms;
	}
	if ( owned_ws ) {
		RadiusFPS_FreeWorkspace( ws );
	}
	return ok;
}
