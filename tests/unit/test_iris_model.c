/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Unit tests for Iris analytical model (J Pathol Inform 2025).
===========================================================================
*/

#include <stdio.h>
#include <stdlib.h>

#include "iris/iris_model.h"

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

static void test_codec_faster_than_openslide( void )
{
	expect_true( Iris_TeFOV( IRIS_LAYER_LR, IRIS_DECODER_CODEC ) < Iris_TeFOV( IRIS_LAYER_LR, IRIS_DECODER_OPENSELIDE ),
		"Iris Codec LR TeFOV faster than OpenSlide" );
	expect_true( Iris_TPT( IRIS_LAYER_HR, IRIS_DECODER_CODEC ) < Iris_TPT( IRIS_LAYER_HR, IRIS_DECODER_OPENSELIDE ),
		"Iris Codec HR TPT faster than OpenSlide" );
}

static void test_literature_speedup( void )
{
	float speedup = Iris_SpeedupVsLiterature( "Schuffler2022", IRIS_LAYER_HR, IRIS_DECODER_CODEC );
	expect_true( speedup >= 5.0f, "HR TeFOV >5x faster than Schuffler2022 TFOV" );
}

int main( void )
{
	test_codec_faster_than_openslide();
	test_literature_speedup();

	printf( "test_iris_model: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
