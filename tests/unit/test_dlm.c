/*
===========================================================================
Unit tests for deep-layered machine exact q(n) distribution (Fink 2026).
===========================================================================
*/

#include "dlm/dlm.h"

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

static void expect_near( float a, float b, float eps, const char *msg )
{
	tests_run++;
	if ( fabsf( a - b ) > eps ) {
		tests_failed++;
		fprintf( stderr, "FAIL: %s (got %.8f expected %.8f)\n", msg, a, b );
	}
}

static void test_table1_k1_n2( void )
{
	dlm_distribution_t *dist;

	dist = DLM_ComputeExact( 1, 2 );
	expect_true( dist != NULL, "k=1 n=2 exact" );
	if ( dist ) {
		expect_near( dist->q[0], 6.0f / 16.0f, 1e-5f, "k=1 n=2 q(w=0)=6/16" );
		expect_near( dist->q[1], 4.0f / 16.0f, 1e-5f, "k=1 n=2 q(w=1)=4/16" );
		expect_near( dist->q[2], 6.0f / 16.0f, 1e-5f, "k=1 n=2 q(w=2)=6/16" );
		DLM_Free( dist );
	}
}

static void test_table1_k2_n2( void )
{
	dlm_distribution_t *dist;

	dist = DLM_ComputeExact( 2, 2 );
	expect_true( dist != NULL, "k=2 n=2 exact" );
	if ( dist ) {
		expect_near( dist->q[0], 680.0f / 4096.0f, 1e-5f, "k=2 n=2 q(w=0)" );
		expect_near( dist->q[4], 680.0f / 4096.0f, 1e-5f, "k=2 n=2 q(w=4)" );
		DLM_Free( dist );
	}
}

static void test_initial_uniform_p( void )
{
	dlm_distribution_t *dist;

	dist = DLM_ComputeExact( 2, 1 );
	expect_true( dist != NULL, "k=2 n=1" );
	if ( dist ) {
		expect_near( dist->p[0], 1.0f / 16.0f, 1e-6f, "n=1 uniform p" );
		expect_near( dist->q[0], 1.0f / 16.0f, 1e-6f, "n=1 binomial q(w=0)" );
		expect_near( dist->q[2], 6.0f / 16.0f, 1e-6f, "n=1 q(w=2)=C(4,2)/16" );
		DLM_Free( dist );
	}
}

static void test_eigenvalues_k2( void )
{
	expect_near( (float)DLM_Eigenvalue( 2, 0 ), 1.0f, 1e-6f, "lambda0=1" );
	expect_near( (float)DLM_Eigenvalue( 2, 1 ), 1.0f, 1e-6f, "lambda1=1" );
	expect_near( (float)DLM_Eigenvalue( 2, 2 ), 0.75f, 1e-6f, "lambda2=12/16" );
}

static void test_enum_matches_exact( void )
{
	int counts[8];
	int total;
	float qEmp[4];
	int w;

	total = DLM_EnumerateWeightCounts( 1, 2, counts, 8 );
	expect_true( total == 16, "k=1 n=2 16 configs" );
	if ( total > 0 ) {
		for ( w = 0; w <= 2; w++ ) {
			qEmp[w] = (float)counts[w] / (float)total;
		}
		expect_near( qEmp[0], 6.0f / 16.0f, 1e-5f, "enum q(w=0)" );
		expect_near( qEmp[1], 4.0f / 16.0f, 1e-5f, "enum q(w=1)" );
		expect_near( qEmp[2], 6.0f / 16.0f, 1e-5f, "enum q(w=2)" );
	}
}

static void test_endpoints_grow_with_depth( void )
{
	dlm_distribution_t *d2;
	dlm_distribution_t *d4;

	d2 = DLM_ComputeExact( 2, 2 );
	d4 = DLM_ComputeExact( 2, 4 );
	expect_true( d2 && d4, "depth solves" );
	if ( d2 && d4 ) {
		expect_true( d4->endpointProb > d2->endpointProb, "true/false prob grows with n" );
	}
	DLM_Free( d2 );
	DLM_Free( d4 );
}

int main( void )
{
	test_table1_k1_n2();
	test_table1_k2_n2();
	test_initial_uniform_p();
	test_eigenvalues_k2();
	test_enum_matches_exact();
	test_endpoints_grow_with_depth();

	fprintf( stderr, "unit_dlm: %d run, %d failed\n", tests_run, tests_failed );
	return ( tests_failed > 0 ) ? 1 : 0;
}
