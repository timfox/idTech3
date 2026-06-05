/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Unit tests for Mímir analytical model (arXiv:2504.20937).
===========================================================================
*/

#include <stdio.h>
#include <stdlib.h>

#include "mimir/mimir_model.h"

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

static void test_interop_faster_than_ram( void )
{
	expect_true( Mimir_InteropFpsSpeedup( 1000000 ) >= 8.0f,
		"N=1e6 interop FPS speedup >= 8x vs RAM (paper Fig. 9)" );
	expect_true( Mimir_InteropTimeSpeedup( 1000000 ) >= 10.0f,
		"N=1e6 interop time speedup >= 10x vs RAM" );
	expect_true( Mimir_InteropVramRatio( 1000000 ) <= 0.75f,
		"N=1e6 mimir uses <= 75% RAM path VRAM (paper ~1.5x less)" );
}

static void test_benchmark_rows( void )
{
	mimir_benchmark_result_t interop;
	mimir_benchmark_result_t ram;

	Mimir_Benchmark( MIMIR_BACKEND_INTEROP, 10000, &interop );
	Mimir_Benchmark( MIMIR_BACKEND_RAM, 10000, &ram );
	expect_true( interop.fps > ram.fps, "interop FPS beats RAM at N=10k" );
	expect_true( interop.total_ms < ram.total_ms, "interop frame time beats RAM at N=10k" );
}

int main( void )
{
	test_interop_faster_than_ram();
	test_benchmark_rows();

	printf( "test_mimir_model: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
