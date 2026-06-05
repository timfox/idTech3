#pragma once

#include "qcommon/q_shared.h"

/* Mímir analytical model — Carter, Hitschfeld & Navarro, arXiv:2504.20937 */

typedef enum {
	MIMIR_RES_FHD = 0,
	MIMIR_RES_QHD,
	MIMIR_RES_UHD
} mimir_res_t;

typedef enum {
	MIMIR_BACKEND_INTEROP = 0,
	MIMIR_BACKEND_RAM,
	MIMIR_BACKEND_OPENGL
} mimir_backend_t;

typedef enum {
	MIMIR_SYNC_ON = 0,
	MIMIR_SYNC_OFF
} mimir_sync_t;

typedef struct {
	int point_count;
	float mimir_fps;
	float ram_fps;
	float opengl_fps;
	float mimir_total_ms;
	float ram_total_ms;
	float gpu_mem_mimir_mib;
	float gpu_mem_ram_mib;
} mimir_interop_row_t;

typedef struct {
	int point_count;
	int target_fps;
	mimir_res_t res;
	float fps_sync_on;
	float fps_sync_off;
} mimir_sync_row_t;

typedef struct {
	mimir_backend_t backend;
	int point_count;
	float fps;
	float total_ms;
	float gpu_mem_mib;
} mimir_benchmark_result_t;

const mimir_interop_row_t *Mimir_InteropTable( int *count );
const mimir_sync_row_t *Mimir_SyncTable( int *count );

void Mimir_Benchmark( mimir_backend_t backend, int point_count, mimir_benchmark_result_t *out );

float Mimir_InteropFpsSpeedup( int point_count );
float Mimir_InteropTimeSpeedup( int point_count );
float Mimir_InteropVramRatio( int point_count );

const char *Mimir_ResName( mimir_res_t res );
const char *Mimir_BackendName( mimir_backend_t backend );
