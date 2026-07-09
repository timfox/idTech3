/*
===========================================================================
Unit tests for BubbleSH compact state and metric helpers.
===========================================================================
*/

#include "qcommon/bubblesh_model.h"

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

static void expect_near( float a, float b, float eps, const char *msg )
{
	tests_run++;
	if ( fabsf( a - b ) > eps ) {
		tests_failed++;
		fprintf( stderr, "FAIL: %s (%.6f vs %.6f)\n", msg, a, b );
	}
}

static void test_coeff_counts( void )
{
	expect_true( BubbleSH_CoefficientCount( 3 ) == 16, "L=3 -> 16 coeffs" );
	expect_true( BubbleSH_CoefficientCount( 5 ) == 36, "L=5 -> 36 coeffs" );
	expect_true( BubbleSH_CoefficientCount( 14 ) == 225, "L=14 -> 225 coeffs" );
}

static void test_config_lookup( void )
{
	bubblesh_config_t cfg;

	expect_true( BubbleSH_FillConfig( 5, 30, 14, &cfg ), "5mm 30% config exists" );
	expect_true( cfg.state_dim == 231, "state dim is 6 + 225" );
	expect_near( cfg.dt_seconds, 1.0e-4f, 1.0e-8f, "5mm cases use 1e-4 s" );

	expect_true( BubbleSH_FillConfig( 4, 15, 14, &cfg ), "4mm 15% config exists" );
	expect_near( cfg.dt_seconds, 1.0e-3f, 1.0e-8f, "4mm 15% uses 1e-3 s" );
}

static void test_domain_size_trends( void )
{
	const float low_eps = BubbleSH_DomainSizeMm( 32, 5.0f, 5.0f );
	const float high_eps = BubbleSH_DomainSizeMm( 32, 5.0f, 40.0f );
	const float d4 = BubbleSH_DomainSizeMm( 32, 4.0f, 20.0f );
	const float d6 = BubbleSH_DomainSizeMm( 32, 6.0f, 20.0f );

	expect_true( low_eps > high_eps, "lower void fraction gives larger box" );
	expect_true( d6 > d4, "larger bubbles need larger box at fixed void fraction" );
}

static void test_relative_metrics( void )
{
	expect_near( BubbleSH_RelativeAverageDisplacementError( 2.0f, 10.0f ), 0.2f, 1.0e-6f, "R-ADE formula" );
	expect_near( BubbleSH_RelativeFinalDisplacementError( 3.0f, 10.0f ), 0.3f, 1.0e-6f, "R-FDE formula" );
	expect_near( BubbleSH_RelativeAverageChamferDistance( 1.0f, 4.0f ), 0.25f, 1.0e-6f, "R-ACD formula" );
}

static void test_wasserstein_normalization( void )
{
	static const float truth[] = { 0.0f, 1.0f, 2.0f, 3.0f };
	static const float pred[] =  { 1.0f, 2.0f, 3.0f, 4.0f };

	expect_near( BubbleSH_Wasserstein1( pred, truth, 4 ), 1.0f, 1.0e-6f, "W1 sorted average distance" );
	expect_near( BubbleSH_InterquartileRange( truth, 4 ), 1.5f, 1.0e-6f, "IQR uses interpolated quartiles" );
	expect_near( BubbleSH_NormalizedWasserstein1( pred, truth, 4 ), 2.0f / 3.0f, 1.0e-6f, "normalized W1 / IQR" );
}

int main( void )
{
	test_coeff_counts();
	test_config_lookup();
	test_domain_size_trends();
	test_relative_metrics();
	test_wasserstein_normalization();

	printf( "test_bubblesh_model: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
