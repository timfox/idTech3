#pragma once

#include "qcommon/q_shared.h"

/* Infernux analytical model — Chen, arXiv:2604.10263 */

typedef enum {
	INFERNUX_BENCH_SPAWN_SINGLE = 0,
	INFERNUX_BENCH_SPAWN_MULTI10,
	INFERNUX_BENCH_SPAWN_MULTI100,
	INFERNUX_BENCH_PURE_COMPUTE
} infernux_bench_t;

typedef struct {
	int grid_n;
	float infernux_editor_fps;
	float infernux_runtime_fps;
	float unity_editor_fps;
	float unity_runtime_fps;
} infernux_row_t;

const infernux_row_t *Infernux_Table( infernux_bench_t bench, int *count );
float Infernux_JitSpeedup( int grid_n );
