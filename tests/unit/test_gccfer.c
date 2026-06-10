/*
===========================================================================
Unit tests for GCC-FER dataset stats and CA-FER adaptation.
===========================================================================
*/

#include "gccfer/gccfer.h"

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

static void test_dataset_totals( void )
{
	const gccfer_culture_row_t *rows = Gccfer_DatasetTable();
	int sum = 0;
	int c;

	for ( c = 0; c < GCCFER_NUM_CULTURES; c++ ) {
		sum += rows[c].by_expr.total;
	}
	expect_true( sum == Gccfer_TotalSamples(), "culture rows sum to 23934" );
	expect_true( Gccfer_CountFor( GCCFER_CULTURE_CAUCASIAN, GCCFER_EXPR_HAPPY ) == 1206,
		"Table II caucasian happy" );
}

static void test_cafer_adapt_changes_latent( void )
{
	gccfer_cafer_params_t params;
	float latent[GCCFER_LATENT_DIM];
	float out[GCCFER_LATENT_DIM];
	float a[GCCFER_LATENT_DIM];
	float b[GCCFER_LATENT_DIM];
	int i;

	Gccfer_CaferInitDefaults( &params, 99u );
	for ( i = 0; i < GCCFER_LATENT_DIM; i++ ) {
		latent[i] = 0.1f;
	}
	Gccfer_GenerateAdaptParams( &params, GCCFER_CULTURE_AFRICAN, a, b );
	Gccfer_AdaptLatent( latent, a, b, GCCFER_LATENT_DIM, out );
	expect_true( fabsf( out[0] - latent[0] ) > 1e-6f, "adaptation modifies latent" );
}

static void test_au_stats_dim( void )
{
	float au[GCCFER_FRAMES_PER_VIDEO * GCCFER_NUM_AUS];
	gccfer_au_stats_t stats;
	float vec[GCCFER_AU_FEATURE_DIM];
	int i;

	for ( i = 0; i < GCCFER_FRAMES_PER_VIDEO * GCCFER_NUM_AUS; i++ ) {
		au[i] = (float)( i % 10 ) / 10.0f;
	}
	Gccfer_AuStatsFromSequence( au, GCCFER_FRAMES_PER_VIDEO, &stats );
	Gccfer_AuStatsToVector( &stats, vec );
	expect_true( vec[0] >= 0.0f && vec[GCCFER_NUM_AUS - 1] <= 1.0f, "AU mean in range" );
	expect_true( vec[GCCFER_AU_FEATURE_DIM - 1] >= 0.0f, "AU freq non-negative" );
}

static void test_benchmark_cafer_best( void )
{
	gccfer_metrics_t m;
	Gccfer_ModelEvaluate( GCCFER_METHOD_CAFER, &m );
	expect_true( m.uar > 60.0f && m.uar < 65.0f, "CA-FER UAR ~61.7" );
	expect_true( m.war > 63.0f && m.war < 67.0f, "CA-FER WAR ~64.8" );
}

static void test_focal_loss_positive( void )
{
	float logits[GCCFER_NUM_EXPRESSIONS] = { 0.1f, -0.2f, 0.3f, 1.0f, 0.0f, -0.5f, 0.2f };
	float loss = Gccfer_FocalLoss( logits, GCCFER_NUM_EXPRESSIONS, GCCFER_EXPR_HAPPY, 2.0f, 1.0f, 0.1f );
	expect_true( loss > 0.0f, "focal loss positive" );
}

int main( void )
{
	test_dataset_totals();
	test_cafer_adapt_changes_latent();
	test_au_stats_dim();
	test_benchmark_cafer_best();
	test_focal_loss_positive();

	printf( "test_gccfer: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
