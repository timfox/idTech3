/*
===========================================================================
RadiusFPS reference (vanilla) farthest point sampling — Algorithm 1.
===========================================================================
*/

#include "radiusfps/radiusfps.h"
#include "radiusfps/radiusfps_internal.h"

#include <float.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

void RadiusFPS_DefaultConfig( radiusfps_config_t *cfg )
{
	if ( !cfg ) {
		return;
	}
	cfg->nvox = RADIUSFPS_DEFAULT_NVOX;
	cfg->seed = 1u;
	cfg->backend = RADIUSFPS_BACKEND_CPU;
	cfg->radius_prune = qtrue;
	cfg->point_skip = qtrue;
}

const char *RadiusFPS_BackendName( radiusfps_backend_t backend )
{
	switch ( backend ) {
	case RADIUSFPS_BACKEND_REFERENCE: return "reference";
	case RADIUSFPS_BACKEND_CPU: return "radiusfps-cpu";
	case RADIUSFPS_BACKEND_GPU: return "radiusfps-g";
	default: return "unknown";
	}
}

qboolean RadiusFPS_GpuAvailable( void )
{
#ifdef RADIUSFPS_HAVE_CUDA
	return RadiusFPS_CudaDeviceReady();
#else
	return qfalse;
#endif
}

qboolean RadiusFPS_ReferenceSample( const float *points_xyz, int num_points, int num_samples,
	int *out_indices, unsigned int seed, radiusfps_profile_t *profile )
{
	float *dist;
	int m;
	int start;
	int last;

	if ( !points_xyz || !out_indices || num_points <= 0 || num_samples <= 0 ) {
		return qfalse;
	}
	if ( num_samples > num_points ) {
		return qfalse;
	}

	dist = (float *)Rfps_Alloc( (size_t)num_points * sizeof( *dist ) );
	if ( !dist ) {
		return qfalse;
	}

	{
		clock_t t0 = clock();
		clock_t t1;

		for ( m = 0; m < num_points; m++ ) {
			dist[m] = FLT_MAX;
		}

		start = Rfps_SeedIndex( num_points, seed );
		out_indices[0] = start;
		dist[start] = 0.0f;

		for ( m = 1; m < num_samples; m++ ) {
			int best_i = 0;
			float best_d = -1.0f;
			int i;

			last = out_indices[m - 1];
			{
				const float lx = points_xyz[last * 3 + 0];
				const float ly = points_xyz[last * 3 + 1];
				const float lz = points_xyz[last * 3 + 2];

				for ( i = 0; i < num_points; i++ ) {
					const float d = Rfps_Vec3Dist(
						points_xyz[i * 3 + 0], points_xyz[i * 3 + 1], points_xyz[i * 3 + 2],
						lx, ly, lz );
					if ( d < dist[i] ) {
						dist[i] = d;
					}
				}
			}

			for ( i = 0; i < num_points; i++ ) {
				if ( Rfps_BetterDistIndex( dist[i], i, best_d, best_i ) ) {
					best_d = dist[i];
					best_i = i;
				}
			}
			out_indices[m] = best_i;
		}

		t1 = clock();
		if ( profile ) {
			memset( profile, 0, sizeof( *profile ) );
			profile->num_points = num_points;
			profile->num_samples = num_samples;
			profile->iterate_ms = Rfps_ElapsedMs( t0, t1 );
			profile->total_ms = profile->iterate_ms;
		}
	}

	Rfps_Free( dist );
	return qtrue;
}

static qboolean Rfps_ComputeBounds( const float *points_xyz, int num_points, float pmin[3], float pmax[3] )
{
	int i;

	if ( num_points <= 0 ) {
		return qfalse;
	}

	pmin[0] = pmax[0] = points_xyz[0];
	pmin[1] = pmax[1] = points_xyz[1];
	pmin[2] = pmax[2] = points_xyz[2];

	for ( i = 1; i < num_points; i++ ) {
		const float x = points_xyz[i * 3 + 0];
		const float y = points_xyz[i * 3 + 1];
		const float z = points_xyz[i * 3 + 2];

		if ( x < pmin[0] ) pmin[0] = x;
		if ( y < pmin[1] ) pmin[1] = y;
		if ( z < pmin[2] ) pmin[2] = z;
		if ( x > pmax[0] ) pmax[0] = x;
		if ( y > pmax[1] ) pmax[1] = y;
		if ( z > pmax[2] ) pmax[2] = z;
	}
	return qtrue;
}

qboolean RadiusFPS_BuildWorkspace( const float *points_xyz, int num_points,
	const radiusfps_config_t *cfg, radiusfps_workspace_t **out_ws )
{
	radiusfps_workspace_t *ws;
	rfps_vid_pair_t *pairs;
	float pmax[3];
	float extent[3];
	float side;
	int nvox;
	int i;
	int v;

	if ( !points_xyz || num_points <= 0 || !out_ws ) {
		return qfalse;
	}

	nvox = cfg && cfg->nvox > 0 ? cfg->nvox : RADIUSFPS_DEFAULT_NVOX;

	ws = (radiusfps_workspace_t *)Rfps_Alloc( sizeof( *ws ) );
	pairs = (rfps_vid_pair_t *)Rfps_Alloc( (size_t)num_points * sizeof( *pairs ) );
	if ( !ws || !pairs ) {
		Rfps_Free( ws );
		Rfps_Free( pairs );
		return qfalse;
	}

	memset( ws, 0, sizeof( *ws ) );
	ws->num_points = num_points;
	ws->nvox = nvox;

	if ( !Rfps_ComputeBounds( points_xyz, num_points, ws->pmin, pmax ) ) {
		Rfps_Free( ws );
		Rfps_Free( pairs );
		return qfalse;
	}

	extent[0] = pmax[0] - ws->pmin[0];
	extent[1] = pmax[1] - ws->pmin[1];
	extent[2] = pmax[2] - ws->pmin[2];
	side = extent[0];
	if ( extent[1] > side ) side = extent[1];
	if ( extent[2] > side ) side = extent[2];
	if ( side <= 0.0f ) {
		side = 1.0f;
	}

	ws->L = RADIUSFPS_BBOX_EXPAND * side / (float)nvox;
	ws->rk = 0.8660254037844386f * ws->L; /* sqrt(3)/2 */

	for ( i = 0; i < num_points; i++ ) {
		int vx;
		int vy;
		int vz;
		float fx;
		float fy;
		float fz;

		fx = ( points_xyz[i * 3 + 0] - ws->pmin[0] ) / ws->L;
		fy = ( points_xyz[i * 3 + 1] - ws->pmin[1] ) / ws->L;
		fz = ( points_xyz[i * 3 + 2] - ws->pmin[2] ) / ws->L;

		vx = (int)floorf( fx );
		vy = (int)floorf( fy );
		vz = (int)floorf( fz );
		if ( vx < 0 ) vx = 0;
		if ( vy < 0 ) vy = 0;
		if ( vz < 0 ) vz = 0;
		if ( vx >= nvox ) vx = nvox - 1;
		if ( vy >= nvox ) vy = nvox - 1;
		if ( vz >= nvox ) vz = nvox - 1;

		pairs[i].vid = vz * nvox * nvox + vy * nvox + vx;
		pairs[i].orig = i;
	}

	qsort( pairs, (size_t)num_points, sizeof( *pairs ), Rfps_CmpVidPair );

	ws->px = (float *)Rfps_Alloc( (size_t)num_points * sizeof( float ) * 3 );
	ws->py = ws->px + num_points;
	ws->pz = ws->px + num_points * 2;
	ws->orig_idx = (int *)Rfps_Alloc( (size_t)num_points * sizeof( int ) );
	if ( !ws->px || !ws->orig_idx ) {
		Rfps_Free( ws->px );
		Rfps_Free( ws->orig_idx );
		Rfps_Free( ws );
		Rfps_Free( pairs );
		return qfalse;
	}

	for ( i = 0; i < num_points; i++ ) {
		const int o = pairs[i].orig;
		ws->px[i] = points_xyz[o * 3 + 0];
		ws->py[i] = points_xyz[o * 3 + 1];
		ws->pz[i] = points_xyz[o * 3 + 2];
		ws->orig_idx[i] = o;
	}

	/* Count active voxels */
	ws->num_voxels = 0;
	for ( i = 0; i < num_points; ) {
		const int vid0 = pairs[i].vid;
		while ( i < num_points && pairs[i].vid == vid0 ) {
			i++;
		}
		ws->num_voxels++;
	}

	ws->voxels = (rfps_voxel_t *)Rfps_Alloc( (size_t)ws->num_voxels * sizeof( rfps_voxel_t ) );
	if ( !ws->voxels ) {
		RadiusFPS_FreeWorkspace( ws );
		Rfps_Free( pairs );
		return qfalse;
	}

	v = 0;
	for ( i = 0; i < num_points; ) {
		const int vid0 = pairs[i].vid;
		const int offset = i;
		int vx;
		int vy;
		int vz;
		float cx;
		float cy;
		float cz;

		while ( i < num_points && pairs[i].vid == vid0 ) {
			i++;
		}

		vz = vid0 / ( nvox * nvox );
		vy = ( vid0 / nvox ) % nvox;
		vx = vid0 % nvox;

		cx = ws->pmin[0] + ( (float)vx + 0.5f ) * ws->L;
		cy = ws->pmin[1] + ( (float)vy + 0.5f ) * ws->L;
		cz = ws->pmin[2] + ( (float)vz + 0.5f ) * ws->L;

		ws->voxels[v].vid = vid0;
		ws->voxels[v].offset = offset;
		ws->voxels[v].count = i - offset;
		ws->voxels[v].cx = cx;
		ws->voxels[v].cy = cy;
		ws->voxels[v].cz = cz;
		ws->voxels[v].rk = ws->rk;
		v++;
	}

	Rfps_Free( pairs );
	*out_ws = ws;
	return qtrue;
}

void RadiusFPS_FreeWorkspace( radiusfps_workspace_t *ws )
{
	if ( !ws ) {
		return;
	}
	Rfps_Free( ws->px );
	Rfps_Free( ws->orig_idx );
	Rfps_Free( ws->voxels );
	Rfps_Free( ws );
}

static qboolean RadiusFPS_CpuSample( const float *points_xyz, int num_points, int num_samples,
	int *out_indices, const radiusfps_config_t *cfg, radiusfps_workspace_t *ws,
	radiusfps_profile_t *profile )
{
	float *distp;
	float *distv;
	int m;
	int start_slot;
	int owned_ws = qfalse;
	unsigned int seed;
	qboolean radius_prune;
	qboolean point_skip;

	if ( !points_xyz || !out_indices || num_points <= 0 || num_samples <= 0 ) {
		return qfalse;
	}
	if ( num_samples > num_points ) {
		return qfalse;
	}

	seed = cfg ? cfg->seed : 1u;
	radius_prune = cfg ? cfg->radius_prune : qtrue;
	point_skip = cfg ? cfg->point_skip : qtrue;

	{
		clock_t t_pre0 = clock();
		clock_t t_pre1;
		clock_t t_init0;
		clock_t t_init1;
		clock_t t_iter0;
		clock_t t_iter1;

	if ( !ws ) {
		if ( !RadiusFPS_BuildWorkspace( points_xyz, num_points, cfg, &ws ) ) {
			return qfalse;
		}
		owned_ws = qtrue;
	}
		t_pre1 = clock();

	distp = (float *)Rfps_Alloc( (size_t)num_points * sizeof( *distp ) );
	distv = (float *)Rfps_Alloc( (size_t)ws->num_voxels * sizeof( *distv ) );
	if ( !distp || !distv ) {
		Rfps_Free( distp );
		Rfps_Free( distv );
		if ( owned_ws ) {
			RadiusFPS_FreeWorkspace( ws );
		}
		return qfalse;
	}

	for ( m = 0; m < num_points; m++ ) {
		distp[m] = FLT_MAX;
	}
	for ( m = 0; m < ws->num_voxels; m++ ) {
		distv[m] = 0.0f;
	}

	t_init0 = clock();
	start_slot = 0;
	{
		const int start_orig = Rfps_SeedIndex( num_points, seed );
		int i;

		for ( i = 0; i < num_points; i++ ) {
			if ( ws->orig_idx[i] == start_orig ) {
				start_slot = i;
				break;
			}
		}
	}
	{
		const float sx = ws->px[start_slot];
		const float sy = ws->py[start_slot];
		const float sz = ws->pz[start_slot];
		int i;

		for ( i = 0; i < num_points; i++ ) {
			distp[i] = Rfps_Vec3Dist( ws->px[i], ws->py[i], ws->pz[i], sx, sy, sz );
		}
	}
	for ( m = 0; m < ws->num_voxels; m++ ) {
		float vmax = -1.0f;
		int q;

		for ( q = 0; q < ws->voxels[m].count; q++ ) {
			const int slot = ws->voxels[m].offset + q;
			if ( distp[slot] > vmax ) {
				vmax = distp[slot];
			}
		}
		distv[m] = vmax;
	}

	t_init1 = clock();
	out_indices[0] = ws->orig_idx[start_slot];

	t_iter0 = clock();
	for ( m = 1; m < num_samples; m++ ) {
		int best_voxel = 0;
		float best_vdist = -1.0f;
		int best_slot = 0;
		float best_pdist = -1.0f;
		int best_orig = ws->orig_idx[0];
		int v;
		int k;
		int q;

		for ( v = 0; v < ws->num_voxels; v++ ) {
			if ( Rfps_BetterDistIndex( distv[v], v, best_vdist, best_voxel ) ) {
				best_vdist = distv[v];
				best_voxel = v;
			}
		}

		{
			const rfps_voxel_t *vk = &ws->voxels[best_voxel];
			for ( q = 0; q < vk->count; q++ ) {
				const int slot = vk->offset + q;
				const int orig = ws->orig_idx[slot];
				if ( Rfps_BetterDistIndex( distp[slot], orig, best_pdist, best_orig ) ) {
					best_pdist = distp[slot];
					best_slot = slot;
					best_orig = orig;
				}
			}
		}

		out_indices[m] = ws->orig_idx[best_slot];
		{
			const float sjx = ws->px[best_slot];
			const float sjy = ws->py[best_slot];
			const float sjz = ws->pz[best_slot];

			for ( k = 0; k < ws->num_voxels; k++ ) {
				const rfps_voxel_t *vk = &ws->voxels[k];
				float lb;

				if ( radius_prune ) {
					const float center_d = Rfps_Vec3Dist( sjx, sjy, sjz, vk->cx, vk->cy, vk->cz );
					lb = center_d - vk->rk;
					if ( lb < 0.0f ) {
						lb = 0.0f;
					}
					if ( lb >= distv[k] ) {
						continue;
					}
				}

				{
				float vmax = -1.0f;
				int qq;

				for ( qq = 0; qq < vk->count; qq++ ) {
					const int slot = vk->offset + qq;
						float dold = distp[slot];
						float dfinal = dold;

						if ( point_skip ) {
							const float dx = fabsf( ws->px[slot] - sjx );
							const float dy = fabsf( ws->py[slot] - sjy );
							const float dz = fabsf( ws->pz[slot] - sjz );
							if ( dx >= dold || dy >= dold || dz >= dold ) {
								/* skip exact L2 */
							} else {
								const float dnew = Rfps_Vec3Dist(
									ws->px[slot], ws->py[slot], ws->pz[slot], sjx, sjy, sjz );
								if ( dnew < dold ) {
									distp[slot] = dnew;
									dfinal = dnew;
								}
							}
						} else {
							const float dnew = Rfps_Vec3Dist(
								ws->px[slot], ws->py[slot], ws->pz[slot], sjx, sjy, sjz );
							if ( dnew < dold ) {
								distp[slot] = dnew;
								dfinal = dnew;
							}
						}

						if ( dfinal > vmax ) {
							vmax = dfinal;
						}
					}
					distv[k] = vmax;
				}
			}
		}
	}
	t_iter1 = clock();

	Rfps_Free( distp );
	Rfps_Free( distv );
	if ( owned_ws ) {
		RadiusFPS_FreeWorkspace( ws );
	}

	if ( profile ) {
		memset( profile, 0, sizeof( *profile ) );
		profile->num_points = num_points;
		profile->num_samples = num_samples;
		profile->num_voxels = ws ? ws->num_voxels : 0;
		profile->preprocess_ms = owned_ws ? Rfps_ElapsedMs( t_pre0, t_pre1 ) : 0.0;
		profile->init_ms = Rfps_ElapsedMs( t_init0, t_init1 );
		profile->iterate_ms = Rfps_ElapsedMs( t_iter0, t_iter1 );
		profile->total_ms = profile->preprocess_ms + profile->init_ms + profile->iterate_ms;
	}
	}
	return qtrue;
}

qboolean RadiusFPS_Sample( const float *points_xyz, int num_points, int num_samples,
	int *out_indices, const radiusfps_config_t *cfg, radiusfps_workspace_t *ws,
	radiusfps_profile_t *profile )
{
	radiusfps_config_t local;
	radiusfps_backend_t backend;

	RadiusFPS_DefaultConfig( &local );
	if ( cfg ) {
		local = *cfg;
	}
	backend = local.backend;

	if ( backend == RADIUSFPS_BACKEND_REFERENCE ) {
		return RadiusFPS_ReferenceSample( points_xyz, num_points, num_samples,
			out_indices, local.seed, profile );
	}

#ifdef RADIUSFPS_HAVE_CUDA
	if ( backend == RADIUSFPS_BACKEND_GPU && RadiusFPS_GpuAvailable() ) {
		return RadiusFPS_GpuSample( points_xyz, num_points, num_samples,
			out_indices, &local, ws, profile );
	}
#endif

	if ( backend == RADIUSFPS_BACKEND_GPU ) {
		/* CUDA not built or no device — CPU RadiusFPS preserves exact FPS output. */
		local.backend = RADIUSFPS_BACKEND_CPU;
	}

	return RadiusFPS_CpuSample( points_xyz, num_points, num_samples,
		out_indices, &local, ws, profile );
}
