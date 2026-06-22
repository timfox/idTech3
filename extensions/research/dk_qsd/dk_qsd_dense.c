/*
===========================================================================
Exact dense transfer matrix and QSD power iteration (small N).
===========================================================================
*/

#include "dk_qsd/dk_qsd_internal.h"

#include "qcommon/qcommon.h"

#include <math.h>
#include <string.h>

int DK_Dense_ConfigIndex( const byte *x, int N )
{
	int idx = 0;
	int i;

	for ( i = 0; i < N; i++ ) {
		idx |= ( x[i] & 1 ) << i;
	}
	return idx;
}

void DK_Dense_ConfigFromIndex( int idx, int N, byte *x )
{
	int i;

	for ( i = 0; i < N; i++ ) {
		x[i] = (byte)( ( idx >> i ) & 1 );
	}
}

int DK_Dense_StateCount( int N )
{
	return 1 << N;
}

static float DK_Dense_StepProb( int N, float p, const byte *xIn, const byte *xOut, int mIdx )
{
	float W[2][2][2];
	float V[2][2];
	byte m[32];
	int k;

	if ( N > 32 ) {
		return 0.0f;
	}

	DK_Kernels_Fill( p, W, V );

	for ( k = 0; k < N - 1; k++ ) {
		m[k] = (byte)( ( mIdx >> k ) & 1 );
	}

	{
		float prob = 1.0f;
		for ( k = 0; k < N - 1; k++ ) {
			prob *= W[xIn[k] & 1][xIn[k + 1] & 1][m[k] & 1];
		}
		prob *= V[m[0] & 1][xOut[0] & 1];
		prob *= V[m[N - 2] & 1][xOut[N - 1] & 1];
		for ( k = 1; k < N - 1; k++ ) {
			prob *= W[m[k - 1] & 1][m[k] & 1][xOut[k] & 1];
		}
		return prob;
	}
}

static void DK_Dense_ApplyTransfer( int N, float p, const float *vIn, float *vOut )
{
	const int dim = DK_Dense_StateCount( N );
	const int mDim = 1 << ( N - 1 );
	byte xIn[32];
	byte xOut[32];
	int xi;

	memset( vOut, 0, sizeof( float ) * (size_t)dim );

	for ( xi = 0; xi < dim; xi++ ) {
		const float vin = vIn[xi];
		int m;

		if ( vin <= 0.0f ) {
			continue;
		}

		DK_Dense_ConfigFromIndex( xi, N, xIn );
		for ( m = 0; m < mDim; m++ ) {
			int xo;
			for ( xo = 0; xo < dim; xo++ ) {
				float prob;

				DK_Dense_ConfigFromIndex( xo, N, xOut );
				prob = DK_Dense_StepProb( N, p, xIn, xOut, m );
				if ( prob > 0.0f ) {
					vOut[xo] += prob * vin;
				}
			}
		}
	}
}

void DK_Dense_BuildTransfer( int N, float p, float *T )
{
	const int dim = DK_Dense_StateCount( N );
	const int mDim = 1 << ( N - 1 );
	byte xIn[32];
	byte xOut[32];
	int xi;
	int xo;
	int m;

	memset( T, 0, sizeof( float ) * (size_t)dim * (size_t)dim );

	for ( xi = 0; xi < dim; xi++ ) {
		DK_Dense_ConfigFromIndex( xi, N, xIn );
		for ( xo = 0; xo < dim; xo++ ) {
			float sum = 0.0f;
			DK_Dense_ConfigFromIndex( xo, N, xOut );
			for ( m = 0; m < mDim; m++ ) {
				sum += DK_Dense_StepProb( N, p, xIn, xOut, m );
			}
			T[xo * dim + xi] = sum;
		}
	}
}

static float DK_Dense_FlatNorm( const float *v, int dim )
{
	float s = 0.0f;
	int i;

	for ( i = 0; i < dim; i++ ) {
		s += v[i];
	}
	return s;
}

static float DK_Dense_L2Norm( const float *v, int dim )
{
	float s = 0.0f;
	int i;

	for ( i = 0; i < dim; i++ ) {
		s += v[i] * v[i];
	}
	return sqrtf( s );
}

static float DK_Dense_Overlap( const float *a, const float *b, int dim )
{
	float dot = 0.0f;
	float na;
	float nb;
	int i;

	for ( i = 0; i < dim; i++ ) {
		dot += a[i] * b[i];
	}
	na = DK_Dense_L2Norm( a, dim );
	nb = DK_Dense_L2Norm( b, dim );
	if ( na < 1e-30f || nb < 1e-30f ) {
		return 0.0f;
	}
	return fabsf( dot / ( na * nb ) );
}

void DK_Dense_PowerIterate( int N, float p, float *prob, int maxIter, float tol,
	float *outLambda, int *outIter, float *outOverlap, qboolean *outConverged )
{
	const int dim = DK_Dense_StateCount( N );
	float *v;
	float *vNew;
	float zPrev;
	float z;
	float overlap;
	int iter;
	int i;

	if ( N > DK_QSD_DENSE_MAX_N ) {
		Com_Printf( S_COLOR_YELLOW "DK_QSD: dense solver limited to N<=%d\n", DK_QSD_DENSE_MAX_N );
		if ( outConverged ) {
			*outConverged = qfalse;
		}
		return;
	}

	v = (float *)Z_Malloc( sizeof( float ) * (size_t)dim );
	vNew = (float *)Z_Malloc( sizeof( float ) * (size_t)dim );

	for ( i = 0; i < dim; i++ ) {
		v[i] = ( i == 0 ) ? 0.0f : 1.0f / (float)( dim - 1 );
	}

	zPrev = DK_Dense_FlatNorm( v, dim );
	overlap = 0.0f;

	for ( iter = 0; iter < maxIter; iter++ ) {
		DK_Dense_ApplyTransfer( N, p, v, vNew );

		vNew[0] = 0.0f;
		z = DK_Dense_FlatNorm( vNew, dim );
		if ( z <= 0.0f ) {
			break;
		}
		for ( i = 0; i < dim; i++ ) {
			vNew[i] /= z;
		}

		overlap = DK_Dense_Overlap( v, vNew, dim );
		memcpy( v, vNew, sizeof( float ) * (size_t)dim );

		if ( outLambda ) {
			*outLambda = z / zPrev;
		}
		zPrev = z;

		if ( 1.0f - overlap < tol ) {
			break;
		}
	}

	memcpy( prob, v, sizeof( float ) * (size_t)dim );

	if ( outIter ) {
		*outIter = iter + 1;
	}
	if ( outOverlap ) {
		*outOverlap = overlap;
	}
	if ( outConverged ) {
		*outConverged = ( 1.0f - overlap < tol ) ? qtrue : qfalse;
	}

	Z_Free( vNew );
	Z_Free( v );
}
