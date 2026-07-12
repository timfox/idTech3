/*
 * Arc Blanc ocean spectra — JONSWAP + Donelan-Banner / swell (Algis et al. 2025).
 */
#include "arc_blanc_internal.h"
#include "qcommon.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static unsigned int s_rng = 0xC0FFEEu;

static float ab_rand01( void )
{
	s_rng ^= s_rng << 13;
	s_rng ^= s_rng >> 17;
	s_rng ^= s_rng << 5;
	return (float)( s_rng & 0xFFFFFFu ) / (float)0x1000000u;
}

static float ab_randGaussian( void )
{
	float u1 = ab_rand01();
	float u2 = ab_rand01();
	if ( u1 < 1e-8f ) {
		u1 = 1e-8f;
	}
	return sqrtf( -2.0f * logf( u1 ) ) * cosf( 2.0f * (float)M_PI * u2 );
}

void AB_Spectrum_Seed( unsigned int seed )
{
	s_rng = seed ? seed : 1u;
}

int AB_Spectrum_NegKIndex( int n, int ix, int iz )
{
	int negIx;
	int negIz;

	if ( n < 2 ) {
		return 0;
	}
	negIx = ( n - ix ) % n;
	negIz = ( n - iz ) % n;
	return negIz * n + negIx;
}

/*
 * ITTC seawater density (kg/m³) — linear blend surface → 1000 m (7.5-02-01-03 style).
 */
float AB_Spectrum_WaterDensity( float depthY )
{
	const float rhoSurface = 1025.0f;
	const float rhoDeep = 1028.0f;
	const float deepM = 1000.0f;
	float depth = ( depthY < 0.0f ) ? -depthY : 0.0f;
	float t = depth / deepM;

	if ( t > 1.0f ) {
		t = 1.0f;
	}
	return rhoSurface + t * ( rhoDeep - rhoSurface );
}

float AB_Spectrum_JONSWAP( float omega, float windSpeed, float fetch )
{
	float alpha, omegaP, gamma, sigma, r, ratio;

	if ( omega < 1e-6f || windSpeed < 0.1f || fetch < 1.0f ) {
		return 0.0f;
	}

	alpha = 0.076f * powf( ( windSpeed * windSpeed ) / ( AB_GRAVITY * fetch ), 0.22f );
	omegaP = 22.0f * powf( ( AB_GRAVITY * AB_GRAVITY ) / ( windSpeed * fetch ), 1.0f / 3.0f );
	gamma = 3.3f;
	sigma = ( omega <= omegaP ) ? 0.07f : 0.09f;
	r = expf( -( ( omega - omegaP ) * ( omega - omegaP ) ) / ( 2.0f * sigma * sigma * omegaP * omegaP ) );
	ratio = omegaP / omega;
	return alpha * AB_GRAVITY * AB_GRAVITY / powf( omega, 5.0f )
		* expf( -1.25f * ratio * ratio * ratio * ratio ) * powf( gamma, r );
}

static float ab_qdb( float rOmega )
{
	if ( rOmega < 0.94f ) {
		return 7.1467551f * rOmega * rOmega - 13.4662001f * rOmega + 7.75651088f;
	}
	if ( rOmega < 5.0f ) {
		return -0.69906109f * rOmega * rOmega + 0.77975933f * rOmega + 0.10169164f;
	}
	if ( rOmega < 100.0f ) {
		return -2.1860997f * rOmega * rOmega + 0.0269209f * rOmega + 0.00016283f;
	}
	return 1.2038847f * rOmega + 0.0008147f;
}

static float ab_beta_s( float rOmega )
{
	float eps;
	if ( rOmega < 0.56f ) {
		return 2.61f;
	}
	if ( rOmega < 0.95f ) {
		return 2.61f * powf( rOmega, 1.3f );
	}
	if ( rOmega < 1.6f ) {
		return 2.28f * powf( rOmega, -1.3f );
	}
	eps = 0.8393f * expf( -0.567f * logf( rOmega * rOmega ) ) - 0.4f;
	return 10.0f * eps;
}

static float ab_ddb( float omega, float theta, float omegaP )
{
	float rOmega = omega / omegaP;
	float beta = ab_beta_s( rOmega );
	float q = ab_qdb( rOmega );
	float sech = 1.0f / coshf( beta * theta );
	return 0.5f * q * beta * sech * sech;
}

static float ab_d_swell( float theta, float rOmega, float swell )
{
	float sXi = 16.0f * tanhf( 1.0f / rOmega ) * swell * swell;
	float c = cosf( theta * 0.5f );
	return powf( fabsf( c ), 2.0f * sXi );
}

static float ab_qdb_xi( float rOmega, float swell )
{
	/* coarse normalization — paper uses integral; piecewise QDBξ approximated */
	float qdb = ab_qdb( rOmega );
	float swellNorm = 1.0f + swell * 0.35f;
	return qdb / swellNorm;
}

float AB_Spectrum_Directional( float omega, float theta, float windDir, float swell, float directional,
	float spread )
{
	float omegaP, rOmega, dNeutral, dDir, qXi;
	(void)windDir;

	if ( omega < 1e-6f ) {
		return 1.0f / ( 2.0f * (float)M_PI );
	}

	omegaP = 22.0f * powf( ( AB_GRAVITY * AB_GRAVITY ) / ( 20.0f * 1000.0f ), 1.0f / 3.0f );
	rOmega = omega / omegaP;
	dNeutral = 1.0f / ( 2.0f * (float)M_PI );
	dDir = ab_ddb( omega, theta, omegaP ) * ab_d_swell( theta, rOmega, swell );
	qXi = ab_qdb_xi( rOmega, swell );
	dDir *= qXi;
	if ( spread > 0.0f ) {
		const float c = fabsf( cosf( theta * 0.5f ) );
		dDir *= powf( c, spread * 24.0f );
	}
	return ( 1.0f - directional ) * dNeutral + directional * dDir;
}

void AB_Spectrum_GenerateH0( abSpectrumState_t *spec, int n, float tileLength,
	float windSpeed, float fetch, float windDirRad, float swell, float directional,
	float spread, float kMin, float kMax )
{
	int ix, iz, half;
	float invL = 1.0f / tileLength;

	if ( !spec || n < 2 || n > AB_MAX_GRID_N ) {
		return;
	}

	half = n >> 1;
	Com_Memset( spec, 0, sizeof( *spec ) );

	for ( iz = 0; iz < n; iz++ ) {
		for ( ix = 0; ix < n; ix++ ) {
			const int idx = iz * n + ix;
			const int m = ix - half;
			const int nn = iz - half;
			const float kx = 2.0f * (float)M_PI * (float)m * invL;
			const float kz = 2.0f * (float)M_PI * (float)nn * invL;
			const float kmag = sqrtf( kx * kx + kz * kz );
			abComplex_t h0;

			spec->kMag[idx] = kmag;
			if ( kmag < 1e-8f ) {
				spec->h0[idx].re = 0.0f;
				spec->h0[idx].im = 0.0f;
				spec->h0conj[idx] = spec->h0[idx];
				spec->omega[idx] = 0.0f;
				continue;
			}

			if ( kmag < kMin || kmag >= kMax ) {
				spec->h0[idx].re = 0.0f;
				spec->h0[idx].im = 0.0f;
				spec->h0conj[idx] = spec->h0[idx];
				spec->omega[idx] = sqrtf( AB_GRAVITY * kmag );
				continue;
			}

			{
				const float omega = sqrtf( AB_GRAVITY * kmag );
				const float theta = atan2f( kz, kx ) - windDirRad;
				const float sOmega = AB_Spectrum_JONSWAP( omega, windSpeed, fetch );
				const float dOmega = AB_Spectrum_Directional( omega, theta, windDirRad, swell, directional, spread );
				const float dwdk = sqrtf( AB_GRAVITY / ( 4.0f * kmag ) );
				const float amp = sqrtf( fmaxf( 0.0f,
					4.0f * (float)M_PI / ( tileLength * tileLength * kmag ) * sOmega * dOmega * dwdk ) );
				const float g1 = ab_randGaussian();
				const float g2 = ab_randGaussian();

				h0.re = (float)( 1.0 / sqrt( 2.0 ) ) * g1 * amp;
				h0.im = (float)( 1.0 / sqrt( 2.0 ) ) * g2 * amp;
			}

			spec->h0[idx] = h0;
			spec->omega[idx] = sqrtf( AB_GRAVITY * kmag );
		}
	}

	/* h̃*_0(-k) at k: conjugate of h̃_0 at the negated wave vector index (Tessendorf symmetry). */
	for ( iz = 0; iz < n; iz++ ) {
		for ( ix = 0; ix < n; ix++ ) {
			const int idx = iz * n + ix;
			const int negIdx = AB_Spectrum_NegKIndex( n, ix, iz );
			spec->h0conj[idx].re = spec->h0[negIdx].re;
			spec->h0conj[idx].im = -spec->h0[negIdx].im;
		}
	}

	spec->valid = qtrue;
}
