/*
===========================================================================
Unit tests for TTP analytical model and traversal simulator.
===========================================================================
*/

#include "qcommon/ttp_model.h"
#include "qcommon/ttp_sim.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int tests_run;
static int tests_failed;

static void expect_true( int cond, const char *msg )
{
	tests_run++;
	if ( !cond ) {
		tests_failed++;
		fprintf( stderr, "FAIL: %s\n", msg );
	}
}

static void test_dfs_speedup_above_one( void )
{
	ttp_pop_histogram_t hist;
	ttp_model_result_t r;
	int intensity[3];

	TTP_SimulateTraversal( TTP_TRAVERSAL_DFS, 14, 6, 11, &hist );
	expect_true( hist.total_pops > 0u, "DFS simulation produces pops" );

	TTP_DefaultPrefetchIntensity( intensity );
	TTP_ModelDFS( &hist, intensity, 0.70f, &r );
	expect_true( r.speedup > 1.05f, "DFS+TTP speedup > 1.05x at mem_wait=0.70" );
	expect_true( r.coverage > 0.05f, "DFS+TTP coverage > 5%" );
	expect_true( r.accuracy > 0.0f && r.accuracy <= 1.0f, "DFS accuracy in [0,1]" );
}

static void test_bfs_beats_dfs_with_ttp_on_deep_tree( void )
{
	ttp_pop_histogram_t hist_dfs;
	ttp_pop_histogram_t hist_bfs;
	ttp_model_result_t dfs;
	ttp_model_result_t bfs;
	int intensity[3];

	TTP_SimulateTraversal( TTP_TRAVERSAL_DFS, 16, 6, 3, &hist_dfs );
	TTP_SimulateTraversal( TTP_TRAVERSAL_BFS, 16, 6, 3, &hist_bfs );
	TTP_DefaultPrefetchIntensity( intensity );
	TTP_ModelDFS( &hist_dfs, intensity, 0.70f, &dfs );
	TTP_ModelBFS( &hist_bfs, 4, 0.70f, &bfs );

	expect_true( bfs.speedup >= dfs.speedup, "BFS+TTP speedup >= DFS+TTP on depth-16 synthetic tree" );
}

static void test_lumibench_preset_count( void )
{
	int count;

	TTP_LumibenchPresets( &count );
	expect_true( count == 16, "16 Lumibench scene presets" );
}

int main( void )
{
	test_dfs_speedup_above_one();
	test_bfs_beats_dfs_with_ttp_on_deep_tree();
	test_lumibench_preset_count();

	printf( "test_ttp_model: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
