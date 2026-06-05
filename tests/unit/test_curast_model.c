/*
===========================================================================
Unit tests for CuRast analytical model.
===========================================================================
*/

#include "curast/curast_model.h"

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

static void test_zorah_faster_than_vk_pip( void )
{
	curast_model_result_t r;

	CuRast_ModelBenchmark( CURAST_SCENE_ZORAH, CURAST_GPU_4090, &r );
	expect_true( r.speedup_vs_vk_pip > 10.0f, "Zorah CuRast >> VK-PIP on 4090" );
	expect_true( r.row.curast_ms < 100.0f, "Zorah CuRast under 100ms (Table 2)" );
}

static void test_sponza_vulkan_wins( void )
{
	curast_model_result_t r;

	CuRast_ModelBenchmark( CURAST_SCENE_SPONZA, CURAST_GPU_4090, &r );
	expect_true( r.speedup_vs_vk_id < 1.0f, "Sponza: Vulkan indexed faster than CuRast" );
}

static void test_lantern_inst_speedup( void )
{
	curast_model_result_t r;

	CuRast_ModelBenchmark( CURAST_SCENE_LANTERN_INST, CURAST_GPU_5090, &r );
	expect_true( r.speedup_vs_vk_id > 10.0f, "Lantern instanced: large CuRast win vs VK-ID" );
}

int main( void )
{
	test_zorah_faster_than_vk_pip();
	test_sponza_vulkan_wins();
	test_lantern_inst_speedup();

	printf( "test_curast_model: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
