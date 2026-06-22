/*
===========================================================================
Hamming-weight transition matrix A (Fink eq. 1) and q(n) evolution.
===========================================================================
*/

#include "dlm/dlm_internal.h"

#include "qcommon/qcommon.h"

#include <math.h>
#include <string.h>

int DLM_Ell( int k )
{
	if ( k < 0 || k > 30 ) {
		return 0;
	}
	return 1 << k;
}

int DLM_NumFunctions( int k )
{
	const int ell = DLM_Ell( k );
	if ( ell <= 0 || ell > 16 ) {
		return 0;
	}
	return 1 << ( 1 << k );
}

int DLM_NumNodes( int k, int depth )
{
	if ( depth < 1 || k < 1 ) {
		return 0;
	}
	return k * ( depth - 1 ) + 1;
}

double DLM_Binom( int n, int r )
{
	int i;
	double num = 1.0;
	double den = 1.0;

	if ( r < 0 || r > n ) {
		return 0.0;
	}
	for ( i = 0; i < r; i++ ) {
		num *= (double)( n - i );
		den *= (double)( i + 1 );
	}
	return num / den;
}

void DLM_TransitionEntry( int k, int i, int j, double *out )
{
	const int ell = DLM_Ell( k );
	double ipow;
	double tail;

	if ( !out || ell <= 0 ) {
		return;
	}

	if ( j == 0 ) {
		ipow = 1.0;
	} else if ( i == 0 ) {
		ipow = 0.0;
	} else {
		ipow = pow( (double)i, (double)j );
	}

	if ( ell - j == 0 ) {
		tail = 1.0;
	} else if ( ell - i == 0 ) {
	 tail = 0.0;
	} else {
		tail = pow( (double)( ell - i ), (double)( ell - j ) );
	}

	{
		const double denom = pow( (double)ell, (double)ell );
		*out = ( denom > 0.0 ) ? ( DLM_Binom( ell, j ) * ipow * tail / denom ) : 0.0;
	}
}

void DLM_BuildTransition( int k, double *A )
{
	const int ell = DLM_Ell( k );
	const int dim = ell + 1;
	int i;
	int j;

	if ( !A || dim <= 0 ) {
		return;
	}

	memset( A, 0, sizeof( double ) * (size_t)dim * (size_t)dim );
	for ( i = 0; i < dim; i++ ) {
		for ( j = 0; j < dim; j++ ) {
			DLM_TransitionEntry( k, i, j, &A[i * dim + j] );
		}
	}
}

void DLM_InitialQ( int k, float *q )
{
	const int ell = DLM_Ell( k );
	const int dim = ell + 1;
	const double denom = pow( 2.0, (double)( 1 << k ) );
	int w;

	if ( !q || dim <= 0 || denom <= 0.0 ) {
		return;
	}

	for ( w = 0; w < dim; w++ ) {
		q[w] = (float)( DLM_Binom( ell, w ) / denom );
	}
}

void DLM_MatrixVector( int dim, const double *A, const float *x, float *y )
{
	int j;
	int i;

	for ( j = 0; j < dim; j++ ) {
		double sum = 0.0;
		for ( i = 0; i < dim; i++ ) {
			sum += A[i * dim + j] * (double)x[i];
		}
		y[j] = (float)sum;
	}
}

void DLM_MatrixPowerApply( int k, int depth, const float *q0, float *qOut )
{
	const int ell = DLM_Ell( k );
	const int dim = ell + 1;
	double *A;
	float *v;
	float *tmp;
	int step;

	if ( depth < 1 || !q0 || !qOut || dim <= 0 ) {
		return;
	}

	A = (double *)Z_Malloc( sizeof( double ) * (size_t)dim * (size_t)dim );
	v = (float *)Z_Malloc( sizeof( float ) * (size_t)dim );
	tmp = (float *)Z_Malloc( sizeof( float ) * (size_t)dim );

	DLM_BuildTransition( k, A );
	memcpy( v, q0, sizeof( float ) * (size_t)dim );

	for ( step = 1; step < depth; step++ ) {
		DLM_MatrixVector( dim, A, v, tmp );
		memcpy( v, tmp, sizeof( float ) * (size_t)dim );
	}

	memcpy( qOut, v, sizeof( float ) * (size_t)dim );

	Z_Free( tmp );
	Z_Free( v );
	Z_Free( A );
}

void DLM_EvolveQ( int k, int depth, float *qOut )
{
	float *q0;

	if ( !qOut || depth < 1 ) {
		return;
	}

	q0 = (float *)Z_Malloc( sizeof( float ) * (size_t)( DLM_Ell( k ) + 1 ) );
	DLM_InitialQ( k, q0 );
	DLM_MatrixPowerApply( k, depth, q0, qOut );
	Z_Free( q0 );
}

int DLM_HammingWeight( dlm_truth_t tt, int ell )
{
	int w = 0;
	int b;
	for ( b = 0; b < ell; b++ ) {
		if ( tt & ( 1u << b ) ) {
			w++;
		}
	}
	return w;
}

void DLM_QToP( int k, const float *q, float *p )
{
	const int ell = DLM_Ell( k );
	const int numF = 1 << ell;
	int idx;

	if ( !q || !p || ell <= 0 || ell > 16 ) {
		return;
	}

	for ( idx = 0; idx < numF; idx++ ) {
		const int w = DLM_HammingWeight( (dlm_truth_t)idx, ell );
		const double binom = DLM_Binom( ell, w );
		p[idx] = ( binom > 0.0 ) ? (float)( q[w] / (float)binom ) : 0.0f;
	}
}

double DLM_Eigenvalue( int k, int j )
{
	const int ell = DLM_Ell( k );
	double falling = 1.0;
	int t;

	if ( ell <= 0 || j < 0 || j > ell ) {
		return 0.0;
	}
	for ( t = 0; t < j; t++ ) {
		falling *= (double)( ell - t );
	}
	return falling / pow( (double)ell, (double)j );
}

static float DLM_InteriorStdRel( int k, const float *q )
{
	const int ell = DLM_Ell( k );
	double sum = 0.0;
	double sum2 = 0.0;
	double maxv = 0.0;
	int w;
	int count = 0;

	for ( w = 1; w < ell; w++ ) {
		const double v = q[w];
		sum += v;
		sum2 += v * v;
		maxv = ( v > maxv ) ? v : maxv;
		count++;
	}
	if ( count <= 0 || maxv <= 0.0 ) {
		return 0.0f;
	}
	{
		const double mean = sum / (double)count;
		const double var = sum2 / (double)count - mean * mean;
		const double std = ( var > 0.0 ) ? sqrt( var ) : 0.0;
		return (float)( std / maxv );
	}
}

dlm_distribution_t *DLM_ComputeExact( int k, int depth )
{
	dlm_distribution_t *dist;
	const int ell = DLM_Ell( k );
	const int dim = ell + 1;
	const int numF = 1 << ell;

	if ( k < 1 || k > DLM_MAX_K || depth < 1 ) {
		return NULL;
	}
	if ( numF <= 0 || numF > 65536 ) {
		return NULL;
	}

	dist = (dlm_distribution_t *)Z_Malloc( sizeof( *dist ) );
	memset( dist, 0, sizeof( *dist ) );
	dist->k = k;
	dist->depth = depth;
	dist->ell = ell;
	dist->dim = dim;
	dist->q = (float *)Z_Malloc( sizeof( float ) * (size_t)dim );
	dist->p = (float *)Z_Malloc( sizeof( float ) * (size_t)numF );

	DLM_EvolveQ( k, depth, dist->q );
	DLM_QToP( k, dist->q, dist->p );
	dist->endpointProb = dist->q[0] + dist->q[ell];
	dist->interiorStdRel = DLM_InteriorStdRel( k, dist->q );

	Com_Printf( "[DLM] exact k=%d n=%d endpoints=%.6f interior_std_rel=%.4f\n",
		k, depth, dist->endpointProb, dist->interiorStdRel );

	return dist;
}

void DLM_Free( dlm_distribution_t *dist )
{
	if ( !dist ) {
		return;
	}
	if ( dist->q ) {
		Z_Free( dist->q );
	}
	if ( dist->p ) {
		Z_Free( dist->p );
	}
	Z_Free( dist );
}

float DLM_CriticalDepthEstimate( int k )
{
	const int ell = DLM_Ell( k );
	float c3 = 0.25f;
	float q0[DLM_MAX_K + 1];
	float v2[DLM_MAX_K + 1];
	int w;

	if ( k < 1 || ell <= 0 ) {
		return 0.0f;
	}

	DLM_InitialQ( k, q0 );
	for ( w = 1; w < ell; w++ ) {
		v2[w] = 1.0f / (float)( ell - 1 );
	}
	v2[0] = -1.0f;
	v2[ell] = -1.0f;

	{
		float dot = 0.0f;
		for ( w = 0; w <= ell; w++ ) {
			dot += q0[w] * v2[w];
		}
		c3 = fabsf( dot );
		if ( c3 < 0.05f ) {
			c3 = 0.25f;
		}
	}

	return (float)ell * logf( 4.0f * c3 );
}
