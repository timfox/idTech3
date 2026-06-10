/*
===========================================================================
Unit tests for DaX benchmark stats and evaluation protocol.
===========================================================================
*/

#include "dax/dax.h"

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

static void test_benchmark_stats( void )
{
	dax_benchmark_stats_t s = Dax_BenchmarkStats();
	expect_true( s.num_tasks == 161, "161 benchmark tasks" );
	expect_true( s.num_datasets == 44, "44 datasets" );
	expect_true( s.num_patients == 28182, "28182 patients" );
	expect_true( s.num_slides == 34394, "34394 slides" );
	expect_true( s.pretrain_wsis == 104569, "104569 pretrain WSIs" );
}

static void test_category_task_sum( void )
{
	const dax_category_row_t *rows = Dax_CategoryTable( NULL );
	int sum = 0;
	int i;

	for ( i = 0; i < DAX_NUM_LEVEL2_CATEGORIES; i++ ) {
		sum += rows[i].task_count;
	}
	expect_true( sum == DAX_NUM_BENCHMARK_TASKS, "category tasks sum to 161" );
}

static void test_anchor_magnifications( void )
{
	expect_true( Dax_AnchorMagnification( 0 ) == 2.5f, "2.5x anchor" );
	expect_true( Dax_AnchorMagnification( 3 ) == 20.0f, "20x anchor" );
}

static void test_stage2_crops( void )
{
	int g;
	int l;

	Dax_Stage2CropPair( 0, &g, &l );
	expect_true( g == 512 && l == 224, "stage2 crop pair 0" );
	Dax_Stage2CropPair( 2, &g, &l );
	expect_true( g == 768 && l == 336, "stage2 crop pair 2" );
}

static void test_statistical_rank( void )
{
	dax_fold_result_t dax_res;
	dax_fold_result_t uni_res;
	dax_fold_result_t models[2];
	int rank;
	int i;

	Dax_FoldResultInit( &dax_res );
	Dax_FoldResultInit( &uni_res );
	for ( i = 0; i < 4; i++ ) {
		Dax_FoldResultPush( &dax_res, 0.80f + 0.01f * (float)i );
		Dax_FoldResultPush( &uni_res, 0.70f + 0.01f * (float)i );
	}
	models[0] = dax_res;
	models[1] = uni_res;
	rank = Dax_StatisticalRankScore( models, 2, models, 0, 0.05f );
	expect_true( rank >= 1, "DaX beats UNI in rank score" );
}

static void test_gram_diff( void )
{
	float a[8] = { 1, 0, 0, 1, 0.5f, 0.5f, 0.5f, 0.5f };
	float b[8] = { 1, 0, 0, 1, 0.5f, 0.5f, 0.5f, 0.5f };
	float diff = Dax_GramMatrixFrobeniusDiff( a, b, 2, 4 );
	expect_true( diff < 1e-5f, "identical tokens zero gram diff" );
}

static void test_dax_top_model( void )
{
	dax_model_row_t row;
	Dax_ModelLookup( DAX_MODEL_DAX, &row );
	expect_true( row.params_m == 304, "DaX ViT-L 304M" );
	expect_true( row.mean_benchmark_score > 40.0f, "DaX mean score leading" );
}

int main( void )
{
	test_benchmark_stats();
	test_category_task_sum();
	test_anchor_magnifications();
	test_stage2_crops();
	test_statistical_rank();
	test_gram_diff();
	test_dax_top_model();

	printf( "test_dax: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
