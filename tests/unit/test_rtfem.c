/*
===========================================================================
Unit tests for RTFEM scaffold (Parker & O'Brien SCA 2009).
===========================================================================
*/

#include "rtfem/rtfem.h"

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
	expect_true( fabsf( RtFem_InvertVolumeThreshold() - 0.06f ) < 1e-6f, "invert 0.06" );
	expect_true( fabsf( RtFem_CgRelError() - 0.001f ) < 1e-9f, "CG 0.001" );
	expect_true( RtFem_FractureMinTets() == 3, "min tets 3" );
	expect_true( fabsf( RtFem_FastObjectMoveFraction() - ( 1.0f / 6.0f ) ) < 1e-6f, "1/6" );
}

static void test_island( void )
{
	expect_true( RtFem_LargeIslandHeuristic( 80, 200 ) == 1, "80 of 200 is large" );
	expect_true( RtFem_LargeIslandHeuristic( 50, 200 ) == 0, "50 < 60 not large" );
	expect_true( RtFem_LargeIslandHeuristic( 80, 400 ) == 0, "80 of 400 not >1/4" );
}

static void test_stages( void )
{
	expect_true( RtFem_StageCount() == 11, "11 stages" );
	expect_true( RtFem_GetStage( 0 ) != NULL, "stage0" );
	expect_true( RtFem_GetStage( 2 ) && strstr( RtFem_GetStage( 2 )->name, "invert" ), "invert stage" );
}

static void test_gaps( void )
{
	int i;
	int foundAbsentTet = 0;

	expect_true( RtFem_GapCount() == 8, "8 gaps" );
	for ( i = 0; i < RtFem_GapCount(); i++ ) {
		const rtfem_gap_t *g = RtFem_GetGap( i );
		if ( g && strstr( g->feature, "tetrahedral" ) && g->status == RTFEM_STATUS_ABSENT ) {
			foundAbsentTet = 1;
		}
	}
	expect_true( foundAbsentTet, "tet FEM marked absent" );
}

static void test_advice( void )
{
	const char *a = RtFem_SelectAdvice( "design" );
	const char *b = RtFem_SelectAdvice( "limit" );
	expect_true( a && strlen( a ) > 20, "design advice" );
	expect_true( b && strlen( b ) > 20, "limit advice" );
	expect_true( RtFem_PaperCite() && strstr( RtFem_PaperCite(), "SCA 2009" ), "cite" );
}

int main( void )
{
	test_constants();
	test_island();
	test_stages();
	test_gaps();
	test_advice();

	printf( "unit_rtfem: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? 1 : 0;
}
