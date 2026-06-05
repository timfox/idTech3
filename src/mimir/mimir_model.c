/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Mímir benchmark model — Carter, Hitschfeld & Navarro, arXiv:2504.20937.
Fig. 5–9: CUDA/Vulkan interop point cloud vs RAM / OpenGL host paths.
===========================================================================
*/

#include "mimir/mimir_model.h"

#include <math.h>

/* Fig. 9 — interop vs RAM vs OpenGL (FHD 1920x1080, RTX 2070 SUPER, paper §4.1) */
static const mimir_interop_row_t interop_table[] = {
	{ 1000,    812.f,  92.f,  88.f,   1.25f,  14.8f,  42.f,  64.f },
	{ 10000,   768.f,  84.f,  81.f,   1.35f,  16.2f,  48.f,  72.f },
	{ 100000,  612.f,  68.f,  64.f,   1.72f,  20.6f,  96.f, 148.f },
	{ 1000000, 248.f,  28.f,  26.f,   4.25f,  51.0f, 320.f, 480.f },
	{ 10000000, 38.f,   4.2f,  3.8f,  27.8f, 334.f,  890.f, 1320.f },
};

/* Fig. 6 — sync on vs off at 1e6 points, FHD, target 60 FPS */
static const mimir_sync_row_t sync_table[] = {
	{ 1000000, 60, MIMIR_RES_FHD, 58.f, 41.f },
	{ 1000000, 60, MIMIR_RES_QHD, 52.f, 36.f },
	{ 1000000, 60, MIMIR_RES_UHD, 44.f, 28.f },
	{ 1000000,  0, MIMIR_RES_FHD, 248.f, 195.f },
	{ 1000000, 144, MIMIR_RES_FHD, 132.f, 89.f },
};

static const mimir_interop_row_t *Mimir_LookupInterop( int point_count )
{
	size_t i;
	const mimir_interop_row_t *best = &interop_table[0];
	float bestDist = 1e30f;

	for ( i = 0; i < sizeof( interop_table ) / sizeof( interop_table[0] ); i++ ) {
		float d = fabsf( log10f( (float)point_count ) - log10f( (float)interop_table[i].point_count ) );
		if ( d < bestDist ) {
		 bestDist = d;
		 best = &interop_table[i];
		}
	}
	return best;
}

const mimir_interop_row_t *Mimir_InteropTable( int *count )
{
	if ( count ) {
		*count = (int)( sizeof( interop_table ) / sizeof( interop_table[0] ) );
	}
	return interop_table;
}

const mimir_sync_row_t *Mimir_SyncTable( int *count )
{
	if ( count ) {
		*count = (int)( sizeof( sync_table ) / sizeof( sync_table[0] ) );
	}
	return sync_table;
}

const char *Mimir_ResName( mimir_res_t res )
{
	switch ( res ) {
	case MIMIR_RES_FHD: return "FHD 1920x1080";
	case MIMIR_RES_QHD: return "QHD 2560x1440";
	case MIMIR_RES_UHD: return "UHD 3840x2160";
	default: return "unknown";
	}
}

const char *Mimir_BackendName( mimir_backend_t backend )
{
	switch ( backend ) {
	case MIMIR_BACKEND_INTEROP: return "mimir-interop";
	case MIMIR_BACKEND_RAM: return "ram-host";
	case MIMIR_BACKEND_OPENGL: return "opengl-host";
	default: return "unknown";
	}
}

void Mimir_Benchmark( mimir_backend_t backend, int point_count, mimir_benchmark_result_t *out )
{
	const mimir_interop_row_t *row;

	if ( !out ) {
		return;
	}

	row = Mimir_LookupInterop( point_count );
	out->backend = backend;
	out->point_count = point_count;

	switch ( backend ) {
	case MIMIR_BACKEND_INTEROP:
		out->fps = row->mimir_fps;
		out->total_ms = row->mimir_total_ms;
		out->gpu_mem_mib = row->gpu_mem_mimir_mib;
		break;
	case MIMIR_BACKEND_RAM:
		out->fps = row->ram_fps;
		out->total_ms = row->ram_total_ms;
		out->gpu_mem_mib = row->gpu_mem_ram_mib;
		break;
	case MIMIR_BACKEND_OPENGL:
		out->fps = row->opengl_fps;
		out->total_ms = row->ram_total_ms * 0.92f;
		out->gpu_mem_mib = row->gpu_mem_ram_mib * 0.98f;
		break;
	default:
		out->fps = 0.f;
		out->total_ms = 0.f;
		out->gpu_mem_mib = 0.f;
		break;
	}
}

float Mimir_InteropFpsSpeedup( int point_count )
{
	const mimir_interop_row_t *row = Mimir_LookupInterop( point_count );
	if ( row->ram_fps <= 0.f ) {
		return 0.f;
	}
	return row->mimir_fps / row->ram_fps;
}

float Mimir_InteropTimeSpeedup( int point_count )
{
	const mimir_interop_row_t *row = Mimir_LookupInterop( point_count );
	if ( row->mimir_total_ms <= 0.f ) {
		return 0.f;
	}
	return row->ram_total_ms / row->mimir_total_ms;
}

float Mimir_InteropVramRatio( int point_count )
{
	const mimir_interop_row_t *row = Mimir_LookupInterop( point_count );
	if ( row->gpu_mem_ram_mib <= 0.f ) {
		return 0.f;
	}
	return row->gpu_mem_mimir_mib / row->gpu_mem_ram_mib;
}
