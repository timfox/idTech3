/*
===========================================================================
QSD observables: active count, R11, clusters, flock, bipartite mutual information.
===========================================================================
*/

#include "dk_qsd/dk_qsd_internal.h"
#include "dk_qsd/dk_qsd.h"

#include "qcommon/qcommon.h"

#include <math.h>
#include <string.h>

float DK_Entropy_Bits( const float *p, int n )
{
	float h = 0.0f;
	int i;

	for ( i = 0; i < n; i++ ) {
		if ( p[i] > 1e-30f ) {
			h -= p[i] * ( logf( p[i] ) / logf( 2.0f ) );
		}
	}
	return h;
}

float DK_Entropy_BinomialHalf( int k )
{
	float p[64];
	int i;
	int n = k;

	if ( k <= 0 ) {
		return 0.0f;
	}
	if ( k > 63 ) {
		k = 63;
	}

	for ( i = 0; i <= k; i++ ) {
		p[i] = 1.0f / (float)( 1 << n );
	}
	return DK_Entropy_Bits( p, k + 1 );
}

static int DK_Obs_Popcount( const byte *x, int N )
{
	int n = 0;
	int i;
	for ( i = 0; i < N; i++ ) {
		n += x[i] & 1;
	}
	return n;
}

static void DK_Obs_FlockStats( const byte *x, int N, int *extent, float *fill, qboolean *singleCluster )
{
	int left = -1;
	int right = -1;
	int n = 0;
	int i;
	int clusters = 0;
	qboolean inCluster = qfalse;

	for ( i = 0; i < N; i++ ) {
		if ( x[i] & 1 ) {
			n++;
			if ( left < 0 ) {
				left = i;
			}
			right = i;
			if ( !inCluster ) {
				clusters++;
				inCluster = qtrue;
			}
		} else {
			inCluster = qfalse;
		}
	}

	if ( left < 0 ) {
		*extent = 0;
		*fill = 0.0f;
		*singleCluster = qfalse;
		return;
	}

	*extent = right - left + 1;
	*fill = ( *extent > 0 ) ? ( (float)n / (float)*extent ) : 0.0f;
	*singleCluster = ( clusters == 1 ) ? qtrue : qfalse;
}

static float DK_Obs_HalfChainMI_Dense( int N, const float *prob )
{
	const int dim = DK_Dense_StateCount( N );
	const int cut = N / 2;
	const int dimL = 1 << cut;
	const int dimR = 1 << ( N - cut );
	float *pL;
	float *pR;
	float *pJoint;
	float hL;
	float hR;
	float hJ;
	int idx;

	if ( N > DK_QSD_DENSE_MAX_N ) {
		return 0.0f;
	}

	pL = (float *)Z_Malloc( sizeof( float ) * (size_t)dimL );
	pR = (float *)Z_Malloc( sizeof( float ) * (size_t)dimR );
	pJoint = (float *)Z_Malloc( sizeof( float ) * (size_t)dimL * (size_t)dimR );
	memset( pL, 0, sizeof( float ) * (size_t)dimL );
	memset( pR, 0, sizeof( float ) * (size_t)dimR );
	memset( pJoint, 0, sizeof( float ) * (size_t)dimL * (size_t)dimR );

	for ( idx = 0; idx < dim; idx++ ) {
		byte x[32];
		int i;
		int iLidx = 0;
		int iRidx = 0;

		DK_Dense_ConfigFromIndex( idx, N, x );
		for ( i = 0; i < cut; i++ ) {
			iLidx |= ( x[i] & 1 ) << i;
		}
		for ( i = cut; i < N; i++ ) {
			iRidx |= ( x[i] & 1 ) << ( i - cut );
		}
		pL[iLidx] += prob[idx];
		pR[iRidx] += prob[idx];
		pJoint[iLidx * dimR + iRidx] += prob[idx];
	}

	hL = DK_Entropy_Bits( pL, dimL );
	hR = DK_Entropy_Bits( pR, dimR );
	hJ = DK_Entropy_Bits( pJoint, dimL * dimR );

	Z_Free( pJoint );
	Z_Free( pR );
	Z_Free( pL );

	return hL + hR - hJ;
}

void DK_Obs_FromProb( int N, const float *prob, dk_qsd_observables_t *obs )
{
	const int dim = DK_Dense_StateCount( N );
	float meanN = 0.0f;
	float nnNum = 0.0f;
	float nnDen = 0.0f;
	int idx;
	int i;

	memset( obs, 0, sizeof( *obs ) );

	for ( idx = 0; idx < dim; idx++ ) {
		byte x[32];
		int n;
		int extent;
		float fill;
		qboolean single;

		if ( prob[idx] <= 0.0f ) {
			continue;
		}

		DK_Dense_ConfigFromIndex( idx, N, x );
		n = DK_Obs_Popcount( x, N );
		meanN += (float)n * prob[idx];

		DK_Obs_FlockStats( x, N, &extent, &fill, &single );
		obs->flockExtentMean += (float)extent * prob[idx];
		obs->flockFillMean += fill * prob[idx];
		if ( single ) {
			obs->singleClusterFrac += prob[idx];
		}
	}

	for ( i = 0; i < N - 1; i++ ) {
		float ei = 0.0f;
		float ej = 0.0f;
		float eij = 0.0f;

		for ( idx = 0; idx < dim; idx++ ) {
			byte x[32];
			float px;
			float pip1;

			DK_Dense_ConfigFromIndex( idx, N, x );
			px = (float)( x[i] & 1 );
			pip1 = (float)( x[i + 1] & 1 );
			ei += px * prob[idx];
			ej += pip1 * prob[idx];
			eij += px * pip1 * prob[idx];
		}
		nnNum += eij;
		nnDen += ei * ej;
	}

	obs->meanActive = meanN;
	obs->r11 = ( nnDen > 1e-30f ) ? ( nnNum / nnDen ) : 0.0f;
	obs->halfChainMI = DK_Obs_HalfChainMI_Dense( N, prob );
}

void DK_Obs_FromMps( const dk_mps_t *mps, int numSamples, unsigned seed, dk_qsd_observables_t *obs )
{
	byte x[256];
	float *ei;
	float *ej;
	float *eij;
	float meanN = 0.0f;
	float nnNum = 0.0f;
	float nnDen = 0.0f;
	unsigned rng = seed ? seed : 1u;
	int s;
	int i;

	memset( obs, 0, sizeof( *obs ) );
	if ( numSamples <= 0 ) {
		numSamples = 4096;
	}

	if ( mps->N < 2 ) {
		return;
	}

	ei = (float *)Z_Malloc( sizeof( float ) * (size_t)( mps->N - 1 ) );
	ej = (float *)Z_Malloc( sizeof( float ) * (size_t)( mps->N - 1 ) );
	eij = (float *)Z_Malloc( sizeof( float ) * (size_t)( mps->N - 1 ) );
	memset( ei, 0, sizeof( float ) * (size_t)( mps->N - 1 ) );
	memset( ej, 0, sizeof( float ) * (size_t)( mps->N - 1 ) );
	memset( eij, 0, sizeof( float ) * (size_t)( mps->N - 1 ) );

	for ( s = 0; s < numSamples; s++ ) {
		int n;
		int extent;
		float fill;
		qboolean single;

		if ( !DK_Mps_Sample( mps, x, &rng ) ) {
			continue;
		}

		n = DK_Obs_Popcount( x, mps->N );
		meanN += (float)n;

		for ( i = 0; i < mps->N - 1; i++ ) {
			const float xi = (float)( x[i] & 1 );
			const float xj = (float)( x[i + 1] & 1 );
			ei[i] += xi;
			ej[i] += xj;
			eij[i] += xi * xj;
		}

		DK_Obs_FlockStats( x, mps->N, &extent, &fill, &single );
		obs->flockExtentMean += (float)extent;
		obs->flockFillMean += fill;
		if ( single ) {
			obs->singleClusterFrac += 1.0f;
		}
	}

	{
		const float inv = 1.0f / (float)numSamples;
		for ( i = 0; i < mps->N - 1; i++ ) {
			nnNum += eij[i] * inv;
			nnDen += ( ei[i] * inv ) * ( ej[i] * inv );
		}
		obs->meanActive = meanN * inv;
		obs->r11 = ( nnDen > 1e-30f ) ? ( nnNum / nnDen ) : 0.0f;
		obs->halfChainMI = DK_Qsd_BinaryEntropy( 0.5f );
		obs->flockExtentMean *= inv;
		obs->flockFillMean *= inv;
		obs->singleClusterFrac *= inv;
	}

	Z_Free( eij );
	Z_Free( ej );
	Z_Free( ei );
}
