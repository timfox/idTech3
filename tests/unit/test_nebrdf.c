/*
===========================================================================
Unit tests for NEBRDF scaffold (Shen et al. arXiv:2604.24081).
===========================================================================
*/

#include "nebrdf/nebrdf.h"

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

static void test_nodes( void )
{
	expect_true( NeBrdf_NodeCount() == 11, "N=11" );
	expect_true( NeBrdf_IsNeural( 3 ), "F neural" );
	expect_true( NeBrdf_IsNeural( 4 ), "G neural" );
	expect_true( NeBrdf_IsNeural( 5 ), "1/E neural" );
	expect_true( NeBrdf_IsNeural( 9 ), "mul F×G neural" );
	expect_true( !NeBrdf_IsNeural( 2 ), "D stays analytic" );
}

static void test_order( void )
{
	int order[NEBRDF_ENHANCE_ORDER_LEN];
	int n = NeBrdf_EnhancementOrder( order, NEBRDF_ENHANCE_ORDER_LEN );

	expect_true( n == 4, "order length 4" );
	expect_true( order[0] == 5, "first E (1/E)" );
	expect_true( order[1] == 4, "second G" );
	expect_true( order[2] == 9, "third mul F×G" );
	expect_true( order[3] == 3, "fourth F" );
}

static void test_params( void )
{
	nebrdf_param_counts_t p;

	NeBrdf_ParamCounts( &p );
	expect_true( p.analyticalParams == 12, "12 analytical" );
	expect_true( p.neuralParams == 27, "27 neural" );
	expect_true( p.totalParams == 39, "39 total" );
	expect_true( p.mlpH0 == 16 && p.mlpH1 == 32 && p.mlpH2 == 16, "MLP 16-32-16" );
}

static void test_hypercube( void )
{
	int n = NeBrdf_NodeCount();
	expect_true( NeBrdf_HypercubeNeighbors( n, 1 ) == n + 1, "Hamming≤1 → N+1" );
	expect_true( NeBrdf_EpochsBetweenStateChanges() == 30, "30 epochs" );
}

static void test_perf( void )
{
	expect_true( NeBrdf_FitTimeSec( 1 ) < NeBrdf_FitTimeSec( 0 ), "enhanced fit faster" );
	expect_true( NeBrdf_RenderRaysPerSec( 1 ) < NeBrdf_RenderRaysPerSec( 0 ), "GGX renders faster" );
}

static void test_compare( void )
{
	const nebrdf_compare_row_t *r = NeBrdf_CompareRow( 0 );
	expect_true( r != NULL, "compare row0" );
	expect_true( r->enhancedSsim > r->ggxSsim, "enhanced SSIM > GGX on row0" );
}

static void test_advice( void )
{
	const char *a = NeBrdf_SelectAdvice( "fit" );
	const char *b = NeBrdf_SelectAdvice( "limit" );
	expect_true( a && strlen( a ) > 20, "fit advice" );
	expect_true( b && strlen( b ) > 20, "limit advice" );
}

int main( void )
{
	test_nodes();
	test_order();
	test_params();
	test_hypercube();
	test_perf();
	test_compare();
	test_advice();

	printf( "unit_nebrdf: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? 1 : 0;
}
