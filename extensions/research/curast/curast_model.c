/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CuRast benchmark model (Table 2 / Table 3, arXiv:2604.21749).
===========================================================================
*/

#include "curast/curast_model.h"

typedef struct {
	curast_scene_t scene;
	curast_gpu_t gpu;
	curast_bench_row_t row;
} curast_table_entry_t;

/* Table 2 — close-up rows unless scene is overview Zorah */
static const curast_table_entry_t table2[] = {
	{ CURAST_SCENE_SPONZA, CURAST_GPU_4070, { 0.580f, 0.068f, 0.071f, 0.197f, "Sponza closeup" } },
	{ CURAST_SCENE_SPONZA, CURAST_GPU_4090, { 0.271f, 0.040f, 0.052f, 0.197f, "Sponza closeup" } },
	{ CURAST_SCENE_SPONZA, CURAST_GPU_5090, { 0.251f, 0.022f, 0.025f, 0.197f, "Sponza closeup" } },

	{ CURAST_SCENE_LANTERN, CURAST_GPU_4090, { 0.208f, 0.056f, 0.094f, 1.0f, "Lantern" } },
	{ CURAST_SCENE_LANTERN_INST, CURAST_GPU_4090, { 17.325f, 142.001f, 282.385f, 3100.0f, "Lantern instanced" } },
	{ CURAST_SCENE_LANTERN_INST, CURAST_GPU_5090, { 9.951f, 125.677f, 142.147f, 3100.0f, "Lantern instanced" } },

	{ CURAST_SCENE_KOMAINU, CURAST_GPU_4090, { 0.865f, 2.791f, 2.678f, 28.7f, "Komainu Kobe" } },
	{ CURAST_SCENE_KOMAINU, CURAST_GPU_5090, { 0.644f, 2.413f, 1.365f, 28.7f, "Komainu Kobe" } },

	{ CURAST_SCENE_VENICE, CURAST_GPU_4090, { 9.570f, 19.710f, 37.575f, 399.9f, "Venice h" } },
	{ CURAST_SCENE_VENICE, CURAST_GPU_5090, { 5.252f, 17.106f, 18.888f, 399.9f, "Venice h" } },

	{ CURAST_SCENE_ZORAH, CURAST_GPU_4090, { 74.906f, 0.0f, 1778.303f, 13600.0f, "Zorah closeup c" } },
	{ CURAST_SCENE_ZORAH, CURAST_GPU_5090, { 57.569f, 0.0f, 633.081f, 13600.0f, "Zorah closeup c" } },
};

static const curast_table_entry_t *CuRast_Lookup( curast_scene_t scene, curast_gpu_t gpu )
{
	size_t i;

	for ( i = 0; i < sizeof( table2 ) / sizeof( table2[0] ); i++ ) {
		if ( table2[i].scene == scene && table2[i].gpu == gpu ) {
			return &table2[i];
		}
	}
	/* Fallback: RTX 4090 row for scene, or first row */
	for ( i = 0; i < sizeof( table2 ) / sizeof( table2[0] ); i++ ) {
		if ( table2[i].scene == scene ) {
			return &table2[i];
		}
	}
	return &table2[0];
}

const char *CuRast_SceneName( curast_scene_t scene )
{
	switch ( scene ) {
	case CURAST_SCENE_SPONZA: return "sponza";
	case CURAST_SCENE_LANTERN: return "lantern";
	case CURAST_SCENE_LANTERN_INST: return "lantern_inst";
	case CURAST_SCENE_KOMAINU: return "komainu";
	case CURAST_SCENE_VENICE: return "venice";
	case CURAST_SCENE_ZORAH: return "zorah";
	default: return "unknown";
	}
}

const char *CuRast_GpuName( curast_gpu_t gpu )
{
	switch ( gpu ) {
	case CURAST_GPU_4070: return "RTX 4070";
	case CURAST_GPU_4090: return "RTX 4090";
	case CURAST_GPU_5090: return "RTX 5090";
	default: return "RTX 4090";
	}
}

void CuRast_ModelBenchmark( curast_scene_t scene, curast_gpu_t gpu, curast_model_result_t *out )
{
	const curast_table_entry_t *e;

	if ( !out ) {
		return;
	}

	e = CuRast_Lookup( scene, gpu );
	out->row = e->row;
	out->speedup_vs_vk_id = ( e->row.vk_id_ms > 0.0f ) ? ( e->row.vk_id_ms / e->row.curast_ms ) : 0.0f;
	out->speedup_vs_vk_pip = ( e->row.vk_pip_ms > 0.0f ) ? ( e->row.vk_pip_ms / e->row.curast_ms ) : 0.0f;
}

const char *CuRast_StageBreakdown( curast_scene_t scene, float *stage1_ms,
	float *stage2_ms, float *stage3_ms, float *resolve_ms )
{
	/* Table 3 — RTX 4090 stage timings (ms) */
	static const struct {
		curast_scene_t scene;
		float s1, s2, s3, resolve;
	} stages[] = {
		{ CURAST_SCENE_SPONZA, 0.048f, 0.054f, 0.063f, 0.098f },
		{ CURAST_SCENE_VENICE, 10.162f, 0.033f, 0.006f, 0.517f },
		{ CURAST_SCENE_ZORAH, 74.109f, 0.193f, 0.183f, 0.416f },
	};
	size_t i;

	for ( i = 0; i < sizeof( stages ) / sizeof( stages[0] ); i++ ) {
		if ( stages[i].scene == scene ) {
			if ( stage1_ms ) {
				*stage1_ms = stages[i].s1;
			}
			if ( stage2_ms ) {
				*stage2_ms = stages[i].s2;
			}
			if ( stage3_ms ) {
				*stage3_ms = stages[i].s3;
			}
			if ( resolve_ms ) {
				*resolve_ms = stages[i].resolve;
			}
			return CuRast_SceneName( scene );
		}
	}

	if ( stage1_ms ) {
		*stage1_ms = 0.0f;
	}
	if ( stage2_ms ) {
		*stage2_ms = 0.0f;
	}
	if ( stage3_ms ) {
		*stage3_ms = 0.0f;
	}
	if ( resolve_ms ) {
		*resolve_ms = 0.0f;
	}
	return NULL;
}
