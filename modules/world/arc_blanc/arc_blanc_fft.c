/*
 * Radix-2 Cooley-Tukey FFT for Arc Blanc ocean IFFT (Tessendorf-style grids).
 */
#include "arc_blanc_internal.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void ab_bit_reverse( abComplex_t *data, int n ) {
	int i, j, bit, n2;

	n2 = n >> 1;
	j = 0;
	for ( i = 0; i < n - 1; i++ ) {
		if ( i < j ) {
			abComplex_t tmp = data[i];
			data[i] = data[j];
			data[j] = tmp;
		}
		bit = n2;
		while ( j & bit ) {
			j &= ~bit;
			bit >>= 1;
		}
		j |= bit;
	}
}

void AB_FFT_Complex1D( abComplex_t *data, int n, qboolean inverse ) {
	int len, half, i, j;
	float ang, wr, wi;
	const float sign = inverse ? 1.0f : -1.0f;

	if ( n < 2 || ( n & ( n - 1 ) ) != 0 ) {
		return;
	}

	ab_bit_reverse( data, n );

	for ( len = 2; len <= n; len <<= 1 ) {
		half = len >> 1;
		ang = sign * 2.0f * (float)M_PI / (float)len;
		wr = cosf( ang );
		wi = sinf( ang );
		for ( i = 0; i < n; i += len ) {
			float wpr = 1.0f;
			float wpi = 0.0f;
			float tr, ti;
			for ( j = 0; j < half; j++ ) {
				abComplex_t u = data[i + j];
				abComplex_t v;
				v.re = data[i + j + half].re * wpr - data[i + j + half].im * wpi;
				v.im = data[i + j + half].re * wpi + data[i + j + half].im * wpr;
				data[i + j].re = u.re + v.re;
				data[i + j].im = u.im + v.im;
				data[i + j + half].re = u.re - v.re;
				data[i + j + half].im = u.im - v.im;
				tr = wpr * wr - wpi * wi;
				ti = wpr * wi + wpi * wr;
				wpr = tr;
				wpi = ti;
			}
		}
	}

	if ( inverse ) {
		const float invN = 1.0f / (float)n;
		for ( i = 0; i < n; i++ ) {
			data[i].re *= invN;
			data[i].im *= invN;
		}
	}
}

void AB_FFT_IFFT2D( abComplex_t *grid, int n ) {
	abComplex_t row[AB_MAX_GRID_N];
	int x, y;

	for ( y = 0; y < n; y++ ) {
		memcpy( row, grid + y * n, (size_t)n * sizeof( row[0] ) );
		AB_FFT_Complex1D( row, n, qtrue );
		memcpy( grid + y * n, row, (size_t)n * sizeof( row[0] ) );
	}

	for ( x = 0; x < n; x++ ) {
		for ( y = 0; y < n; y++ ) {
			row[y] = grid[y * n + x];
		}
		AB_FFT_Complex1D( row, n, qtrue );
		for ( y = 0; y < n; y++ ) {
			grid[y * n + x] = row[y];
		}
	}
}

/*
 * Theorem 1 (Arc Blanc §3.5): F^{-1}(X + iY) = Re(F^{-1}(X)) + i*Re(F^{-1}(Y)).
 * Pack Hermitian frequency pairs into one complex IFFT; spatial Re/Im are the two real outputs.
 */
void AB_FFT_IFFT2D_HermitianPair( abComplex_t *freqA, abComplex_t *freqB, float *outA, float *outB, int n ) {
	abComplex_t combined[AB_MAX_GRID_N * AB_MAX_GRID_N];
	abComplex_t spatial[AB_MAX_GRID_N * AB_MAX_GRID_N];
	int i;

	if ( n < 2 || n > AB_MAX_GRID_N || !freqA || !freqB || !outA || !outB ) {
		return;
	}

	for ( i = 0; i < n * n; i++ ) {
		combined[i].re = freqA[i].re - freqB[i].im;
		combined[i].im = freqA[i].im + freqB[i].re;
	}

	memcpy( spatial, combined, (size_t)( n * n ) * sizeof( combined[0] ) );
	AB_FFT_IFFT2D( spatial, n );

	for ( i = 0; i < n * n; i++ ) {
		outA[i] = spatial[i].re;
		outB[i] = spatial[i].im;
	}
	AB_FFT_ApplyCheckerboard( outA, n );
	AB_FFT_ApplyCheckerboard( outB, n );
}

void AB_FFT_ApplyCheckerboard( float *realGrid, int n ) {
	int x, y;
	for ( y = 0; y < n; y++ ) {
		for ( x = 0; x < n; x++ ) {
			const float sign = ( ( x + y ) & 1 ) ? -1.0f : 1.0f;
			realGrid[y * n + x] *= sign;
		}
	}
}
