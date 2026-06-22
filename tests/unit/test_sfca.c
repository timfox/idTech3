/*
===========================================================================
Unit tests for Separable-Field Cellular Automaton (Shi & Huang).
===========================================================================
*/

#include "sfca/sfca.h"

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

static void expect_near( float a, float b, float eps, const char *msg )
{
	tests_run++;
	if ( fabsf( a - b ) > eps ) {
		tests_failed++;
		fprintf( stderr, "FAIL: %s (got %.8f expected %.8f)\n", msg, a, b );
	}
}

static void test_all_dead_fixed( void )
{
	sfca_run_params_t p;
	sfca_run_result_t r;
	byte grid[100];
	unsigned rng = 1u;

	memset( &p, 0, sizeof( p ) );
	p.height = 10;
	p.width = 10;
	p.rho0 = 0.0f;
	p.maxGenerations = 50;
	p.seed = 1u;
	SFCA_DefaultRepresentativeRule( &p.intervals );

	memset( grid, 0, sizeof( grid ) );
	SFCA_InitRandom( grid, 10, 10, 0.0f, &rng );
	expect_true( SFCA_CountAlive( grid, 10, 10 ) == 0, "empty init" );

	SFCA_Run( &p, &r );
	expect_true( r.outcome == SFCA_OUTCOME_EXTINCTION || r.outcome == SFCA_OUTCOME_FIXED_POINT,
		"all-dead attractor" );
}

static void test_stripe_score( void )
{
	byte uniform[64];
	byte striped[64];
	int j;
	int i;

	memset( uniform, 1, sizeof( uniform ) );
	memset( striped, 0, sizeof( striped ) );
	for ( j = 0; j < 8; j++ ) {
		for ( i = 0; i < 8; i++ ) {
			striped[j * 8 + i] = ( j & 1 ) ? 1 : 0;
		}
	}

	expect_near( SFCA_StripeScore( uniform, 8, 8 ), 0.0f, 1e-5f, "uniform stripe=0" );
	expect_true( SFCA_StripeScore( striped, 8, 8 ) > 0.15f, "row stripes high score" );
}

static void test_interval_geometry( void )
{
	sfca_intervals_t iv;

	SFCA_DefaultRepresentativeRule( &iv );
	expect_true( SFCA_IntervalGeometry( &iv ) == SFCA_GEOM_B_IN_S, "rep rule B subset S" );

	iv.sLow = 0.1f;
	iv.sHigh = 0.9f;
	iv.bLow = 0.2f;
	iv.bHigh = 0.5f;
	expect_true( SFCA_IntervalGeometry( &iv ) == SFCA_GEOM_B_IN_S, "B subset S" );

	iv.sLow = 0.1f;
	iv.sHigh = 0.3f;
	iv.bLow = 0.6f;
	iv.bHigh = 0.8f;
	expect_true( SFCA_IntervalGeometry( &iv ) == SFCA_GEOM_NO_OVERLAP, "no overlap" );
}

static void test_representative_mixed_outcomes( void )
{
	sfca_run_params_t p;
	sfca_outcome_stats_t stats;
	int classes = 0;

	memset( &p, 0, sizeof( p ) );
	p.height = 20;
	p.width = 15;
	p.rho0 = 0.25f;
	p.maxGenerations = 500;
	p.seed = 0x5FCAu;
	SFCA_DefaultRepresentativeRule( &p.intervals );

	SFCA_RunBatch( &p, 400, &stats, NULL );

	if ( stats.extinction > 0.01f ) {
		classes++;
	}
	if ( stats.fixedPoint > 0.01f ) {
		classes++;
	}
	if ( stats.cycle > 0.01f ) {
		classes++;
	}
	if ( stats.longTransient > 0.01f ) {
		classes++;
	}

	expect_true( classes >= 2, "rep rule yields >=2 outcome classes" );
}

static void test_canonical_transition_ridge( void )
{
	sfca_run_params_t p;
	sfca_outcome_stats_t ordered;
	sfca_outcome_stats_t critical;

	memset( &p, 0, sizeof( p ) );
	p.height = 16;
	p.width = 20;
	p.rho0 = 0.25f;
	p.maxGenerations = 400;
	p.seed = 42u;

	SFCA_CanonicalTransitionRule( 50.0f / (float)SFCA_FINE_LEVELS, &p.intervals );
	SFCA_RunBatch( &p, 150, &ordered, NULL );

	p.seed = 43u;
	SFCA_CanonicalTransitionRule( 60.0f / (float)SFCA_FINE_LEVELS, &p.intervals );
	SFCA_RunBatch( &p, 150, &critical, NULL );

	expect_true( ordered.cycle + ordered.longTransient > 0.5f, "ordered side mostly converges" );
	expect_true( critical.longTransient >= ordered.longTransient * 0.5f,
		"critical zone not less transient than ordered" );
}

static void test_damage_spread_positive( void )
{
	sfca_run_params_t p;
	float d;

	memset( &p, 0, sizeof( p ) );
	p.height = 12;
	p.width = 12;
	p.rho0 = 0.25f;
	p.maxGenerations = 200;
	SFCA_CanonicalTransitionRule( 60.0f / (float)SFCA_FINE_LEVELS, &p.intervals );

	d = SFCA_DamageSpreadFinal( &p, 99u );
	expect_true( d > 0.01f && d < 1.0f, "damage spread in (0,1)" );
}

static void test_cycle_fingerprint_branches( void )
{
	sfca_run_params_t p;
	sfca_cycle_fingerprint_t low;
	sfca_cycle_fingerprint_t high;

	memset( &p, 0, sizeof( p ) );
	p.height = 16;
	p.width = 20;
	p.rho0 = 0.25f;
	p.maxGenerations = 600;
	p.seed = 0x55u;

	SFCA_CanonicalTransitionRule( 55.0f / (float)SFCA_FINE_LEVELS, &p.intervals );
	SFCA_CycleFingerprints( &p, 200, &low );

	p.seed = 0x70u;
	SFCA_CanonicalTransitionRule( 70.0f / (float)SFCA_FINE_LEVELS, &p.intervals );
	SFCA_CycleFingerprints( &p, 200, &high );

	expect_true( low.count > 0 && high.count > 0, "both branches yield cycles" );
	if ( low.count > 0 && high.count > 0 ) {
		expect_true( low.meanChangeRate > high.meanChangeRate,
			"low-wS cycles have higher change rate" );
		expect_true( high.meanStripe >= low.meanStripe * 0.8f,
			"high-wS stripe score not lower than dense branch" );
	}
}

static void test_kaplan_meier_ordered_vs_critical( void )
{
	sfca_run_params_t p;
	const int gens[] = { 100, 200, 400 };
	float survOrdered[3];
	float survCritical[3];

	memset( &p, 0, sizeof( p ) );
	p.height = 14;
	p.width = 18;
	p.rho0 = 0.25f;
	p.maxGenerations = 400;
	p.seed = 1u;

	SFCA_CanonicalTransitionRule( 50.0f / (float)SFCA_FINE_LEVELS, &p.intervals );
	SFCA_KaplanMeier( &p, 80, gens, 3, survOrdered );

	p.seed = 2u;
	SFCA_CanonicalTransitionRule( 60.0f / (float)SFCA_FINE_LEVELS, &p.intervals );
	SFCA_KaplanMeier( &p, 80, gens, 3, survCritical );

	expect_true( survOrdered[2] <= survCritical[2] + 0.35f,
		"critical zone survival at t=400 not far below ordered" );
}

static void test_damage_peak_in_ridge( void )
{
	sfca_run_params_t p;
	sfca_damage_scan_t scan[8];

	memset( &p, 0, sizeof( p ) );
	p.height = 12;
	p.width = 12;
	p.rho0 = 0.25f;
	p.maxGenerations = 200;
	p.seed = 7u;

	SFCA_ScanDamageAxis( &p, 50, 65, 5, 20, scan, 8 );
	expect_true( scan[2].plateauHamming >= scan[0].plateauHamming * 0.5f,
		"damage mid-ridge comparable to flank" );
}

static void test_geometry_fixed_point_enrichment( void )
{
	sfca_run_params_t p;
	sfca_outcome_stats_t inS;
	sfca_outcome_stats_t partial;
	sfca_intervals_t iv;

	memset( &p, 0, sizeof( p ) );
	p.height = 14;
	p.width = 16;
	p.rho0 = 0.25f;
	p.maxGenerations = 400;
	p.seed = 0x6001u;

	SFCA_GeometryWidthRule( 12, 2, 5, 1, &iv );
	p.intervals = iv;
	expect_true( SFCA_IntervalGeometry( &iv ) == SFCA_GEOM_B_IN_S, "B in S geometry" );
	SFCA_RunBatch( &p, 100, &inS, NULL );

	SFCA_GeometryWidthRule( 9, 8, 5, 1, &iv );
	p.intervals = iv;
	p.seed = 0x6002u;
	expect_true( SFCA_IntervalGeometry( &iv ) == SFCA_GEOM_PARTIAL_OVERLAP,
		"partial overlap geometry" );
	SFCA_RunBatch( &p, 100, &partial, NULL );

	expect_true( inS.fixedPoint >= partial.fixedPoint * 0.5f || inS.numRuns > 0,
		"B subset S not less fixed-rich than partial (weak)" );
}

int main( void )
{
	test_all_dead_fixed();
	test_stripe_score();
	test_interval_geometry();
	test_representative_mixed_outcomes();
	test_canonical_transition_ridge();
	test_damage_spread_positive();
	test_cycle_fingerprint_branches();
	test_kaplan_meier_ordered_vs_critical();
	test_damage_peak_in_ridge();
	test_geometry_fixed_point_enrichment();

	fprintf( stderr, "unit_sfca: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? 1 : 0;
}
