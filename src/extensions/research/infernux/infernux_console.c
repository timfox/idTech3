/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Infernux model console — arXiv:2604.10263.
===========================================================================
*/

#include "infernux/infernux_console.h"
#include "infernux/infernux_model.h"

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

static cvar_t *cl_infernux_model;
static qboolean infernux_console_registered = qfalse;

static infernux_bench_t Infernux_ParseBench( const char *name )
{
	if ( !name || !name[0] ) {
		return INFERNUX_BENCH_SPAWN_SINGLE;
	}
	if ( !Q_stricmp( name, "multi10" ) || !Q_stricmp( name, "m10" ) ) {
		return INFERNUX_BENCH_SPAWN_MULTI10;
	}
	if ( !Q_stricmp( name, "multi100" ) || !Q_stricmp( name, "m100" ) ) {
		return INFERNUX_BENCH_SPAWN_MULTI100;
	}
	if ( !Q_stricmp( name, "compute" ) || !Q_stricmp( name, "pure" ) ) {
		return INFERNUX_BENCH_PURE_COMPUTE;
	}
	return INFERNUX_BENCH_SPAWN_SINGLE;
}

static void Infernux_Cmd_Status_f( void )
{
	Com_Printf( "[Infernux] cl_infernux_model=%d\n", cl_infernux_model ? cl_infernux_model->integer : 0 );
	Com_Printf( "[Infernux] Commands: infernux_api, infernux_model, infernux_jit\n" );
	Com_Printf( "[Infernux] Python runtime: py_reload, py_list, py_exec (USE_PYTHON build)\n" );
	Com_Printf( "[Infernux] See docs/PYTHON.md — upstream https://chenlizheme.github.io/Infernux/\n" );
}

static void Infernux_Cmd_Api_f( void )
{
	Com_Printf( "[Infernux] Python-native layer over C/Vulkan core (engine integration):\n" );
	Com_Printf( "  Batch bridge: Engine.batch_read/write SoA columns (§VI-A)\n" );
	Com_Printf( "  JIT path: idtech3.jit.infernux_jit (Numba prange when installed)\n" );
	Com_Printf( "  Lifecycle: Engine.on('frame'|event) + py_reload scripts/python/\n" );
	Com_Printf( "[Infernux] Also available: Lua (script_reload), JS (js_reload), C# (cs_reload)\n" );
}

static void Infernux_Cmd_Model_f( void )
{
	infernux_bench_t bench;
	const infernux_row_t *rows;
	int count;
	int i;
	const char *label;

	bench = ( Cmd_Argc() >= 2 ) ? Infernux_ParseBench( Cmd_Argv( 1 ) ) : INFERNUX_BENCH_SPAWN_SINGLE;
	rows = Infernux_Table( bench, &count );

	switch ( bench ) {
	case INFERNUX_BENCH_SPAWN_MULTI10:
		label = "SpawnCube M=10";
		break;
	case INFERNUX_BENCH_SPAWN_MULTI100:
		label = "SpawnCube M=100";
		break;
	case INFERNUX_BENCH_PURE_COMPUTE:
		label = "Pure compute (Table IV)";
		break;
	default:
		label = "SpawnCube single material (Table I)";
		break;
	}

	Com_Printf( "[Infernux] %s — Infernux vs Unity 6 IL2CPP (paper)\n", label );
	for ( i = 0; i < count; i++ ) {
		Com_Printf( "[Infernux] N=%3d  Infernux ed=%.0f rt=%.0f  Unity ed=%.0f rt=%.0f\n",
			rows[i].grid_n,
			rows[i].infernux_editor_fps, rows[i].infernux_runtime_fps,
			rows[i].unity_editor_fps, rows[i].unity_runtime_fps );
	}
}

static void Infernux_Cmd_Jit_f( void )
{
	int n;

	n = ( Cmd_Argc() >= 2 ) ? atoi( Cmd_Argv( 1 ) ) : 1000;
	Com_Printf( "[Infernux] Estimated JIT speedup vs plain NumPy at N=%d: %.1fx (Table IV)\n",
		n, Infernux_JitSpeedup( n ) );
	Com_Printf( "[Infernux] Install numba for runtime JIT: pip install numba\n" );
}

void Infernux_ConsoleInit( void )
{
	cl_infernux_model = Cvar_Get( "cl_infernux_model", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_infernux_model,
		"Enable Infernux Python/JIT benchmark commands (Chen, arXiv:2604.10263)." );

	if ( !cl_infernux_model->integer ) {
		return;
	}

	if ( !infernux_console_registered ) {
		Cmd_AddCommand( "infernux_model_status", Infernux_Cmd_Status_f );
		Cmd_AddCommand( "infernux_api", Infernux_Cmd_Api_f );
		Cmd_AddCommand( "infernux_model", Infernux_Cmd_Model_f );
		Cmd_AddCommand( "infernux_jit", Infernux_Cmd_Jit_f );
		infernux_console_registered = qtrue;
	}

	Com_Printf( "[Infernux] Model commands enabled (cl_infernux_model 1)\n" );
}
