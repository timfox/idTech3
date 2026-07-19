/*
===========================================================================
Unit tests for How Dark is Dark scaffold (Filip & Vávra arXiv:2601.05094).
===========================================================================
*/

#include "howdark/howdark.h"

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

static void test_material_count( void )
{
	expect_true( HowDark_MaterialCount() == 6, "six materials" );
}

static void test_albedo_gap( void )
{
	float fabric = HowDark_Albedo( 3 ); /* Musou fabric */
	float ultra = HowDark_Albedo( 0 );  /* Vantablack */
	float acrylic = HowDark_Albedo( 4 );

	expect_true( fabric < acrylic * 0.2f, "fabric albedo << acrylic" );
	expect_true( ultra < acrylic * 0.2f, "vantablack albedo << acrylic" );
	expect_true( acrylic / fabric > 8.0f, "acrylic ~10x fabric albedo" );
}

static void test_velvet_tis( void )
{
	float velvet = HowDark_GetMaterial( 2 )->tisMean;
	float acrylic = HowDark_GetMaterial( 4 )->tisMean;
	float chalk = HowDark_GetMaterial( 5 )->tisMean;

	expect_true( velvet <= acrylic, "velvet TIS <= acrylic" );
	expect_true( velvet <= chalk, "velvet TIS <= chalkboard" );
}

static void test_rank_intensity_100( void )
{
	int ids[HOWDARK_MATERIAL_COUNT];
	int n;
	int i;
	int topOk;
	int bottomOk;

	n = HowDark_RankByDarkness( 100, ids, HOWDARK_MATERIAL_COUNT );
	expect_true( n == 6, "rank writes 6" );

	/* Darkest first: Musou fabric (3) or Vantablack (0) */
	topOk = ( ids[0] == 3 || ids[0] == 0 ) && ( ids[1] == 3 || ids[1] == 0 ) && ids[0] != ids[1];
	expect_true( topOk, "top-2 are Musou fabric + Vantablack" );

	/* Brightest last: acrylic (4) or chalkboard (5) */
	bottomOk = ( ids[5] == 4 || ids[5] == 5 );
	expect_true( bottomOk, "acrylic/chalkboard among brightest" );

	for ( i = 0; i < n - 1; i++ ) {
		expect_true(
			HowDark_PerceivedDarkness( ids[i], 100 ) >=
				HowDark_PerceivedDarkness( ids[i + 1], 100 ),
			"rank monotonic descending" );
	}
}

static void test_grazing_specular_coatings( void )
{
	float vantaRs = HowDark_Specular( 0, 85.0f );
	float acrylicRs = HowDark_Specular( 4, 85.0f );

	expect_true( acrylicRs > vantaRs * 10.0f, "acrylic Rs >> vantablack at grazing" );
}

static void test_advice( void )
{
	const char *a = HowDark_SelectAdvice( "stray" );
	const char *b = HowDark_SelectAdvice( "calibration" );
	const char *c = HowDark_SelectAdvice( NULL );

	expect_true( a && strlen( a ) > 20, "stray advice" );
	expect_true( b && strlen( b ) > 20, "calibration advice" );
	expect_true( c && strlen( c ) > 20, "default advice" );
}

static void test_find( void )
{
	expect_true( HowDark_FindMaterial( "0" ) == 0, "find by id" );
	expect_true( HowDark_FindMaterial( "velvet" ) == 2, "find velvet" );
	expect_true( HowDark_FindMaterial( "acrylic" ) == 4, "find acrylic" );
	expect_true( HowDark_FindMaterial( "nope" ) < 0, "unknown" );
}

int main( void )
{
	test_material_count();
	test_albedo_gap();
	test_velvet_tis();
	test_rank_intensity_100();
	test_grazing_specular_coatings();
	test_advice();
	test_find();

	printf( "unit_howdark: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? 1 : 0;
}
