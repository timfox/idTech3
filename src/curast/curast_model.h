#pragma once

#include "qcommon/q_shared.h"

/* CuRast analytical model — Schütz et al., arXiv:2604.21749 */

typedef enum {
	CURAST_GPU_4070 = 0,
	CURAST_GPU_4090,
	CURAST_GPU_5090
} curast_gpu_t;

typedef enum {
	CURAST_SCENE_SPONZA = 0,
	CURAST_SCENE_LANTERN,
	CURAST_SCENE_LANTERN_INST,
	CURAST_SCENE_KOMAINU,
	CURAST_SCENE_VENICE,
	CURAST_SCENE_ZORAH
} curast_scene_t;

typedef struct {
	float curast_ms;
	float vk_id_ms;
	float vk_pip_ms;
	float visible_tris_m;
	const char *label;
} curast_bench_row_t;

typedef struct {
	float speedup_vs_vk_id;
	float speedup_vs_vk_pip;
	curast_bench_row_t row;
} curast_model_result_t;

void CuRast_ModelBenchmark( curast_scene_t scene, curast_gpu_t gpu, curast_model_result_t *out );

const char *CuRast_StageBreakdown( curast_scene_t scene, float *stage1_ms,
	float *stage2_ms, float *stage3_ms, float *resolve_ms );

const char *CuRast_SceneName( curast_scene_t scene );
const char *CuRast_GpuName( curast_gpu_t gpu );
