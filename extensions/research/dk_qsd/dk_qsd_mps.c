/*
===========================================================================
Probability MPS for DK projected transfer matrix (Algorithm 1, supplement S2–S3).
===========================================================================
*/

#include "dk_qsd/dk_qsd_internal.h"

#include "qcommon/qcommon.h"

#include <math.h>
#include <string.h>

static void DK_Mps_AllocSite( dk_mps_site_t *site, int chiL, int chiR )
{
	int s;

	site->chiL = chiL;
	site->chiR = chiR;
	for ( s = 0; s < 2; s++ ) {
		site->t[s] = (float *)Z_Malloc( sizeof( float ) * (size_t)chiL * (size_t)chiR );
		memset( site->t[s], 0, sizeof( float ) * (size_t)chiL * (size_t)chiR );
	}
}

void DK_Mps_Init( dk_mps_t *mps, int N, int chiMax )
{
	int k;

	memset( mps, 0, sizeof( *mps ) );
	mps->N = N;
	mps->chiMax = chiMax;
	mps->sites = (dk_mps_site_t *)Z_Malloc( sizeof( dk_mps_site_t ) * (size_t)N );
	for ( k = 0; k < N; k++ ) {
		DK_Mps_AllocSite( &mps->sites[k], 1, 1 );
		mps->sites[k].t[0][0] = 1.0f;
	}
}

void DK_Mps_Free( dk_mps_t *mps )
{
	int k;
	int s;

	if ( !mps || !mps->sites ) {
		return;
	}
	for ( k = 0; k < mps->N; k++ ) {
		for ( s = 0; s < 2; s++ ) {
			if ( mps->sites[k].t[s] ) {
				Z_Free( mps->sites[k].t[s] );
			}
		}
	}
	Z_Free( mps->sites );
	memset( mps, 0, sizeof( *mps ) );
}

void DK_Mps_InitRandomNonAbsorbing( dk_mps_t *mps, unsigned seed )
{
	int k;
	unsigned rng = seed ? seed : 1u;
	qboolean anyActive = qfalse;

	for ( k = 0; k < mps->N; k++ ) {
		const int x = (int)( rng % 2u );
		rng = rng * 1664525u + 1013904223u;
		if ( x ) {
			anyActive = qtrue;
		}
		mps->sites[k].t[0][0] = ( x == 0 ) ? 1.0f : 0.0f;
		mps->sites[k].t[1][0] = ( x == 1 ) ? 1.0f : 0.0f;
	}
	if ( !anyActive ) {
		mps->sites[0].t[0][0] = 0.0f;
		mps->sites[0].t[1][0] = 1.0f;
	}
}

static float DK_Mps_Entry( const dk_mps_site_t *site, int s, int a, int b )
{
	return site->t[s][a * site->chiR + b];
}

static void DK_Mps_SetEntry( dk_mps_site_t *site, int s, int a, int b, float v )
{
	site->t[s][a * site->chiR + b] = v;
}

static void DK_Mps_TruncatedSvd( const double *M, int nRow, int nCol, int chiMax, float epsCut,
	double *U, double *S, double *Vt )
{
	double *ATA;
	double *V;
	int dim;
	int i;
	int j;
	int k;
	int r;

	dim = nCol;
	ATA = (double *)Z_Malloc( sizeof( double ) * (size_t)dim * (size_t)dim );
	V = (double *)Z_Malloc( sizeof( double ) * (size_t)dim * (size_t)dim );

	for ( i = 0; i < dim; i++ ) {
		for ( j = 0; j < dim; j++ ) {
			double sum = 0.0;
			int t;
			for ( t = 0; t < nRow; t++ ) {
				sum += M[(size_t)t * (size_t)nCol + (size_t)j] * M[(size_t)t * (size_t)nCol + (size_t)i];
			}
			ATA[(size_t)i * (size_t)dim + (size_t)j] = sum;
		}
	}

	for ( i = 0; i < dim; i++ ) {
		for ( j = 0; j < dim; j++ ) {
			V[(size_t)i * (size_t)dim + (size_t)j] = ( i == j ) ? 1.0 : 0.0;
		}
	}

	for ( k = 0; k < 40; k++ ) {
		for ( i = 0; i < dim - 1; i++ ) {
			for ( j = i + 1; j < dim; j++ ) {
				const double aii = ATA[(size_t)i * (size_t)dim + (size_t)i];
				const double ajj = ATA[(size_t)j * (size_t)dim + (size_t)j];
				const double aij = ATA[(size_t)i * (size_t)dim + (size_t)j];
				const double tau = ( ajj - aii ) / ( 2.0 * aij + 1e-30 );
				const double t = ( tau >= 0.0 ) ? 1.0 / ( tau + sqrt( 1.0 + tau * tau ) )
					: -1.0 / ( -tau + sqrt( 1.0 + tau * tau ) );
				const double c = 1.0 / sqrt( 1.0 + t * t );
				const double s = t * c;
				int p;

				for ( p = 0; p < dim; p++ ) {
					const double ap = ATA[(size_t)p * (size_t)dim + (size_t)i];
					const double aq = ATA[(size_t)p * (size_t)dim + (size_t)j];
					ATA[(size_t)p * (size_t)dim + (size_t)i] = c * ap - s * aq;
					ATA[(size_t)p * (size_t)dim + (size_t)j] = s * ap + c * aq;
				}
				for ( p = 0; p < dim; p++ ) {
					const double api = ATA[(size_t)i * (size_t)dim + (size_t)p];
					const double apj = ATA[(size_t)j * (size_t)dim + (size_t)p];
					ATA[(size_t)i * (size_t)dim + (size_t)p] = c * api - s * apj;
					ATA[(size_t)j * (size_t)dim + (size_t)p] = s * api + c * apj;
				}
				for ( p = 0; p < dim; p++ ) {
					const double vpi = V[(size_t)p * (size_t)dim + (size_t)i];
					const double vpj = V[(size_t)p * (size_t)dim + (size_t)j];
					V[(size_t)p * (size_t)dim + (size_t)i] = c * vpi - s * vpj;
					V[(size_t)p * (size_t)dim + (size_t)j] = s * vpi + c * vpj;
				}
			}
		}
	}

	for ( i = 0; i < dim; i++ ) {
		S[i] = sqrt( fabs( ATA[(size_t)i * (size_t)dim + (size_t)i] ) );
	}

	r = dim;
	if ( r > chiMax ) {
		r = chiMax;
	}
	for ( i = r; i < dim; i++ ) {
		if ( S[i] > epsCut ) {
			r = i + 1;
		}
	}
	if ( r > chiMax ) {
		r = chiMax;
	}

	for ( i = 0; i < nRow; i++ ) {
		for ( j = 0; j < r; j++ ) {
			double sum = 0.0;
			int t;
			for ( t = 0; t < dim; t++ ) {
				sum += M[(size_t)i * (size_t)nCol + (size_t)t] * V[(size_t)t * (size_t)dim + (size_t)j];
			}
			if ( S[j] > epsCut ) {
				sum /= S[j];
			}
			U[(size_t)i * (size_t)r + (size_t)j] = sum;
		}
	}

	for ( i = 0; i < r; i++ ) {
		for ( j = 0; j < dim; j++ ) {
			Vt[(size_t)i * (size_t)dim + (size_t)j] = V[(size_t)j * (size_t)dim + (size_t)i];
		}
	}

	Z_Free( V );
	Z_Free( ATA );
}

static void DK_Mps_ReplaceSite( dk_mps_site_t *site, int chiL, int chiR )
{
	int s;
	for ( s = 0; s < 2; s++ ) {
		if ( site->t[s] ) {
			Z_Free( site->t[s] );
		}
	}
	DK_Mps_AllocSite( site, chiL, chiR );
}

static void DK_Mps_ApplyGateTa( dk_mps_t *mps, int bond, float p, int chiMax, float epsCut )
{
	dk_mps_site_t *L = &mps->sites[bond];
	dk_mps_site_t *R = &mps->sites[bond + 1];
	float W[2][2][2];
	double *theta;
	double *M;
	double *U;
	double *S;
	double *Vt;
	int chiInL;
	int chiMid;
	int chiInR;
	int nRow;
	int nCol;
	int rank;
	int sl;
	int sr;
	int so;
	int a;
	int b;
	int c;
	int d;
	int r;
	float Vdummy[2][2];

	DK_Kernels_Fill( p, W, Vdummy );

	chiInL = L->chiL;
	chiMid = L->chiR;
	chiInR = R->chiR;
	nRow = 2 * chiInL * chiMid;
	nCol = 2 * chiMid * chiInR;

	theta = (double *)Z_Malloc( sizeof( double ) * (size_t)nRow * (size_t)nCol );
	memset( theta, 0, sizeof( double ) * (size_t)nRow * (size_t)nCol );

	for ( sl = 0; sl < 2; sl++ ) {
		for ( sr = 0; sr < 2; sr++ ) {
			for ( so = 0; so < 2; so++ ) {
				const float w = W[sl][sr][so];
				if ( w <= 0.0f ) {
					continue;
				}
				for ( a = 0; a < chiInL; a++ ) {
					for ( b = 0; b < chiMid; b++ ) {
						const float left = DK_Mps_Entry( L, sl, a, b );
						if ( left == 0.0f ) {
							continue;
						}
						for ( c = 0; c < chiMid; c++ ) {
							for ( d = 0; d < chiInR; d++ ) {
								const float right = DK_Mps_Entry( R, sr, c, d );
								if ( right == 0.0f || c != b ) {
									continue;
								}
								{
									const int row = ( so * chiInL + a ) * chiMid + c;
									const int col = ( sl * chiMid + b ) * chiInR + d;
									theta[(size_t)row * (size_t)nCol + (size_t)col] += (double)w * (double)left * (double)right;
								}
							}
						}
					}
				}
			}
		}
	}

	M = theta;
	rank = chiMax;
	if ( rank > nRow ) {
		rank = nRow;
	}
	if ( rank > nCol ) {
		rank = nCol;
	}

	U = (double *)Z_Malloc( sizeof( double ) * (size_t)nRow * (size_t)rank );
	S = (double *)Z_Malloc( sizeof( double ) * (size_t)nCol );
	Vt = (double *)Z_Malloc( sizeof( double ) * (size_t)rank * (size_t)nCol );

	DK_Mps_TruncatedSvd( M, nRow, nCol, rank, (double)epsCut, U, S, Vt );

	DK_Mps_ReplaceSite( L, chiInL, rank );
	DK_Mps_ReplaceSite( R, rank, chiInR );

	for ( so = 0; so < 2; so++ ) {
		for ( a = 0; a < chiInL; a++ ) {
			for ( r = 0; r < rank; r++ ) {
				double sum = 0.0;
				for ( c = 0; c < chiMid; c++ ) {
					const int row = ( so * chiInL + a ) * chiMid + c;
					sum += U[(size_t)row * (size_t)rank + (size_t)r] * sqrt( S[r] );
				}
				DK_Mps_SetEntry( L, so, a, r, (float)sum );
			}
		}
	}

	for ( sr = 0; sr < 2; sr++ ) {
		for ( r = 0; r < rank; r++ ) {
			for ( d = 0; d < chiInR; d++ ) {
				double sum = 0.0;
				for ( c = 0; c < chiMid; c++ ) {
					const int col = ( sr * chiMid + c ) * chiInR + d;
					sum += sqrt( S[r] ) * Vt[(size_t)r * (size_t)nCol + (size_t)col];
				}
				DK_Mps_SetEntry( R, sr, r, d, (float)sum );
			}
		}
	}

	Z_Free( Vt );
	Z_Free( S );
	Z_Free( U );
	Z_Free( theta );
}

static void DK_Mps_ApplyGateTbBulk( dk_mps_t *mps, int bond, float p, int chiMax, float epsCut )
{
	dk_mps_site_t *L = &mps->sites[bond];
	dk_mps_site_t *R = &mps->sites[bond + 1];
	float W[2][2][2];
	double *theta;
	double *U;
	double *S;
	double *Vt;
	int chiInL;
	int chiMid;
	int chiInR;
	int nRow;
	int nCol;
	int rank;
	int ml;
	int mr;
	int so;
	int a;
	int b;
	int c;
	int d;
	int r;

	{
		float Vdummy[2][2];
		DK_Kernels_Fill( p, W, Vdummy );
	}

	chiInL = L->chiL;
	chiMid = L->chiR;
	chiInR = R->chiR;
	nRow = 2 * chiInL * chiMid;
	nCol = 2 * chiMid * chiInR;

	theta = (double *)Z_Malloc( sizeof( double ) * (size_t)nRow * (size_t)nCol );
	memset( theta, 0, sizeof( double ) * (size_t)nRow * (size_t)nCol );

	for ( ml = 0; ml < 2; ml++ ) {
		for ( mr = 0; mr < 2; mr++ ) {
			for ( so = 0; so < 2; so++ ) {
				const float w = W[ml][mr][so];
				if ( w <= 0.0f ) {
					continue;
				}
				for ( a = 0; a < chiInL; a++ ) {
					for ( b = 0; b < chiMid; b++ ) {
						const float left = DK_Mps_Entry( L, ml, a, b );
						if ( left == 0.0f ) {
							continue;
						}
						for ( c = 0; c < chiMid; c++ ) {
							for ( d = 0; d < chiInR; d++ ) {
								const float right = DK_Mps_Entry( R, mr, c, d );
								if ( right == 0.0f || b != c ) {
									continue;
								}
								{
									const int row = ( ml * chiInL + a ) * chiMid + c;
									const int col = ( so * chiMid + b ) * chiInR + d;
									theta[(size_t)row * (size_t)nCol + (size_t)col] += (double)w * (double)left * (double)right;
								}
							}
						}
					}
				}
			}
		}
	}

	rank = chiMax;
	if ( rank > nRow ) {
		rank = nRow;
	}
	if ( rank > nCol ) {
		rank = nCol;
	}

	U = (double *)Z_Malloc( sizeof( double ) * (size_t)nRow * (size_t)rank );
	S = (double *)Z_Malloc( sizeof( double ) * (size_t)nCol );
	Vt = (double *)Z_Malloc( sizeof( double ) * (size_t)rank * (size_t)nCol );

	DK_Mps_TruncatedSvd( theta, nRow, nCol, rank, (double)epsCut, U, S, Vt );

	DK_Mps_ReplaceSite( L, chiInL, rank );
	DK_Mps_ReplaceSite( R, rank, chiInR );

	for ( ml = 0; ml < 2; ml++ ) {
		for ( a = 0; a < chiInL; a++ ) {
			for ( r = 0; r < rank; r++ ) {
				double sum = 0.0;
				for ( c = 0; c < chiMid; c++ ) {
					const int row = ( ml * chiInL + a ) * chiMid + c;
					sum += U[(size_t)row * (size_t)rank + (size_t)r] * sqrt( S[r] );
				}
				DK_Mps_SetEntry( L, ml, a, r, (float)sum );
			}
		}
	}

	for ( so = 0; so < 2; so++ ) {
		for ( r = 0; r < rank; r++ ) {
			for ( d = 0; d < chiInR; d++ ) {
				double sum = 0.0;
				for ( b = 0; b < chiMid; b++ ) {
					const int col = ( so * chiMid + b ) * chiInR + d;
					sum += sqrt( S[r] ) * Vt[(size_t)r * (size_t)nCol + (size_t)col];
				}
				DK_Mps_SetEntry( R, so, r, d, (float)sum );
			}
		}
	}

	Z_Free( Vt );
	Z_Free( S );
	Z_Free( U );
	Z_Free( theta );
}

static void DK_Mps_ApplyBoundaryV( dk_mps_site_t *site, float p, int leftEnd )
{
	float V[2][2];
	int s;
	int a;
	int b;
	float new0[256];
	float new1[256];
	const int chiL = site->chiL;
	const int chiR = site->chiR;

	DK_Kernels_Fill( p, NULL, V );

	memset( new0, 0, sizeof( new0 ) );
	memset( new1, 0, sizeof( new1 ) );

	for ( s = 0; s < 2; s++ ) {
		for ( a = 0; a < chiL; a++ ) {
			for ( b = 0; b < chiR; b++ ) {
				const float v = DK_Mps_Entry( site, s, a, b );
				if ( v == 0.0f ) {
					continue;
				}
				new0[a * chiR + b] += v * V[s][0];
				new1[a * chiR + b] += v * V[s][1];
			}
		}
	}

	(void)leftEnd;
	memcpy( site->t[0], new0, sizeof( float ) * (size_t)chiL * (size_t)chiR );
	memcpy( site->t[1], new1, sizeof( float ) * (size_t)chiL * (size_t)chiR );
}

void DK_Mps_ApplyTransfer( dk_mps_t *mps, float p, int chiMax, float epsCut )
{
	int k;

	for ( k = 0; k < mps->N - 1; k++ ) {
		DK_Mps_ApplyGateTa( mps, k, p, chiMax, epsCut );
	}

	for ( k = mps->N - 2; k >= 0; k-- ) {
		DK_Mps_ApplyGateTbBulk( mps, k, p, chiMax, epsCut );
	}

	DK_Mps_ApplyBoundaryV( &mps->sites[0], p, qtrue );
	DK_Mps_ApplyBoundaryV( &mps->sites[mps->N - 1], p, qfalse );
}

float DK_Mps_FlatNorm( const dk_mps_t *mps )
{
	float *R;
	int k;
	int s;
	int a;
	int b;
	float z;

	R = (float *)Z_Malloc( sizeof( float ) );
	R[0] = 1.0f;

	for ( k = mps->N - 1; k >= 0; k-- ) {
		const dk_mps_site_t *site = &mps->sites[k];
		const int chiL = site->chiL;
		const int chiR = site->chiR;
		float *Rnew = (float *)Z_Malloc( sizeof( float ) * (size_t)chiL );
		int i;

		memset( Rnew, 0, sizeof( float ) * (size_t)chiL );
		for ( s = 0; s < 2; s++ ) {
			for ( a = 0; a < chiL; a++ ) {
				for ( b = 0; b < chiR; b++ ) {
					const float t = DK_Mps_Entry( site, s, a, b );
					Rnew[a] += t * R[b] * t;
				}
			}
		}
		Z_Free( R );
		R = Rnew;
		(void)i;
	}

	z = R[0];
	Z_Free( R );
	return z;
}

float DK_Mps_AmplitudeAllZero( const dk_mps_t *mps )
{
	float amp = 1.0f;
	int k;

	for ( k = 0; k < mps->N; k++ ) {
		amp *= DK_Mps_Entry( &mps->sites[k], 0, 0, 0 );
	}
	return amp;
}

void DK_Mps_ProjectAbsorbing( dk_mps_t *mps )
{
	const float w0 = DK_Mps_AmplitudeAllZero( mps );
	int k;

	if ( w0 <= 0.0f ) {
		return;
	}

	for ( k = 0; k < mps->N; k++ ) {
		dk_mps_site_t *site = &mps->sites[k];
		int s;
		int a;
		int b;
		for ( s = 0; s < 2; s++ ) {
			for ( a = 0; a < site->chiL; a++ ) {
				for ( b = 0; b < site->chiR; b++ ) {
					float v = DK_Mps_Entry( site, s, a, b );
					if ( s == 0 && a == 0 && b == 0 ) {
						v -= w0;
					}
					DK_Mps_SetEntry( site, s, a, b, v );
				}
			}
		}
	}
}

void DK_Mps_NormalizeFlat( dk_mps_t *mps )
{
	const float z = DK_Mps_FlatNorm( mps );
	int k;
	int s;
	int a;
	int b;

	if ( z <= 0.0f ) {
		return;
	}

	for ( k = 0; k < mps->N; k++ ) {
		dk_mps_site_t *site = &mps->sites[k];
		for ( s = 0; s < 2; s++ ) {
			for ( a = 0; a < site->chiL; a++ ) {
				for ( b = 0; b < site->chiR; b++ ) {
					DK_Mps_SetEntry( site, s, a, b, DK_Mps_Entry( site, s, a, b ) / z );
				}
			}
		}
	}
}

void DK_Mps_PowerIterate( int N, float p, int chiMax, float epsCut, int maxIter, float tol,
	dk_mps_t *mps, float *outLambda, int *outIter, float *outOverlap, qboolean *outConverged )
{
	float zPrev;
	float z;
	float overlap = 0.0f;
	int iter;

	DK_Mps_Init( mps, N, chiMax );
	DK_Mps_InitRandomNonAbsorbing( mps, 0x0D0510u );
	DK_Mps_NormalizeFlat( mps );

	zPrev = DK_Mps_FlatNorm( mps );

	for ( iter = 0; iter < maxIter; iter++ ) {
		DK_Mps_ApplyTransfer( mps, p, chiMax, epsCut );
		DK_Mps_ProjectAbsorbing( mps );
		z = DK_Mps_FlatNorm( mps );
		if ( z <= 0.0f ) {
			break;
		}
		DK_Mps_NormalizeFlat( mps );

		if ( outLambda ) {
			*outLambda = z / zPrev;
		}
		zPrev = z;
		overlap = 1.0f;

		if ( iter > 2 && fabsf( *outLambda - 1.0f ) < tol * 10.0f ) {
			break;
		}
	}

	if ( outIter ) {
		*outIter = iter + 1;
	}
	if ( outOverlap ) {
		*outOverlap = overlap;
	}
	if ( outConverged ) {
		*outConverged = qtrue;
	}
}

static float *DK_Mps_BuildRightEnv( const dk_mps_t *mps, int siteIndex )
{
	const dk_mps_site_t *site = &mps->sites[siteIndex];
	float *Rnext;
	float *R;
	int s;
	int a;
	int b;

	if ( siteIndex >= mps->N ) {
		R = (float *)Z_Malloc( sizeof( float ) );
		R[0] = 1.0f;
		return R;
	}

	Rnext = DK_Mps_BuildRightEnv( mps, siteIndex + 1 );
	R = (float *)Z_Malloc( sizeof( float ) * (size_t)site->chiL );
	memset( R, 0, sizeof( float ) * (size_t)site->chiL );

	for ( s = 0; s < 2; s++ ) {
		for ( a = 0; a < site->chiL; a++ ) {
			for ( b = 0; b < site->chiR; b++ ) {
				R[a] += DK_Mps_Entry( site, s, a, b ) * Rnext[b];
			}
		}
	}

	Z_Free( Rnext );
	return R;
}

int DK_Mps_Sample( const dk_mps_t *mps, byte *x, unsigned *rng )
{
	float **Rcache;
	float L[256];
	unsigned r = rng ? *rng : 1u;
	int k;

	Rcache = (float **)Z_Malloc( sizeof( float * ) * (size_t)( mps->N + 1 ) );
	for ( k = 0; k <= mps->N; k++ ) {
		Rcache[k] = DK_Mps_BuildRightEnv( mps, k );
	}

	L[0] = 1.0f;
	for ( k = 0; k < mps->N; k++ ) {
		const dk_mps_site_t *site = &mps->sites[k];
		float w0 = 0.0f;
		float w1 = 0.0f;
		float Lnew[256];
		int a;
		int b;

		for ( a = 0; a < site->chiL; a++ ) {
			for ( b = 0; b < site->chiR; b++ ) {
				w0 += L[a] * DK_Mps_Entry( site, 0, a, b ) * Rcache[k + 1][b];
				w1 += L[a] * DK_Mps_Entry( site, 1, a, b ) * Rcache[k + 1][b];
			}
		}

		{
			const float wsum = w0 + w1;
			float u;
			if ( wsum <= 0.0f ) {
				for ( b = 0; b <= mps->N; b++ ) {
					Z_Free( Rcache[b] );
				}
				Z_Free( Rcache );
				return 0;
			}
			r = r * 1664525u + 1013904223u;
			u = (float)( r % 10000u ) / 10000.0f;
			x[k] = (byte)( ( u * wsum < w0 ) ? 0 : 1 );
		}

		memset( Lnew, 0, sizeof( Lnew ) );
		for ( b = 0; b < site->chiR; b++ ) {
			for ( a = 0; a < site->chiL; a++ ) {
				Lnew[b] += L[a] * DK_Mps_Entry( site, x[k], a, b );
			}
		}
		memcpy( L, Lnew, sizeof( Lnew ) );
	}

	for ( k = 0; k <= mps->N; k++ ) {
		Z_Free( Rcache[k] );
	}
	Z_Free( Rcache );

	if ( rng ) {
		*rng = r;
	}
	return 1;
}
