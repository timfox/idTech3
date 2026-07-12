/*
 * Tessendorf FFT ocean surface + cascade combine (Arc Blanc).
 */
#include "arc_blanc_internal.h"
#include "qcommon.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void ab_spectrum_time_terms( const abSpectrumState_t *spec, int i, int n, float tileLength, float t,
	abComplex_t *outHt, abComplex_t *outDrive )
{
	const float w = spec->omega[i];
	const float cw = cosf( w * t );
	const float sw = sinf( w * t );
	const float h0re = spec->h0[i].re;
	const float h0im = spec->h0[i].im;
	const float hcRe = spec->h0conj[i].re;
	const float hcIm = spec->h0conj[i].im;
	abComplex_t a, b;

	a.re = h0re * cw - h0im * sw;
	a.im = h0re * sw + h0im * cw;
	b.re = hcRe * cw + hcIm * sw;
	b.im = -hcRe * sw + hcIm * cw;

	if ( outHt ) {
		outHt->re = a.re + b.re;
		outHt->im = a.im + b.im;
	}
	if ( outDrive ) {
		outDrive->re = a.re - b.re;
		outDrive->im = a.im - b.im;
	}
	(void)n;
	(void)tileLength;
}

void AB_Spectrum_TimeHt( const abSpectrumState_t *spec, int n, float tileLength, float t,
	abComplex_t *outH, abComplex_t *outDx, abComplex_t *outDz )
{
	int i;
	const float invL = 1.0f / tileLength;

	for ( i = 0; i < n * n; i++ ) {
		abComplex_t ht;
		float kmag = spec->kMag[i];
		float invK = ( kmag > 1e-8f ) ? ( 1.0f / kmag ) : 0.0f;
		int ix = i % n;
		int iz = i / n;
		int half = n >> 1;
		const float kx = 2.0f * (float)M_PI * (float)( ix - half ) * invL;
		const float kz = 2.0f * (float)M_PI * (float)( iz - half ) * invL;

		ab_spectrum_time_terms( spec, i, n, tileLength, t, &ht, NULL );
		if ( outH ) {
			outH[i] = ht;
		}

		if ( outDx && outDz ) {
			if ( kmag > 1e-8f ) {
				outDx[i].re = -kx * invK * ht.im;
				outDx[i].im = kx * invK * ht.re;
				outDz[i].re = -kz * invK * ht.im;
				outDz[i].im = kz * invK * ht.re;
			} else {
				outDx[i].re = outDx[i].im = 0.0f;
				outDz[i].re = outDz[i].im = 0.0f;
			}
		}
	}
}

void AB_Spectrum_TimeVelocityDrive( const abSpectrumState_t *spec, int n, float tileLength, float t,
	int idx, abComplex_t *outDrive )
{
	ab_spectrum_time_terms( spec, idx, n, tileLength, t, NULL, outDrive );
}

static void ab_ifft_real( abComplex_t *freq, float *spatial, int n )
{
	AB_FFT_IFFT2D( freq, n );
	{
		int i;
		for ( i = 0; i < n * n; i++ ) {
			spatial[i] = freq[i].re;
		}
	}
	AB_FFT_ApplyCheckerboard( spatial, n );
}

static void ab_update_cascade_field( abCascadeField_t *field, int n, float tileLength, float t )
{
	abComplex_t bufH[AB_MAX_GRID_N * AB_MAX_GRID_N];
	abComplex_t bufDx[AB_MAX_GRID_N * AB_MAX_GRID_N];
	abComplex_t bufDz[AB_MAX_GRID_N * AB_MAX_GRID_N];
	abComplex_t gradX[AB_MAX_GRID_N * AB_MAX_GRID_N];
	abComplex_t gradZ[AB_MAX_GRID_N * AB_MAX_GRID_N];
	int i;

	if ( !field->spec.valid ) {
		return;
	}

	AB_Spectrum_TimeHt( &field->spec, n, tileLength, t, bufH, bufDx, bufDz );

	for ( i = 0; i < n * n; i++ ) {
		int ix = i % n;
		int iz = i / n;
		int half = n >> 1;
		const float invL = 1.0f / tileLength;
		float kx = 2.0f * (float)M_PI * (float)( ix - half ) * invL;
		float kz = 2.0f * (float)M_PI * (float)( iz - half ) * invL;
		abComplex_t h = bufH[i];
		gradX[i].re = kx * h.im;
		gradX[i].im = -kx * h.re;
		gradZ[i].re = kz * h.im;
		gradZ[i].im = -kz * h.re;
	}

	/* Hermitian pair IFFTs (Theorem 1): height+dispX, dispZ+gradHx, gradHz alone */
	AB_FFT_IFFT2D_HermitianPair( bufH, bufDx, field->height, field->dispX, n );
	AB_FFT_IFFT2D_HermitianPair( bufDz, gradX, field->dispZ, field->gradHx, n );
	ab_ifft_real( gradZ, field->gradHz, n );
}

void AB_Ocean_InitDefaults( abOceanState_t *ocean, int gridN, float tileSize )
{
	int c;
	static const abCascadeParams_t defaults[AB_CASCADE_COUNT] = {
		{ 256.0f, 0.0f, 12.0f * (float)M_PI / 16.0f },
		{ 16.0f, 12.0f * (float)M_PI / 16.0f, 12.0f * (float)M_PI / 4.0f },
		{ 4.0f, 12.0f * (float)M_PI / 4.0f, 1.0e6f }
	};

	if ( !ocean ) {
		return;
	}
	Com_Memset( ocean, 0, sizeof( *ocean ) );
	ocean->gridN = gridN > 0 ? gridN : AB_DEFAULT_GRID_N;
	ocean->tileSize = tileSize > 0.0f ? tileSize : 256.0f;
	ocean->windSpeed = 20.0f;
	ocean->fetch = 1000.0f;
	ocean->windDirRad = 0.0f;
	ocean->swell = 0.5f;
	ocean->directional = 1.0f;
	ocean->amplitudeScale = 1.0f;
	ocean->heightScale = 1.0f;
	ocean->chopScale = 1.0f;
	ocean->waveSpeed = 1.0f;
	ocean->spread = 0.0f;
	ocean->gustStrength = 0.0f;
	ocean->gustSpeed = 0.5f;
	for ( c = 0; c < AB_CASCADE_COUNT; c++ ) {
		ocean->cascades[c] = defaults[c];
	}
	ocean->spectrumDirty = qtrue;
}

void AB_Ocean_UpdateSpectrum( abOceanState_t *ocean )
{
	int c;
	if ( !ocean ) {
		return;
	}
	AB_Spectrum_Seed( 0xA9C1u );
	for ( c = 0; c < AB_CASCADE_COUNT; c++ ) {
		AB_Spectrum_GenerateH0( &ocean->fields[c].spec, ocean->gridN,
			ocean->cascades[c].length, ocean->windSpeed, ocean->fetch,
			ocean->windDirRad, ocean->swell, ocean->directional, ocean->spread,
			ocean->cascades[c].kMin, ocean->cascades[c].kMax );
	}
	ocean->spectrumDirty = qfalse;
}

void AB_Ocean_UpdateTime( abOceanState_t *ocean, float dt )
{
	int c;
	if ( !ocean ) {
		return;
	}
	if ( ocean->spectrumDirty ) {
		AB_Ocean_UpdateSpectrum( ocean );
	}
	ocean->time += dt * ocean->waveSpeed;
	for ( c = 0; c < AB_CASCADE_COUNT; c++ ) {
		ab_update_cascade_field( &ocean->fields[c], ocean->gridN, ocean->cascades[c].length, ocean->time );
	}
	AB_Ocean_CombineCascades( ocean );
	AB_Ocean_UpdateVelocitySlices( ocean );
}

void AB_Ocean_CombineCascades( abOceanState_t *ocean )
{
	int i, n2;
	float gust = 1.0f;
	float heightScale;
	float chopScale;
	if ( !ocean ) {
		return;
	}
	if ( ocean->gustStrength > 0.0f ) {
		gust += ocean->gustStrength *
			( 0.6f * sinf( ocean->time * ocean->gustSpeed ) +
			  0.4f * sinf( ocean->time * ocean->gustSpeed * 2.37f + 0.7f ) );
		if ( gust < 0.2f ) {
			gust = 0.2f;
		}
	}
	heightScale = ocean->amplitudeScale * ocean->heightScale * gust;
	chopScale = ocean->amplitudeScale * ocean->chopScale * gust;
	n2 = ocean->gridN * ocean->gridN;
	for ( i = 0; i < n2; i++ ) {
		ocean->combinedHeight[i] =
			( ocean->fields[0].height[i] + ocean->fields[1].height[i] + ocean->fields[2].height[i] ) * heightScale;
		ocean->combinedDispX[i] =
			( ocean->fields[0].dispX[i] + ocean->fields[1].dispX[i] + ocean->fields[2].dispX[i] ) * chopScale;
		ocean->combinedDispZ[i] =
			( ocean->fields[0].dispZ[i] + ocean->fields[1].dispZ[i] + ocean->fields[2].dispZ[i] ) * chopScale;
	}
}

static float ab_bilinear( const float *grid, int n, float u, float v )
{
	int x0, z0, x1, z1;
	float fx, fz, h00, h10, h01, h11;

	u = fmodf( u, 1.0f );
	v = fmodf( v, 1.0f );
	if ( u < 0.0f ) {
		u += 1.0f;
	}
	if ( v < 0.0f ) {
		v += 1.0f;
	}

	x0 = (int)( u * ( n - 1 ) );
	z0 = (int)( v * ( n - 1 ) );
	x1 = ( x0 + 1 < n ) ? x0 + 1 : 0;
	z1 = ( z0 + 1 < n ) ? z0 + 1 : 0;
	fx = u * ( n - 1 ) - (float)x0;
	fz = v * ( n - 1 ) - (float)z0;

	h00 = grid[z0 * n + x0];
	h10 = grid[z0 * n + x1];
	h01 = grid[z1 * n + x0];
	h11 = grid[z1 * n + x1];
	return ( 1.0f - fx ) * ( 1.0f - fz ) * h00 + fx * ( 1.0f - fz ) * h10
		+ ( 1.0f - fx ) * fz * h01 + fx * fz * h11;
}

float AB_Ocean_SampleHeightTile( const abOceanState_t *ocean, float localX, float localZ )
{
	float u, v;
	if ( !ocean || ocean->gridN < 2 ) {
		return 0.0f;
	}
	u = localX / ocean->tileSize;
	v = localZ / ocean->tileSize;
	return ab_bilinear( ocean->combinedHeight, ocean->gridN, u, v );
}

float AB_Ocean_SampleHeightWorld( const abOceanState_t *ocean, float worldX, float worldZ )
{
	float wx, wz;
	int iter;

	if ( !ocean ) {
		return 0.0f;
	}

	wx = fmodf( worldX, ocean->tileSize );
	wz = fmodf( worldZ, ocean->tileSize );
	if ( wx < 0.0f ) {
		wx += ocean->tileSize;
	}
	if ( wz < 0.0f ) {
		wz += ocean->tileSize;
	}

	for ( iter = 0; iter < AB_HEIGHT_ITER; iter++ ) {
		float u = wx / ocean->tileSize;
		float v = wz / ocean->tileSize;
		float dx = ab_bilinear( ocean->combinedDispX, ocean->gridN, u, v );
		float dz = ab_bilinear( ocean->combinedDispZ, ocean->gridN, u, v );
		wx = fmodf( worldX, ocean->tileSize ) - dx;
		wz = fmodf( worldZ, ocean->tileSize ) - dz;
		if ( wx < 0.0f ) {
			wx += ocean->tileSize;
		}
		if ( wz < 0.0f ) {
			wz += ocean->tileSize;
		}
	}

	return ab_bilinear( ocean->combinedHeight, ocean->gridN, wx / ocean->tileSize, wz / ocean->tileSize );
}

float AB_Ocean_MaxHeightGridErrorHermitian( const abOceanState_t *ocean, int cascadeIndex )
{
	abComplex_t bufH[AB_MAX_GRID_N * AB_MAX_GRID_N];
	abComplex_t bufDx[AB_MAX_GRID_N * AB_MAX_GRID_N];
	abComplex_t bufDz[AB_MAX_GRID_N * AB_MAX_GRID_N];
	float sepHeight[AB_MAX_GRID_N * AB_MAX_GRID_N];
	float maxErr = 0.0f;
	int i, n;

	if ( !ocean || cascadeIndex < 0 || cascadeIndex >= AB_CASCADE_COUNT ) {
		return 0.0f;
	}

	n = ocean->gridN;
	if ( n < 2 || !ocean->fields[cascadeIndex].spec.valid ) {
		return 0.0f;
	}

	AB_Spectrum_TimeHt( &ocean->fields[cascadeIndex].spec, n,
		ocean->cascades[cascadeIndex].length, ocean->time, bufH, bufDx, bufDz );
	{
		abComplex_t tmpH[AB_MAX_GRID_N * AB_MAX_GRID_N];
		memcpy( tmpH, bufH, (size_t)( n * n ) * sizeof( bufH[0] ) );
		ab_ifft_real( tmpH, sepHeight, n );
	}

	for ( i = 0; i < n * n; i++ ) {
		const float e = fabsf( ocean->fields[cascadeIndex].height[i] - sepHeight[i] );
		if ( e > maxErr ) {
			maxErr = e;
		}
	}
	return maxErr;
}
