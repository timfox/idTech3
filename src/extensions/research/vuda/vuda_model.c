/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

VUDA throughput / memory-sharing model (Xu et al., arXiv:2605.01352).
===========================================================================
*/

#include "vuda/vuda_model.h"

#include <math.h>

static float VUDA_Clampf( float v, float lo, float hi )
{
	if ( v < lo ) {
		return lo;
	}
	if ( v > hi ) {
		return hi;
	}
	return v;
}

void VUDA_ModelDataGen( const vuda_phase_profile_t *profile, float overlap_efficiency,
	vuda_model_result_t *out )
{
	float step_ms;
	float pipelined_ms;
	float eff;

	if ( !out ) {
		return;
	}

	out->baseline_steps_per_s = 0.0f;
	out->vuda_steps_per_s = 0.0f;
	out->speedup = 1.0f;
	out->sm_util_baseline = 0.0f;
	out->sm_util_vuda = 0.0f;

	if ( !profile || profile->sim_ms <= 0.0f || profile->render_ms <= 0.0f ) {
		return;
	}

	eff = VUDA_Clampf( overlap_efficiency, 0.0f, 1.0f );
	step_ms = profile->sim_ms + profile->render_ms;
	pipelined_ms = profile->sim_ms + profile->render_ms -
		eff * ( profile->sim_ms < profile->render_ms ? profile->sim_ms : profile->render_ms );

	if ( pipelined_ms < 0.5f ) {
		pipelined_ms = 0.5f;
	}

	out->baseline_steps_per_s = 1000.0f / step_ms;
	out->vuda_steps_per_s = 1000.0f / pipelined_ms;
	out->speedup = out->vuda_steps_per_s / out->baseline_steps_per_s;

	/* Paper Fig. 1 / 8: sim ~10% SM util, render ~70%; overlap stabilizes ~70–80%. */
	out->sm_util_baseline =
		( 0.10f * profile->sim_ms + 0.70f * profile->render_ms ) / step_ms;
	out->sm_util_vuda = out->sm_util_baseline +
		eff * ( 0.70f - 0.10f ) * ( profile->sim_ms / step_ms );
	if ( out->sm_util_vuda > 0.85f ) {
		out->sm_util_vuda = 0.85f;
	}
	(void)profile->batch_size;
}

void VUDA_ModelRL( const vuda_phase_profile_t *profile, vuda_scenario_t scenario,
	float overlap_efficiency, vuda_model_result_t *out )
{
	float step_ms;
	float pipelined_ms;
	float eff;
	float overlap_scale;

	if ( !out ) {
		return;
	}

	out->baseline_steps_per_s = 0.0f;
	out->vuda_steps_per_s = 0.0f;
	out->speedup = 1.0f;
	out->sm_util_baseline = 0.0f;
	out->sm_util_vuda = 0.0f;

	if ( !profile || profile->sim_ms <= 0.0f || profile->render_ms <= 0.0f ) {
		return;
	}

	eff = VUDA_Clampf( overlap_efficiency, 0.0f, 1.0f );
	step_ms = profile->inference_ms + profile->sim_ms + profile->render_ms;

	/* Inter-trajectory overlap: sim batch A with render batch B (paper §3.1). */
	overlap_scale = ( profile->batch_size >= 256 ) ? 0.95f :
		( profile->batch_size >= 128 ) ? 0.90f :
		( profile->batch_size >= 64 ) ? 0.80f : 0.65f;

	pipelined_ms = step_ms - eff * overlap_scale *
		( profile->sim_ms < profile->render_ms ? profile->sim_ms : profile->render_ms );

	if ( scenario == VUDA_SCENARIO_RL_VLA ) {
		/* Inference on separate GPU — VUDA only accelerates sim GPU (paper §6.4.2). */
		pipelined_ms = profile->inference_ms +
			( profile->sim_ms + profile->render_ms -
				eff * 0.75f * ( profile->sim_ms < profile->render_ms ?
					profile->sim_ms : profile->render_ms ) );
	}

	if ( pipelined_ms < 0.5f ) {
		pipelined_ms = 0.5f;
	}

	out->baseline_steps_per_s = 1000.0f / step_ms;
	out->vuda_steps_per_s = 1000.0f / pipelined_ms;
	out->speedup = out->vuda_steps_per_s / out->baseline_steps_per_s;
	out->sm_util_baseline = 0.35f;
	out->sm_util_vuda = 0.35f + eff * overlap_scale * 0.30f;
}

float VUDA_ModelGraftCostMs( int num_buffers_2mb )
{
	float cost;

	if ( num_buffers_2mb <= 0 ) {
		return 0.0f;
	}

	/* Paper Fig. 6: ~6 ms base graft + modest rise beyond 128 buffers. */
	cost = 6.0f;
	if ( num_buffers_2mb > 128 ) {
		cost += (float)( num_buffers_2mb - 128 ) * 0.002f;
	}
	return cost;
}

float VUDA_ModelExportImportCostMs( int num_buffers_2mb )
{
	/* Per-buffer export+import ioctl cost grows linearly (paper Fig. 6). */
	if ( num_buffers_2mb <= 0 ) {
		return 0.0f;
	}
	return 0.25f * (float)num_buffers_2mb;
}

static const vuda_maniskill_preset_t maniskill_presets[] = {
	{ "PushCube",       2.5f, 8.0f },
	{ "PickCube",       2.8f, 7.5f },
	{ "LiftPegUpright", 3.0f, 9.0f },
	{ "PokeCube",       2.6f, 8.5f },
	{ "PushT",          3.2f, 10.0f },
	{ "AntRun",         4.0f, 3.5f },
	{ "HopperHop",      5.0f, 4.0f },
	{ "StackCube",      3.5f, 5.5f },
	{ "HumanoidRun",    8.0f, 4.0f }
};

const vuda_maniskill_preset_t *VUDA_ManiSkillPresets( int *count )
{
	if ( count ) {
		*count = (int)( sizeof( maniskill_presets ) / sizeof( maniskill_presets[0] ) );
	}
	return maniskill_presets;
}
