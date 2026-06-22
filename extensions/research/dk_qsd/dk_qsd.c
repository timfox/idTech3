/*
===========================================================================
Domany–Kinzel QSD solver API (dense + MPS).
===========================================================================
*/

#include "dk_qsd/dk_qsd.h"
#include "dk_qsd/dk_qsd_internal.h"

#include "qcommon/qcommon.h"

#include <math.h>
#include <string.h>

float DK_Qsd_Pc( void )
{
	return DK_QSD_PC;
}

float DK_Qsd_P2( float p )
{
	return p * ( 2.0f - p );
}

float DK_Qsd_BinaryEntropy( float x )
{
	if ( x <= 0.0f || x >= 1.0f ) {
		return 0.0f;
	}
	return -( x * ( logf( x ) / logf( 2.0f ) ) + ( 1.0f - x ) * ( logf( 1.0f - x ) / logf( 2.0f ) ) );
}

float DK_Qsd_BondMI_UniformFlock( float cutFraction, int N, float kEff )
{
	const float x = cutFraction;
	const float h = DK_Qsd_BinaryEntropy( x );
	const float corr = ( N > 0 && kEff > 0.0f )
		? ( kEff / (float)N ) * ( logf( x * ( 1.0f - x ) + 1e-30f ) / logf( 2.0f ) )
		: 0.0f;
	(void)N;
	return h + corr;
}

dk_qsd_state_t *DK_Qsd_Solve( int N, float p, dk_qsd_method_t method, int chiMax,
	int maxIter, float tol )
{
	dk_qsd_state_t *state;

	if ( N < 2 || N > 1024 || p <= 0.0f || p >= 1.0f ) {
		return NULL;
	}
	if ( maxIter <= 0 ) {
		maxIter = 200;
	}
	if ( tol <= 0.0f ) {
		tol = 1e-5f;
	}
	if ( chiMax <= 0 ) {
		chiMax = DK_QSD_CHI_MAX_DEFAULT;
	}

	state = (dk_qsd_state_t *)Z_Malloc( sizeof( *state ) );
	memset( state, 0, sizeof( *state ) );
	state->N = N;
	state->p = p;
	state->method = method;

	if ( method == DK_QSD_METHOD_DENSE || N <= DK_QSD_DENSE_MAX_N ) {
		const int dim = DK_Dense_StateCount( N );
		state->method = DK_QSD_METHOD_DENSE;
		state->prob = (float *)Z_Malloc( sizeof( float ) * (size_t)dim );
		DK_Dense_PowerIterate( N, p, state->prob, maxIter, tol,
			&state->lambda1, &state->iterations, &state->overlap, &state->converged );
	} else {
		state->method = DK_QSD_METHOD_MPS;
		DK_Mps_PowerIterate( N, p, chiMax, DK_QSD_EPS_CUT, maxIter, tol,
			&state->mps, &state->lambda1, &state->iterations, &state->overlap, &state->converged );
	}

	Com_Printf( "[DK-QSD] solved N=%d p=%.3f method=%s lambda1=%.6f iter=%d%s\n",
		N, p, ( state->method == DK_QSD_METHOD_DENSE ) ? "dense" : "mps",
		state->lambda1, state->iterations, state->converged ? "" : " (not converged)" );

	return state;
}

void DK_Qsd_Free( dk_qsd_state_t *state )
{
	if ( !state ) {
		return;
	}
	if ( state->prob ) {
		Z_Free( state->prob );
	}
	if ( state->mps.sites ) {
		DK_Mps_Free( &state->mps );
	}
	Z_Free( state );
}

float DK_Qsd_Lambda1( const dk_qsd_state_t *state )
{
	return state ? state->lambda1 : 0.0f;
}

int DK_Qsd_ChainLength( const dk_qsd_state_t *state )
{
	return state ? state->N : 0;
}

float DK_Qsd_ParamP( const dk_qsd_state_t *state )
{
	return state ? state->p : 0.0f;
}

qboolean DK_Qsd_Converged( const dk_qsd_state_t *state )
{
	return state ? state->converged : qfalse;
}

void DK_Qsd_ComputeObservables( const dk_qsd_state_t *state, int numSamples, unsigned seed,
	dk_qsd_observables_t *obs )
{
	if ( !state || !obs ) {
		return;
	}
	memset( obs, 0, sizeof( *obs ) );
	obs->lambda1 = state->lambda1;

	if ( state->method == DK_QSD_METHOD_DENSE && state->prob ) {
		DK_Obs_FromProb( state->N, state->prob, obs );
	} else if ( state->mps.sites ) {
		DK_Obs_FromMps( &state->mps, numSamples, seed, obs );
	}
}

int DK_Qsd_SampleConfig( const dk_qsd_state_t *state, byte *sites, int N, unsigned *rng )
{
	if ( !state || !sites || N != state->N ) {
		return 0;
	}
	if ( state->method == DK_QSD_METHOD_DENSE && state->prob ) {
		const int dim = DK_Dense_StateCount( N );
		unsigned r = rng ? *rng : 1u;
		float u;
		float cdf = 0.0f;
		int i;

		r = r * 1664525u + 1013904223u;
		u = (float)( r % 1000000u ) / 1000000.0f;
		for ( i = 0; i < dim; i++ ) {
			cdf += state->prob[i];
			if ( u <= cdf ) {
				DK_Dense_ConfigFromIndex( i, N, sites );
				if ( rng ) {
					*rng = r;
				}
				return 1;
			}
		}
		return 0;
	}
	if ( state->mps.sites ) {
		return DK_Mps_Sample( &state->mps, sites, rng );
	}
	return 0;
}
