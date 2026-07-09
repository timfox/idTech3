/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Tree Traversal Prefetcher (TTP) — software model and characterization.
Tozlu, Naithani & Zhou, arXiv:2605.16253.
===========================================================================
*/

#include "ttp.h"
#include "ttp_model.h"
#include "ttp_sim.h"

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

#include <math.h>

static cvar_t *cl_ttp;
static cvar_t *cl_ttp_mem_wait;
static cvar_t *cl_ttp_bfs_distance;
static qboolean ttp_cmds_registered = qfalse;

static void TTP_PrintResult( const char *label, const ttp_model_result_t *r )
{
	if ( !r ) {
		return;
	}
	Com_Printf( "[TTP] %s: speedup=%.2fx coverage=%.1f%% accuracy=%.1f%% prefetches=%llu useful=%llu\n",
		label,
		r->speedup,
		r->coverage * 100.0f,
		r->accuracy * 100.0f,
		(unsigned long long)r->prefetches_issued,
		(unsigned long long)r->prefetches_useful );
}

static void TTP_PrintHistogram( const ttp_pop_histogram_t *hist )
{
	if ( !hist ) {
		return;
	}
	Com_Printf( "[TTP] pops=%llu streak[1/2/3/4+]=%llu/%llu/%llu/%llu\n",
		(unsigned long long)hist->total_pops,
		(unsigned long long)hist->streak_pops[0],
		(unsigned long long)hist->streak_pops[1],
		(unsigned long long)hist->streak_pops[2],
		(unsigned long long)hist->streak_pops[3] );
}

static void TTP_Sim_f( void )
{
	int depth;
	int arity;
	int seed;
	ttp_traversal_mode_t mode;
	ttp_pop_histogram_t hist;
	ttp_model_result_t dfs;
	ttp_model_result_t bfs;
	int intensity[3];
	float mem_wait;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: ttp_sim <dfs|bfs> [depth] [arity] [seed]\n" );
		return;
	}

	mode = TTP_TRAVERSAL_DFS;
	if ( !Q_stricmp( Cmd_Argv( 1 ), "bfs" ) ) {
		mode = TTP_TRAVERSAL_BFS;
	} else if ( Q_stricmp( Cmd_Argv( 1 ), "dfs" ) ) {
		Com_Printf( S_COLOR_YELLOW "[TTP] mode must be dfs or bfs\n" );
		return;
	}

	depth = ( Cmd_Argc() >= 3 ) ? atoi( Cmd_Argv( 2 ) ) : 12;
	arity = ( Cmd_Argc() >= 4 ) ? atoi( Cmd_Argv( 3 ) ) : 6;
	seed = ( Cmd_Argc() >= 5 ) ? atoi( Cmd_Argv( 4 ) ) : 42;
	mem_wait = cl_ttp_mem_wait ? cl_ttp_mem_wait->value : 0.70f;

	TTP_SimulateTraversal( mode, depth, arity, seed, &hist );
	TTP_PrintHistogram( &hist );

	TTP_DefaultPrefetchIntensity( intensity );
	TTP_ModelDFS( &hist, intensity, mem_wait, &dfs );
	TTP_ModelBFS( &hist, cl_ttp_bfs_distance ? cl_ttp_bfs_distance->integer : 4, mem_wait, &bfs );

	if ( mode == TTP_TRAVERSAL_DFS ) {
		TTP_PrintResult( "DFS+TTP (paper FSM 1/2/16)", &dfs );
	} else {
		TTP_PrintResult( "BFS+TTP", &bfs );
	}
}

static void TTP_Lumibench_f( void )
{
	const ttp_scene_preset_t *scenes;
	int count;
	int i;
	float mem_wait;
	int intensity[3];
	int bfs_n;
	double log_speedup = 0.0;
	double sum_speedup;
	double sum_treelet;

	scenes = TTP_LumibenchPresets( &count );
	mem_wait = cl_ttp_mem_wait ? cl_ttp_mem_wait->value : 0.70f;
	bfs_n = cl_ttp_bfs_distance ? cl_ttp_bfs_distance->integer : 4;
	TTP_DefaultPrefetchIntensity( intensity );

	Com_Printf( "[TTP] Lumibench-style sweep (synthetic 6-ary trees, paper Table II depths)\n" );
	Com_Printf( "[TTP] cl_ttp_mem_wait=%.2f bfs_distance=%d\n", mem_wait, bfs_n );

	sum_speedup = 0.0;
	sum_treelet = 0.0;
	for ( i = 0; i < count; i++ ) {
		ttp_pop_histogram_t hist_dfs;
		ttp_pop_histogram_t hist_bfs;
		ttp_model_result_t dfs;
		ttp_model_result_t bfs;
		float treelet_speedup;

		TTP_SimulateTraversal( TTP_TRAVERSAL_DFS, scenes[i].tree_depth, scenes[i].arity, i + 1, &hist_dfs );
		TTP_SimulateTraversal( TTP_TRAVERSAL_BFS, scenes[i].tree_depth, scenes[i].arity, i + 1, &hist_bfs );
		TTP_ModelDFS( &hist_dfs, intensity, mem_wait, &dfs );
		TTP_ModelBFS( &hist_bfs, bfs_n, mem_wait, &bfs );

		/* Paper Fig. 23: Treelet ~1.0x average at 128x128; some scenes regress below 1.0. */
		treelet_speedup = 1.0f;
		if ( !Q_stricmp( scenes[i].label, "ship" ) || !Q_stricmp( scenes[i].label, "spnza" ) ||
			!Q_stricmp( scenes[i].label, "crnvl" ) || !Q_stricmp( scenes[i].label, "fox" ) ) {
			treelet_speedup = 0.95f;
		}

		Com_Printf( "[TTP] %-6s depth=%2d DFS=%.2fx BFS=%.2fx treelet~%.2fx\n",
			scenes[i].label,
			scenes[i].tree_depth,
			dfs.speedup,
			bfs.speedup,
			treelet_speedup );

		log_speedup += log( (double)dfs.speedup );
		sum_treelet += (double)treelet_speedup;
	}

	sum_speedup = exp( log_speedup / (double)count );
	Com_Printf( "[TTP] geometric mean DFS+TTP speedup=%.2fx (paper path tracing ~1.48x)\n", (float)sum_speedup );
	Com_Printf( "[TTP] average Treelet reference=%.2fx (paper ~1.00x at 128x128)\n",
		(float)( sum_treelet / (double)count ) );
}

static void TTP_Compare_f( void )
{
	ttp_pop_histogram_t hist_dfs;
	ttp_pop_histogram_t hist_bfs;
	ttp_model_result_t dfs;
	ttp_model_result_t bfs;
	int intensity[3];
	float mem_wait;
	int depth;
	int bfs_n;

	depth = ( Cmd_Argc() >= 2 ) ? atoi( Cmd_Argv( 1 ) ) : 14;
	mem_wait = cl_ttp_mem_wait ? cl_ttp_mem_wait->value : 0.70f;
	bfs_n = cl_ttp_bfs_distance ? cl_ttp_bfs_distance->integer : 4;
	TTP_DefaultPrefetchIntensity( intensity );

	TTP_SimulateTraversal( TTP_TRAVERSAL_DFS, depth, 6, 7, &hist_dfs );
	TTP_SimulateTraversal( TTP_TRAVERSAL_BFS, depth, 6, 7, &hist_bfs );
	TTP_ModelDFS( &hist_dfs, intensity, mem_wait, &dfs );
	TTP_ModelBFS( &hist_bfs, bfs_n, mem_wait, &bfs );

	Com_Printf( "[TTP] DFS vs BFS comparison (depth=%d, 6-ary synthetic tree)\n", depth );
	TTP_PrintHistogram( &hist_dfs );
	Com_Printf( "[TTP] DFS without prefetch: speedup=1.00x (baseline)\n" );
	TTP_PrintResult( "DFS + TTP", &dfs );
	Com_Printf( "[TTP] BFS without prefetch: slower than DFS in paper (see Table I)\n" );
	TTP_PrintResult( "BFS + TTP", &bfs );
	Com_Printf( "[TTP] Paper: BFS alone slower than DFS; with TTP, BFS can win on cache-friendly scenes\n" );
}

static void TTP_BFSSweep_f( void )
{
	ttp_pop_histogram_t hist_bfs;
	ttp_model_result_t bfs1;
	ttp_model_result_t bfs2;
	ttp_model_result_t bfs4;
	float mem_wait;
	int depth;
	int arity;
	int seed;

	depth = ( Cmd_Argc() >= 2 ) ? atoi( Cmd_Argv( 1 ) ) : 14;
	arity = ( Cmd_Argc() >= 3 ) ? atoi( Cmd_Argv( 2 ) ) : 6;
	seed = ( Cmd_Argc() >= 4 ) ? atoi( Cmd_Argv( 3 ) ) : 7;
	mem_wait = cl_ttp_mem_wait ? cl_ttp_mem_wait->value : 0.70f;

	TTP_SimulateTraversal( TTP_TRAVERSAL_BFS, depth, arity, seed, &hist_bfs );
	TTP_ModelBFS( &hist_bfs, 1, mem_wait, &bfs1 );
	TTP_ModelBFS( &hist_bfs, 2, mem_wait, &bfs2 );
	TTP_ModelBFS( &hist_bfs, 4, mem_wait, &bfs4 );

	Com_Printf( "[TTP] BFS distance sweep (depth=%d arity=%d seed=%d)\n", depth, arity, seed );
	Com_Printf( "[TTP] Paper refs at mem_wait~0.70: N=1 -> ~1.85x, N=2 -> ~2.05x, N=4 -> ~2.20x\n" );
	TTP_PrintResult( "BFS N=1", &bfs1 );
	TTP_PrintResult( "BFS N=2", &bfs2 );
	TTP_PrintResult( "BFS N=4", &bfs4 );
}

static void TTP_Status_f( void )
{
	Com_Printf( "[TTP] cl_ttp=%d cl_ttp_mem_wait=%.2f cl_ttp_bfs_distance=%d\n",
		cl_ttp ? cl_ttp->integer : 0,
		cl_ttp_mem_wait ? cl_ttp_mem_wait->value : 0.70f,
		cl_ttp_bfs_distance ? cl_ttp_bfs_distance->integer : 4 );
	Com_Printf( "[TTP] Commands: ttp_status, ttp_sim, ttp_lumibench, ttp_compare, ttp_bfs_sweep\n" );
	Com_Printf( "[TTP] Hardware RT-unit prefetch (stack pop streaks); see docs/TTP.md\n" );
}

void TTP_Init( void )
{
	cl_ttp = Cvar_Get( "cl_ttp", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_ttp,
		"Enable Tree Traversal Prefetcher (TTP) characterization commands (Tozlu et al., arXiv:2605.16253)." );

	cl_ttp_mem_wait = Cvar_Get( "cl_ttp_mem_wait", "0.70", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_ttp_mem_wait,
		"Fraction of RT-unit thread cycles waiting on BVH memory (paper Fig. 1); used by analytical speedup model." );

	cl_ttp_bfs_distance = Cvar_Get( "cl_ttp_bfs_distance", "4", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_ttp_bfs_distance,
		"BFS prefetch distance N (paper Section VI-C; default 4)." );

	if ( !cl_ttp->integer ) {
		return;
	}

	if ( !ttp_cmds_registered ) {
		Cmd_AddCommand( "ttp_status", TTP_Status_f );
		Cmd_AddCommand( "ttp_sim", TTP_Sim_f );
		Cmd_AddCommand( "ttp_lumibench", TTP_Lumibench_f );
		Cmd_AddCommand( "ttp_compare", TTP_Compare_f );
		Cmd_AddCommand( "ttp_bfs_sweep", TTP_BFSSweep_f );
		ttp_cmds_registered = qtrue;
	}

	Com_Printf( "[TTP] Tree Traversal Prefetcher model enabled (cl_ttp 1)\n" );
}
