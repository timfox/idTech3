/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Mímir console — Carter, Hitschfeld & Navarro, arXiv:2504.20937.
===========================================================================
*/

#include "mimir/mimir_console.h"
#include "mimir/mimir_model.h"

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

static cvar_t *cl_mimir_model;
static qboolean mimir_console_registered = qfalse;

static mimir_backend_t Mimir_ParseBackend( const char *name )
{
	if ( !name || !name[0] ) {
		return MIMIR_BACKEND_INTEROP;
	}
	if ( !Q_stricmp( name, "ram" ) || !Q_stricmp( name, "host" ) ) {
		return MIMIR_BACKEND_RAM;
	}
	if ( !Q_stricmp( name, "opengl" ) || !Q_stricmp( name, "ogl" ) ) {
		return MIMIR_BACKEND_OPENGL;
	}
	return MIMIR_BACKEND_INTEROP;
}

static void Mimir_Cmd_ModelStatus_f( void )
{
	Com_Printf( "[Mímir] cl_mimir_model=%d\n", cl_mimir_model ? cl_mimir_model->integer : 0 );
	Com_Printf( "[Mímir] Commands: mimir_api, mimir_model, mimir_interop, mimir_sync\n" );
	Com_Printf( "[Mímir] Runtime (renderer): mimir_status, mimir_step, mimir_reset, mimir_prepare, mimir_update\n" );
	Com_Printf( "[Mímir] See docs/MIMIR.md — paper https://arxiv.org/abs/2504.20937\n" );
}

static void Mimir_Cmd_Api_f( void )
{
	Com_Printf( "[Mímir] CUDA/Vulkan interop visualization library (C++ upstream):\n" );
	Com_Printf( "  allocLinear → shared VRAM; createView (Markers/Lines/Voxels)\n" );
	Com_Printf( "  display / displayAsync + prepareViews / updateViews sync\n" );
	Com_Printf( "  Slang → SPIR-V shaders; zero-copy read of simulation state\n" );
	Com_Printf( "[Mímir] Engine scaffold: r_mimir 1 + vid_restart; mimir_step N\n" );
	Com_Printf( "[Mímir] Brownian point cloud on Vulkan compute (CUDA import when USE_MIMIR_CUDA)\n" );
}

static void Mimir_Cmd_Model_f( void )
{
	mimir_benchmark_result_t interop;
	mimir_benchmark_result_t ram;
	int n;
	int i;
	int count;
	const mimir_interop_row_t *rows;

	n = ( Cmd_Argc() >= 2 ) ? atoi( Cmd_Argv( 1 ) ) : 100000;

	Mimir_Benchmark( MIMIR_BACKEND_INTEROP, n, &interop );
	Mimir_Benchmark( MIMIR_BACKEND_RAM, n, &ram );

	Com_Printf( "[Mímir] Benchmark N=%d (paper Fig. 9, RTX 2070 SUPER FHD):\n", n );
	Com_Printf( "  interop: %.0f FPS, %.2f ms/frame, %.0f MiB VRAM\n",
		interop.fps, interop.total_ms, interop.gpu_mem_mib );
	Com_Printf( "  ram:     %.0f FPS, %.2f ms/frame, %.0f MiB VRAM\n",
		ram.fps, ram.total_ms, ram.gpu_mem_mib );
	Com_Printf( "  speedup: %.1fx FPS, %.1fx total time, %.2fx VRAM (mimir/ram)\n",
		Mimir_InteropFpsSpeedup( n ), Mimir_InteropTimeSpeedup( n ), Mimir_InteropVramRatio( n ) );

	rows = Mimir_InteropTable( &count );
	Com_Printf( "[Mímir] Full interop table (%d rows):\n", count );
	for ( i = 0; i < count; i++ ) {
		Com_Printf( "  N=%7d  mimir=%6.0f  ram=%5.0f  ogl=%5.0f  FPS  mem %.0f/%.0f MiB\n",
			rows[i].point_count,
			rows[i].mimir_fps, rows[i].ram_fps, rows[i].opengl_fps,
			rows[i].gpu_mem_mimir_mib, rows[i].gpu_mem_ram_mib );
	}
}

static void Mimir_Cmd_Interop_f( void )
{
	mimir_backend_t backend;
	mimir_benchmark_result_t r;
	int n;

	n = ( Cmd_Argc() >= 2 ) ? atoi( Cmd_Argv( 1 ) ) : 1000000;
	backend = ( Cmd_Argc() >= 3 ) ? Mimir_ParseBackend( Cmd_Argv( 2 ) ) : MIMIR_BACKEND_INTEROP;

	Mimir_Benchmark( backend, n, &r );
	Com_Printf( "[Mímir] %s N=%d: %.0f FPS, %.2f ms/frame, %.0f MiB\n",
		Mimir_BackendName( backend ), n, r.fps, r.total_ms, r.gpu_mem_mib );
}

static void Mimir_Cmd_Sync_f( void )
{
	const mimir_sync_row_t *rows;
	int count;
	int i;

	rows = Mimir_SyncTable( &count );
	Com_Printf( "[Mímir] Sync vs async (paper Fig. 6, N=1e6):\n" );
	for ( i = 0; i < count; i++ ) {
		Com_Printf( "  %s target=%3d  sync=%5.0f  async=%5.0f FPS\n",
			Mimir_ResName( rows[i].res ), rows[i].target_fps,
			rows[i].fps_sync_on, rows[i].fps_sync_off );
	}
	Com_Printf( "[Mímir] r_mimir_sync 1 = prepareViews/updateViews pattern (engine)\n" );
}

void Mimir_ConsoleInit( void )
{
	cl_mimir_model = Cvar_Get( "cl_mimir_model", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_mimir_model,
		"Enable Mímir paper benchmark console commands (arXiv:2504.20937)." );

	if ( !cl_mimir_model->integer ) {
		return;
	}

	if ( !mimir_console_registered ) {
		Cmd_AddCommand( "mimir_model_status", Mimir_Cmd_ModelStatus_f );
		Cmd_AddCommand( "mimir_api", Mimir_Cmd_Api_f );
		Cmd_AddCommand( "mimir_model", Mimir_Cmd_Model_f );
		Cmd_AddCommand( "mimir_interop", Mimir_Cmd_Interop_f );
		Cmd_AddCommand( "mimir_sync", Mimir_Cmd_Sync_f );
		mimir_console_registered = qtrue;
	}

	Com_Printf( "[Mímir] Model commands enabled (cl_mimir_model 1)\n" );
}
