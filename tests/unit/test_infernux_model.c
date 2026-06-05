/*
===========================================================================
Unit tests for Infernux analytical model.
===========================================================================
*/

#include "infernux/infernux_model.h"

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

static void test_spawn_100_infernux_faster_editor( void )
{
	const infernux_row_t *rows;
	int count;
	int i;

	rows = Infernux_Table( INFERNUX_BENCH_SPAWN_SINGLE, &count );
	for ( i = 0; i < count; i++ ) {
		if ( rows[i].grid_n == 100 ) {
			expect_true( rows[i].infernux_editor_fps > rows[i].unity_editor_fps * 2.0f,
				"N=100 editor: Infernux > 2x Unity (paper)" );
			return;
		}
	}
	expect_true( 0, "N=100 row present" );
}

static void test_pure_compute_jit_speedup( void )
{
	expect_true( Infernux_JitSpeedup( 1000 ) > 8.0f, "N=1000 JIT ~10x vs No-JIT" );
}

int main( void )
{
	test_spawn_100_infernux_faster_editor();
	test_pure_compute_jit_speedup();

	printf( "test_infernux_model: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
