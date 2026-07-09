/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

VUDA console: paper API documentation + analytical model (always available).
===========================================================================
*/

#include "vuda/vuda_console.h"
#include "vuda/vuda_model.h"

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

#include <math.h>

static cvar_t *cl_vuda_model;
static cvar_t *cl_vuda_overlap;
static qboolean vuda_console_registered = qfalse;

static void Vuda_PrintResult( const char *label, const vuda_model_result_t *r )
{
	if ( !r ) {
		return;
	}
	Com_Printf( "[VUDA] %s: %.2f k steps/s (speedup %.2fx) SM util baseline=%.0f%% vuda=%.0f%%\n",
		label,
		r->vuda_steps_per_s / 1000.0f,
		r->speedup,
		r->sm_util_baseline * 100.0f,
		r->sm_util_vuda * 100.0f );
}

static void Vuda_Cmd_ModelDataGen_f( void )
{
	vuda_phase_profile_t profile;
	vuda_model_result_t base;
	vuda_model_result_t vuda;
	float overlap;

	profile.sim_ms = ( Cmd_Argc() >= 2 ) ? (float)atof( Cmd_Argv( 1 ) ) : 3.0f;
	profile.render_ms = ( Cmd_Argc() >= 3 ) ? (float)atof( Cmd_Argv( 2 ) ) : 9.0f;
	profile.inference_ms = 0.0f;
	profile.batch_size = ( Cmd_Argc() >= 4 ) ? atoi( Cmd_Argv( 3 ) ) : 128;
	overlap = cl_vuda_overlap ? cl_vuda_overlap->value : 0.85f;

	VUDA_ModelDataGen( &profile, 0.0f, &base );
	VUDA_ModelDataGen( &profile, overlap, &vuda );

	Com_Printf( "[VUDA] Data generation model (inter-step sim/render overlap)\n" );
	Com_Printf( "[VUDA] phases sim=%.1fms render=%.1fms batch=%d overlap=%.2f\n",
		profile.sim_ms, profile.render_ms, profile.batch_size, overlap );
	Vuda_PrintResult( "baseline (temporal)", &base );
	Vuda_PrintResult( "VUDA spatial", &vuda );
}

static void Vuda_Cmd_ModelRL_f( void )
{
	vuda_phase_profile_t profile;
	vuda_model_result_t vuda;
	float overlap;
	vuda_scenario_t scenario;

	scenario = VUDA_SCENARIO_RL_MLP;
	if ( Cmd_Argc() >= 2 && !Q_stricmp( Cmd_Argv( 1 ), "vla" ) ) {
		scenario = VUDA_SCENARIO_RL_VLA;
	}

	profile.inference_ms = ( Cmd_Argc() >= 3 ) ? (float)atof( Cmd_Argv( 2 ) ) : 4.0f;
	profile.sim_ms = ( Cmd_Argc() >= 4 ) ? (float)atof( Cmd_Argv( 3 ) ) : 3.5f;
	profile.render_ms = ( Cmd_Argc() >= 5 ) ? (float)atof( Cmd_Argv( 4 ) ) : 3.0f;
	profile.batch_size = ( Cmd_Argc() >= 6 ) ? atoi( Cmd_Argv( 5 ) ) : 256;
	overlap = cl_vuda_overlap ? cl_vuda_overlap->value : 0.85f;

	VUDA_ModelRL( &profile, scenario, overlap, &vuda );
	Com_Printf( "[VUDA] RL rollout model (%s, inter-trajectory overlap)\n",
		scenario == VUDA_SCENARIO_RL_VLA ? "VLA disaggregated" : "MLP single-GPU" );
	Com_Printf( "[VUDA] inference=%.1f sim=%.1f render=%.1f batch=%d\n",
		profile.inference_ms, profile.sim_ms, profile.render_ms, profile.batch_size );
	Vuda_PrintResult( "VUDA vs baseline", &vuda );
}

static void Vuda_Cmd_ModelGraft_f( void )
{
	int n;
	float graft;
	float expimp;

	n = ( Cmd_Argc() >= 2 ) ? atoi( Cmd_Argv( 1 ) ) : 128;
	if ( n < 1 ) {
		n = 1;
	}

	graft = VUDA_ModelGraftCostMs( n );
	expimp = VUDA_ModelExportImportCostMs( n );

	Com_Printf( "[VUDA] Memory sharing cost for %d x 2MiB buffers (paper Fig. 6)\n", n );
	Com_Printf( "[VUDA]   page-table graft: %.2f ms\n", graft );
	Com_Printf( "[VUDA]   export/import:    %.2f ms (ratio %.1fx)\n",
		expimp, ( graft > 0.0f ) ? expimp / graft : 0.0f );
}

static void Vuda_Cmd_ManiSkill_f( void )
{
	const vuda_maniskill_preset_t *scenes;
	int count;
	int i;
	float overlap;
	double log_speedup;
	int batch;

	scenes = VUDA_ManiSkillPresets( &count );
	overlap = cl_vuda_overlap ? cl_vuda_overlap->value : 0.85f;
	batch = ( Cmd_Argc() >= 2 ) ? atoi( Cmd_Argv( 1 ) ) : 128;
	log_speedup = 0.0;

	Com_Printf( "[VUDA] ManiSkill-style sweep (batch=%d, overlap=%.2f)\n", batch, overlap );

	for ( i = 0; i < count; i++ ) {
		vuda_phase_profile_t profile;
		vuda_model_result_t r;

		profile.sim_ms = scenes[i].sim_ms;
		profile.render_ms = scenes[i].render_ms;
		profile.inference_ms = 0.0f;
		profile.batch_size = batch;

		if ( i < 5 ) {
			VUDA_ModelDataGen( &profile, overlap, &r );
		} else {
			profile.inference_ms = 2.0f;
			VUDA_ModelRL( &profile, VUDA_SCENARIO_RL_MLP, overlap, &r );
		}

		Com_Printf( "[VUDA] %-16s speedup=%.2fx\n", scenes[i].label, r.speedup );
		log_speedup += log( (double)r.speedup );
	}

	Com_Printf( "[VUDA] geometric mean speedup=%.2fx (paper data-gen peak ~1.80x)\n",
		(float)exp( log_speedup / (double)count ) );
}

static void Vuda_Cmd_Api_f( void )
{
	Com_Printf( "[VUDA] Paper programming interface (Table 1, arXiv:2605.01352):\n" );
	Com_Printf( "  CUstream_bind(s)     — redirect CUDA stream into Vulkan TSG\n" );
	Com_Printf( "  CUstream_unbind(s)   — restore native CUDA channel\n" );
	Com_Printf( "  step_async()         — async physics simulation\n" );
	Com_Printf( "  wait_step()          — block until simulation completes\n" );
	Com_Printf( "  render_async()       — async Vulkan rendering\n" );
	Com_Printf( "  wait_render()        — block until rendering completes\n" );
	Com_Printf( "[VUDA] Engine mapping: vuda_bind_stream, vuda_step_async, vuda_wait_step, vuda_wait_render\n" );
	Com_Printf( "[VUDA] Runtime setup: r_vuda 1, cl_vuda 1, r_vuda_coStreamMask, build with vuda flag\n" );
	Com_Printf( "[VUDA] Driver channel redirection / page-table graft: see docs/VUDA.md\n" );
}

static void Vuda_Cmd_ModelStatus_f( void )
{
	Com_Printf( "[VUDA] cl_vuda_model=%d cl_vuda_overlap=%.2f\n",
		cl_vuda_model ? cl_vuda_model->integer : 0,
		cl_vuda_overlap ? cl_vuda_overlap->value : 0.85f );
	Com_Printf( "[VUDA] Commands: vuda_api, vuda_model_datagen, vuda_model_rl, vuda_model_graft, vuda_maniskill\n" );
	Com_Printf( "[VUDA] Runtime (USE_VUDA build): vuda_status, vuda_reload, vuda_run, vuda_bind_stream\n" );
	Com_Printf( "[VUDA] Paper-style runtime: vuda_step_async, vuda_wait_step, vuda_wait_render, vuda_unbind_stream\n" );
}

void Vuda_ConsoleInit( void )
{
	cl_vuda_model = Cvar_Get( "cl_vuda_model", "1", CVAR_ARCHIVE_ND );
	cl_vuda_overlap = Cvar_Get( "cl_vuda_overlap", "0.85", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_vuda_model,
		"Enable VUDA analytical model console commands (Xu et al., arXiv:2605.01352)." );
	Cvar_SetDescription( cl_vuda_overlap,
		"Estimated spatial overlap efficiency for VUDA throughput model (0–1)." );

	if ( !cl_vuda_model->integer ) {
		return;
	}

	if ( !vuda_console_registered ) {
		Cmd_AddCommand( "vuda_model_status", Vuda_Cmd_ModelStatus_f );
		Cmd_AddCommand( "vuda_api", Vuda_Cmd_Api_f );
		Cmd_AddCommand( "vuda_model_datagen", Vuda_Cmd_ModelDataGen_f );
		Cmd_AddCommand( "vuda_model_rl", Vuda_Cmd_ModelRL_f );
		Cmd_AddCommand( "vuda_model_graft", Vuda_Cmd_ModelGraft_f );
		Cmd_AddCommand( "vuda_maniskill", Vuda_Cmd_ManiSkill_f );
		vuda_console_registered = qtrue;
	}

	Com_Printf( "[VUDA] Model commands enabled (cl_vuda_model 1)\n" );
}
