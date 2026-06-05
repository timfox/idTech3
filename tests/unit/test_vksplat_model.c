/*
===========================================================================
Unit tests for VkSplat analytical model.
===========================================================================
*/

#include "vksplat/vksplat_model.h"

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

static void test_default_speedup( void )
{
	vksplat_model_result_t r;

	VKSplat_ModelBenchmark( VKSPLAT_DENSIFY_DEFAULT, &r );
	expect_true( r.speedup > 3.0f && r.speedup < 3.6f, "default speedup ~3.3x" );
	expect_true( r.vram_ratio < 0.70f, "VRAM reduction vs GSplat" );
}

static void test_mcmc_faster( void )
{
	vksplat_model_result_t def;
	vksplat_model_result_t mcmc;

	VKSplat_ModelBenchmark( VKSPLAT_DENSIFY_DEFAULT, &def );
	VKSplat_ModelBenchmark( VKSPLAT_DENSIFY_MCMC, &mcmc );
	expect_true( mcmc.vksplat.total_s < def.vksplat.total_s, "MCMC VkSplat faster than default" );
}

int main( void )
{
	test_default_speedup();
	test_mcmc_faster();

	printf( "test_vksplat_model: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
