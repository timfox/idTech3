/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CuRast console — Schütz et al., arXiv:2604.21749.
===========================================================================
*/

#include "curast/curast_console.h"
#include "curast/curast_model.h"

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

static cvar_t *cl_curast_model;
static qboolean curast_console_registered = qfalse;

static curast_scene_t Curast_ParseScene( const char *name )
{
	if ( !name || !name[0] ) {
		return CURAST_SCENE_ZORAH;
	}
	if ( !Q_stricmp( name, "sponza" ) ) {
		return CURAST_SCENE_SPONZA;
	}
	if ( !Q_stricmp( name, "lantern" ) ) {
		return CURAST_SCENE_LANTERN;
	}
	if ( !Q_stricmp( name, "lantern_inst" ) || !Q_stricmp( name, "instances" ) ) {
		return CURAST_SCENE_LANTERN_INST;
	}
	if ( !Q_stricmp( name, "komainu" ) ) {
		return CURAST_SCENE_KOMAINU;
	}
	if ( !Q_stricmp( name, "venice" ) ) {
		return CURAST_SCENE_VENICE;
	}
	return CURAST_SCENE_ZORAH;
}

static curast_gpu_t Curast_ParseGpu( const char *name )
{
	if ( !name || !name[0] ) {
		return CURAST_GPU_4090;
	}
	if ( !Q_stricmp( name, "4070" ) || !Q_stricmp( name, "rtx4070" ) ) {
		return CURAST_GPU_4070;
	}
	if ( !Q_stricmp( name, "5090" ) || !Q_stricmp( name, "rtx5090" ) ) {
		return CURAST_GPU_5090;
	}
	return CURAST_GPU_4090;
}

static void Curast_Cmd_ModelStatus_f( void )
{
	Com_Printf( "[CuRast] cl_curast_model=%d\n", cl_curast_model ? cl_curast_model->integer : 0 );
	Com_Printf( "[CuRast] Commands: curast_api, curast_model, curast_stages\n" );
	Com_Printf( "[CuRast] Runtime (renderer): curast_status, curast_render, curast_partition, curast_reset\n" );
	Com_Printf( "[CuRast] See docs/CURAST.md — upstream https://github.com/m-schuetz/CuRast\n" );
}

static void Curast_Cmd_Api_f( void )
{
	Com_Printf( "[CuRast] 3-stage software rasterizer (CUDA in paper; Vulkan compute scaffold here):\n" );
	Com_Printf( "  Stage 1: 1 thread/triangle, bbox raster, atomicMin visibility buffer (<128 px)\n" );
	Com_Printf( "  Stage 2: 32 threads/triangle for medium tris (<4096 px) + near-plane queue\n" );
	Com_Printf( "  Stage 3: 64 threads per 64x64 tile for large triangles (world-space ray hit)\n" );
	Com_Printf( "  Resolve: deferred shading from 28-bit depth + 36-bit global triangle index\n" );
	Com_Printf( "[CuRast] Engine: r_curast 1 + vid_restart; curast_render and curast_partition\n" );
}

static void Curast_Cmd_Model_f( void )
{
	curast_model_result_t r;
	curast_scene_t scene;
	curast_gpu_t gpu;

	scene = ( Cmd_Argc() >= 2 ) ? Curast_ParseScene( Cmd_Argv( 1 ) ) : CURAST_SCENE_ZORAH;
	gpu = ( Cmd_Argc() >= 3 ) ? Curast_ParseGpu( Cmd_Argv( 2 ) ) : CURAST_GPU_4090;

	CuRast_ModelBenchmark( scene, gpu, &r );

	Com_Printf( "[CuRast] %s on %s (Table 2, visible ~%.1fM tris)\n",
		r.row.label, CuRast_GpuName( gpu ), r.row.visible_tris_m );
	Com_Printf( "[CuRast] CuRast: %.3f ms\n", r.row.curast_ms );
	if ( r.row.vk_id_ms > 0.0f ) {
		Com_Printf( "[CuRast] VK-ID:  %.3f ms (CuRast %.2fx %s)\n",
			r.row.vk_id_ms, r.speedup_vs_vk_id,
			r.speedup_vs_vk_id >= 1.0f ? "faster" : "slower" );
	} else {
		Com_Printf( "[CuRast] VK-ID:  n/a (OOM/unsupported config in paper)\n" );
	}
	if ( r.row.vk_pip_ms > 0.0f ) {
		Com_Printf( "[CuRast] VK-PIP: %.3f ms (CuRast %.2fx faster)\n",
			r.row.vk_pip_ms, r.speedup_vs_vk_pip );
	}
}

static void Curast_Cmd_Stages_f( void )
{
	curast_scene_t scene;
	float s1, s2, s3, resolve;
	const char *name;

	scene = ( Cmd_Argc() >= 2 ) ? Curast_ParseScene( Cmd_Argv( 1 ) ) : CURAST_SCENE_ZORAH;
	name = CuRast_StageBreakdown( scene, &s1, &s2, &s3, &resolve );

	if ( !name ) {
		Com_Printf( "[CuRast] No Table 3 breakdown for scene '%s'\n", CuRast_SceneName( scene ) );
		return;
	}

	Com_Printf( "[CuRast] Stage timings for %s (RTX 4090, Table 3)\n", name );
	Com_Printf( "[CuRast] Stage1=%.3f ms Stage2=%.3f ms Stage3=%.3f ms Resolve=%.3f ms Total=%.3f ms\n",
		s1, s2, s3, resolve, s1 + s2 + s3 + resolve );
}

void Curast_ConsoleInit( void )
{
	cl_curast_model = Cvar_Get( "cl_curast_model", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_curast_model,
		"Enable CuRast software rasterization benchmark commands (Schütz et al., arXiv:2604.21749)." );

	if ( !cl_curast_model->integer ) {
		return;
	}

	if ( !curast_console_registered ) {
		Cmd_AddCommand( "curast_model_status", Curast_Cmd_ModelStatus_f );
		Cmd_AddCommand( "curast_api", Curast_Cmd_Api_f );
		Cmd_AddCommand( "curast_model", Curast_Cmd_Model_f );
		Cmd_AddCommand( "curast_stages", Curast_Cmd_Stages_f );
		curast_console_registered = qtrue;
	}

	Com_Printf( "[CuRast] Model commands enabled (cl_curast_model 1)\n" );
}
