/*
===========================================================================
Unit tests for Domany–Kinzel QSD (dense transfer + observables).
===========================================================================
*/

#include "dk_qsd/dk_qsd.h"
#include "dk_qsd/dk_qsd_internal.h"

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
		fprintf( stderr, "FAIL: %s (got %.6f expected %.6f)\n", msg, a, b );
	}
}

static void test_bond_dp_kernels( void )
{
	float W[2][2][2];
	float V[2][2];
	const float p = 0.60f;

	DK_Kernels_Fill( p, W, V );
	expect_near( W[1][1][1], p * ( 2.0f - p ), 1e-6f, "P[2]=p(2-p)" );
	expect_near( W[0][0][1], 0.0f, 1e-6f, "P[0]=0" );
	expect_near( V[1][1], p, 1e-6f, "boundary P[1]=p" );
}

static void test_transfer_stochastic( void )
{
	const int N = 8;
	const int dim = DK_Dense_StateCount( N );
	float *T;
	int j;
	float colSum;

	T = (float *)malloc( sizeof( float ) * (size_t)dim * (size_t)dim );
	DK_Dense_BuildTransfer( N, 0.60f, T );

	for ( j = 0; j < dim; j++ ) {
		int i;
		colSum = 0.0f;
		for ( i = 0; i < dim; i++ ) {
			colSum += T[i * dim + j];
		}
		expect_near( colSum, 1.0f, 1e-4f, "transfer column stochastic" );
	}

	free( T );
}

static void test_inactive_mean_active_saturated( void )
{
	dk_qsd_state_t *s8;
	dk_qsd_state_t *s10;
	dk_qsd_observables_t o8;
	dk_qsd_observables_t o10;

	s8 = DK_Qsd_Solve( 8, 0.60f, DK_QSD_METHOD_DENSE, 32, 120, 1e-4f );
	s10 = DK_Qsd_Solve( 10, 0.60f, DK_QSD_METHOD_DENSE, 32, 120, 1e-4f );
	expect_true( s8 != NULL && s10 != NULL, "inactive solve" );

	DK_Qsd_ComputeObservables( s8, 0, 0, &o8 );
	DK_Qsd_ComputeObservables( s10, 0, 0, &o10 );

	expect_true( o8.meanActive < 30.0f, "inactive <n> O(1) at N=8" );
	expect_true( fabsf( o8.meanActive - o10.meanActive ) < 5.0f, "inactive <n> N-independent" );
	expect_true( o8.r11 > 1.0f, "inactive R11 > 1" );
	expect_true( o8.halfChainMI > 0.15f, "inactive I_half > active-scale noise" );
	expect_true( s8->lambda1 < 0.999f && s8->lambda1 > 0.5f, "inactive lambda1 < 1" );

	DK_Qsd_Free( s8 );
	DK_Qsd_Free( s10 );
}

static void test_active_bulk_like( void )
{
	dk_qsd_state_t *sInactive;
	dk_qsd_state_t *sActive;
	dk_qsd_observables_t oInactive;
	dk_qsd_observables_t oActive;

	sInactive = DK_Qsd_Solve( 8, 0.60f, DK_QSD_METHOD_DENSE, 32, 120, 1e-4f );
	sActive = DK_Qsd_Solve( 8, 0.70f, DK_QSD_METHOD_DENSE, 32, 120, 1e-4f );
	expect_true( sInactive != NULL && sActive != NULL, "phase solves" );
	DK_Qsd_ComputeObservables( sInactive, 0, 0, &oInactive );
	DK_Qsd_ComputeObservables( sActive, 0, 0, &oActive );
	expect_true( oActive.meanActive > oInactive.meanActive, "active <n> above inactive" );
	expect_near( oActive.r11, 1.0f, 0.35f, "active R11 ~ 1" );
	expect_true( oInactive.halfChainMI > oActive.halfChainMI, "inactive MI exceeds active" );
	DK_Qsd_Free( sInactive );
	DK_Qsd_Free( sActive );
}

static void test_binary_entropy( void )
{
	expect_near( DK_Qsd_BinaryEntropy( 0.5f ), 1.0f, 1e-4f, "h(1/2)=1" );
	expect_near( DK_Qsd_BinaryEntropy( 0.0f ), 0.0f, 1e-6f, "h(0)=0" );
}

static void test_absorbing_projected( void )
{
	const int N = 8;
	const int dim = DK_Dense_StateCount( N );
	float *prob;
	dk_qsd_state_t *s;

	prob = (float *)malloc( sizeof( float ) * (size_t)dim );
	DK_Dense_PowerIterate( N, 0.60f, prob, 120, 1e-4f, NULL, NULL, NULL, NULL );
	expect_near( prob[0], 0.0f, 1e-8f, "QSD has zero absorbing weight" );

	s = DK_Qsd_Solve( N, 0.60f, DK_QSD_METHOD_DENSE, 32, 120, 1e-4f );
	expect_true( s != NULL && s->converged, "dense converged" );
	DK_Qsd_Free( s );
	free( prob );
}

int main( void )
{
	test_bond_dp_kernels();
	test_transfer_stochastic();
	test_binary_entropy();
	test_absorbing_projected();
	test_inactive_mean_active_saturated();
	test_active_bulk_like();

	fprintf( stderr, "unit_dk_qsd: %d run, %d failed\n", tests_run, tests_failed );
	return ( tests_failed > 0 ) ? 1 : 0;
}
