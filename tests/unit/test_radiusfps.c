/*
===========================================================================
Unit tests for RadiusFPS exactness vs reference FPS.
===========================================================================
*/

#include "radiusfps/radiusfps.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void fill_dragon_like( float *pts, int n )
{
	int i;
	for ( i = 0; i < n; i++ ) {
		const float t = (float)i / (float)n;
		pts[i * 3 + 0] = 2.0f * t - 1.0f;
		pts[i * 3 + 1] = 0.25f * (float)sin( (double)t * 31.0 );
		pts[i * 3 + 2] = 0.25f * (float)cos( (double)t * 17.0 );
	}
}

static void test_matches_reference( radiusfps_backend_t backend, const char *label )
{
	const int n = 512;
	const int m = 64;
	float pts[512 * 3];
	int ref[64];
	int test[64];
	radiusfps_config_t cfg;
	int i;

	fill_dragon_like( pts, n );
	RadiusFPS_DefaultConfig( &cfg );
	cfg.backend = RADIUSFPS_BACKEND_REFERENCE;
	cfg.seed = 42u;
	cfg.nvox = 16;

	expect_true( RadiusFPS_Sample( pts, n, m, ref, &cfg, NULL, NULL ), "reference ok" );

	cfg.backend = backend;
	expect_true( RadiusFPS_Sample( pts, n, m, test, &cfg, NULL, NULL ), label );

	for ( i = 0; i < m; i++ ) {
		if ( ref[i] != test[i] ) {
			char msg[128];
			snprintf( msg, sizeof( msg ), "%s mismatch at %d (%d vs %d)", label, i, ref[i], test[i] );
			expect_true( 0, msg );
			return;
		}
	}
	expect_true( 1, label );
}

static void test_ablation_radius_only( void )
{
	const int n = 256;
	const int m = 32;
	float pts[256 * 3];
	int ref[32];
	int prune[32];
	radiusfps_config_t cfg;
	int i;

	fill_dragon_like( pts, n );
	RadiusFPS_DefaultConfig( &cfg );
	cfg.seed = 7u;
	cfg.nvox = 8;
	cfg.backend = RADIUSFPS_BACKEND_CPU;
	cfg.radius_prune = qfalse;
	cfg.point_skip = qfalse;
	expect_true( RadiusFPS_Sample( pts, n, m, ref, &cfg, NULL, NULL ), "full cpu" );

	cfg.radius_prune = qtrue;
	cfg.point_skip = qfalse;
	expect_true( RadiusFPS_Sample( pts, n, m, prune, &cfg, NULL, NULL ), "radius only" );

	for ( i = 0; i < m; i++ ) {
		expect_true( ref[i] == prune[i], "radius prune exact" );
	}
}

static void test_workspace_reuse( void )
{
	const int n = 128;
	const int m = 16;
	float pts[128 * 3];
	int a[16];
	int b[16];
	radiusfps_config_t cfg;
	radiusfps_workspace_t *ws = NULL;
	int i;

	fill_dragon_like( pts, n );
	RadiusFPS_DefaultConfig( &cfg );
	cfg.seed = 3u;
	cfg.backend = RADIUSFPS_BACKEND_CPU;

	expect_true( RadiusFPS_BuildWorkspace( pts, n, &cfg, &ws ), "build ws" );
	expect_true( RadiusFPS_Sample( pts, n, m, a, &cfg, ws, NULL ), "sample a" );
	expect_true( RadiusFPS_Sample( pts, n, m, b, &cfg, ws, NULL ), "sample b" );
	for ( i = 0; i < m; i++ ) {
		expect_true( a[i] == b[i], "workspace deterministic" );
	}
	RadiusFPS_FreeWorkspace( ws );
}

int main( void )
{
	test_matches_reference( RADIUSFPS_BACKEND_CPU, "cpu matches reference" );
#ifdef RADIUSFPS_HAVE_CUDA
	if ( RadiusFPS_GpuAvailable() ) {
		test_matches_reference( RADIUSFPS_BACKEND_GPU, "gpu matches reference" );
	}
#endif
	test_ablation_radius_only();
	test_workspace_reuse();

	printf( "test_radiusfps: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
