#pragma once

#include "qcommon/q_shared.h"

/* VUDA analytical model — Xu et al., arXiv:2605.01352 */

typedef enum {
	VUDA_SCENARIO_DATA_GEN = 0,
	VUDA_SCENARIO_RL_MLP,
	VUDA_SCENARIO_RL_VLA
} vuda_scenario_t;

typedef struct {
	float sim_ms;
	float render_ms;
	float inference_ms;
	int batch_size;
} vuda_phase_profile_t;

typedef struct {
	float baseline_steps_per_s;
	float vuda_steps_per_s;
	float speedup;
	float sm_util_baseline;
	float sm_util_vuda;
} vuda_model_result_t;

void VUDA_ModelDataGen( const vuda_phase_profile_t *profile, float overlap_efficiency,
	vuda_model_result_t *out );

void VUDA_ModelRL( const vuda_phase_profile_t *profile, vuda_scenario_t scenario,
	float overlap_efficiency, vuda_model_result_t *out );

float VUDA_ModelGraftCostMs( int num_buffers_2mb );
float VUDA_ModelExportImportCostMs( int num_buffers_2mb );

typedef struct {
	const char *label;
	float sim_ms;
	float render_ms;
} vuda_maniskill_preset_t;

const vuda_maniskill_preset_t *VUDA_ManiSkillPresets( int *count );
