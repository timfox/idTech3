/*
===========================================================================
Unit tests for VUDA analytical model.
===========================================================================
*/

#include "vuda/vuda_model.h"

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

static void test_datagen_speedup( void )
{
	vuda_phase_profile_t profile;
	vuda_model_result_t r;

	profile.sim_ms = 3.0f;
	profile.render_ms = 9.0f;
	profile.inference_ms = 0.0f;
	profile.batch_size = 128;

	VUDA_ModelDataGen( &profile, 0.85f, &r );
	expect_true( r.speedup > 1.2f, "data-gen speedup > 1.2x with overlap" );
}

static void test_graft_cheaper_than_export( void )
{
	float graft;
	float expimp;

	graft = VUDA_ModelGraftCostMs( 512 );
	expimp = VUDA_ModelExportImportCostMs( 512 );
	expect_true( expimp > graft * 5.0f, "export/import >> graft at 512 buffers" );
}

static void test_maniskill_preset_count( void )
{
	int count;

	VUDA_ManiSkillPresets( &count );
	expect_true( count >= 5, "ManiSkill presets present" );
}

int main( void )
{
	test_datagen_speedup();
	test_graft_cheaper_than_export();
	test_maniskill_preset_count();

	printf( "test_vuda_model: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
