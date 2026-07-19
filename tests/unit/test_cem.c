/*
===========================================================================
Unit tests for CEM scaffold (Xie et al. arXiv:2508.04076) + G_I/G_II.
===========================================================================
*/

#include "cem/cem.h"

#include <math.h>
#include <stdio.h>
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

static void test_constants( void )
{
	expect_true( fabsf( Cem_KalthoffGc() - 2.213e4f ) < 1.0f, "Kalthoff Gc" );
	expect_true( fabsf( Cem_BranchingPlateGc() - 3.0f ) < 1e-6f, "branch Gc" );
	expect_true( fabsf( Cem_KalthoffYoungGPa() - 190.0f ) < 1e-6f, "Kalthoff E" );
	expect_true( Cem_NeumannBranchMeshFloor() == 70000, "mesh floor" );
}

static void test_stages_patterns( void )
{
	expect_true( Cem_StageCount() == 5, "5 stages" );
	expect_true( Cem_PatternCount() == 6, "6 patterns" );
	expect_true( Cem_GetPattern( 0 ) && strstr( Cem_GetPattern( 0 )->name, "I" ), "pattern I" );
	expect_true( Cem_GetStage( 2 ) && strstr( Cem_GetStage( 2 )->name, "release" ), "release stage" );
}

static void test_gi_gii( void )
{
	const float n[3] = { 0.0f, 1.0f, 0.0f };
	const float d1[3] = { 0.0f, 2.0f, 0.0f };
	const float d2[3] = { 0.0f, 4.0f, 0.0f };
	const float s3[3] = { 0.0f, 3.0f, 0.0f };
	const float s4[3] = { 0.0f, 5.0f, 0.0f };
	const float s6[3] = { 0.0f, 3.0f, 0.0f };
	float gi, gii;

	/* G_I = 1/2 (3*2 + 5*4) = 1/2 (6+20) = 13 */
	gi = Cem_EvalGI( n, d1, d2, s3, s4 );
	expect_true( fabsf( gi - 13.0f ) < 1e-4f, "G_I = 13" );

	/* G_II = 1/2 (3*2 + 3*4) = 1/2 (6+12) = 9 */
	gii = Cem_EvalGII( n, d1, d2, s6 );
	expect_true( fabsf( gii - 9.0f ) < 1e-4f, "G_II = 9" );

	expect_true( Cem_ShouldFail( 13.0f, 10.0f ) == 1, "fail when G>=Gc" );
	expect_true( Cem_ShouldFail( 9.0f, 10.0f ) == 0, "no fail when G<Gc" );
}

static void test_stretch( void )
{
	float delta[3];
	const float uI[3] = { 0.0f, 0.1f, 0.0f };
	const float uJ[3] = { 0.0f, 0.0f, 0.0f };
	const float xI[3] = { 0.0f, 0.0f, 0.0f };
	const float xJ[3] = { 0.0f, 1.1f, 0.0f };
	const float XI[3] = { 0.0f, 0.0f, 0.0f };
	const float XJ[3] = { 0.0f, 1.0f, 0.0f };

	Cem_EdgeStretch( delta, uI, uJ, xI, xJ, XI, XJ );
	expect_true( fabsf( delta[1] - 0.1f ) < 1e-5f, "tensile stretch uy" );

	/* Compression → zero */
	{
		const float xJc[3] = { 0.0f, 0.9f, 0.0f };
		Cem_EdgeStretch( delta, uI, uJ, xI, xJc, XI, XJ );
		expect_true( fabsf( delta[0] ) + fabsf( delta[1] ) + fabsf( delta[2] ) < 1e-6f,
			"no stretch when compressed" );
	}
}

static void test_gaps( void )
{
	int i;
	int foundAbsentG = 0;

	expect_true( Cem_GapCount() == 5, "5 gaps" );
	for ( i = 0; i < Cem_GapCount(); i++ ) {
		const cem_gap_t *g = Cem_GetGap( i );
		if ( g && strstr( g->feature, "ES-FEM" ) && g->status == CEM_STATUS_ABSENT ) {
			foundAbsentG = 1;
		}
	}
	expect_true( foundAbsentG, "ES-FEM G marked absent vs DMM" );
}

static void test_advice( void )
{
	const char *a = Cem_SelectAdvice( "branch" );
	const char *b = Cem_SelectAdvice( "limit" );
	expect_true( a && strlen( a ) > 20, "branch advice" );
	expect_true( b && strlen( b ) > 20, "limit advice" );
	expect_true( Cem_PaperCite() && strstr( Cem_PaperCite(), "2508.04076" ), "cite" );
}

int main( void )
{
	test_constants();
	test_stages_patterns();
	test_gi_gii();
	test_stretch();
	test_gaps();
	test_advice();

	printf( "unit_cem: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? 1 : 0;
}
