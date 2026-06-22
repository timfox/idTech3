/*
 * Depth velocity field with logarithmic slices + exponential interpolation (Arc Blanc §3.4–3.6).
 */
#include "arc_blanc_internal.h"
#include "../../qcommon/qcommon.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float ab_attenuation( float kmag, float y )
{
	if ( y > 0.0f ) {
		return 1.0f + kmag * y;
	}
	return expf( kmag * y );
}

static void ab_build_depth_samples( float ymin, float ymax, float *out, int count )
{
	const float alpha = 0.0001f;
	float beta;
	int i;

	if ( count < 1 ) {
		return;
	}
	beta = -ymin / ( 2.0f * logf( alpha * ymin * ymin + 1.0f ) );

	for ( i = 0; i < count; i++ ) {
		const float t = ( count == 1 ) ? 0.0f : (float)i / (float)( count - 1 );
		float y = ymin + t * ( ymax - ymin );
		float ld = ( y > 0.0f ) ? beta * logf( alpha * y * y + 1.0f )
			: -beta * logf( alpha * y * y + 1.0f );
		out[i] = ld;
	}
}

void AB_Ocean_FillDepthSamples( float *out, int count )
{
	ab_build_depth_samples( -125.0f, 4.5f, out, count );
}

static void ab_ifft_velocity_slice( const abSpectrumState_t *spec, int n, float tileLength, float t,
	float depthY, float *outVx, float *outVy, float *outVz )
{
	abComplex_t freqX[AB_MAX_GRID_N * AB_MAX_GRID_N];
	abComplex_t freqY[AB_MAX_GRID_N * AB_MAX_GRID_N];
	abComplex_t freqZ[AB_MAX_GRID_N * AB_MAX_GRID_N];
	const float invL = 1.0f / tileLength;
	int i;

	for ( i = 0; i < n * n; i++ ) {
		const float w = spec->omega[i];
		const float kmag = spec->kMag[i];
		const float att = ab_attenuation( kmag, depthY );
		const float invW = ( w > 1e-8f ) ? ( 1.0f / w ) : 0.0f;
		const int ix = i % n;
		const int iz = i / n;
		const int half = n >> 1;
		const float kx = 2.0f * (float)M_PI * (float)( ix - half ) * invL;
		const float kz = 2.0f * (float)M_PI * (float)( iz - half ) * invL;
		abComplex_t drive;
		float scaleX, scaleZ;

		AB_Spectrum_TimeVelocityDrive( spec, n, tileLength, t, i, &drive );

		scaleX = att * ( -kx * AB_GRAVITY * invW );
		scaleZ = att * ( -kz * AB_GRAVITY * invW );
		freqX[i].re = scaleX * drive.re;
		freqX[i].im = scaleX * drive.im;
		freqZ[i].re = scaleZ * drive.re;
		freqZ[i].im = scaleZ * drive.im;
		/* v_y = E * i*omega * drive */
		freqY[i].re = att * w * ( -drive.im );
		freqY[i].im = att * w * drive.re;
	}

	AB_FFT_IFFT2D( freqX, n );
	AB_FFT_IFFT2D( freqY, n );
	AB_FFT_IFFT2D( freqZ, n );

	for ( i = 0; i < n * n; i++ ) {
		outVx[i] = freqX[i].re;
		outVy[i] = freqY[i].re;
		outVz[i] = freqZ[i].re;
	}
	AB_FFT_ApplyCheckerboard( outVx, n );
	AB_FFT_ApplyCheckerboard( outVy, n );
	AB_FFT_ApplyCheckerboard( outVz, n );
}

void AB_Ocean_UpdateVelocitySlices( abOceanState_t *ocean )
{
	static float scratchVx[AB_MAX_GRID_N * AB_MAX_GRID_N];
	static float scratchVy[AB_MAX_GRID_N * AB_MAX_GRID_N];
	static float scratchVz[AB_MAX_GRID_N * AB_MAX_GRID_N];
	int s, c, n2, n;

	if ( !ocean || !ocean->fields[0].spec.valid ) {
		return;
	}

	n = ocean->gridN;
	n2 = n * n;
	AB_Ocean_FillDepthSamples( ocean->depthSamples, AB_VELOCITY_SAMPLES );

	for ( s = 0; s < AB_VELOCITY_SAMPLES; s++ ) {
		float *slice = ocean->velocitySlices[s];
		int i;

		Com_Memset( slice, 0, (size_t)( n2 * 3 ) * sizeof( float ) );

		for ( c = 0; c < AB_CASCADE_COUNT; c++ ) {
			ab_ifft_velocity_slice( &ocean->fields[c].spec, n, ocean->cascades[c].length,
				ocean->time, ocean->depthSamples[s], scratchVx, scratchVy, scratchVz );
			for ( i = 0; i < n2; i++ ) {
				slice[i * 3 + 0] += scratchVx[i];
				slice[i * 3 + 1] += scratchVy[i];
				slice[i * 3 + 2] += scratchVz[i];
			}
		}
	}
}

static float ab_lerp_exp_mag( float ya, float yb, float va, float vb, float y )
{
	float alpha, beta;
	if ( fabsf( yb - ya ) < 1e-6f ) {
		return va;
	}
	beta = ( logf( fabsf( vb ) + 1e-6f ) - logf( fabsf( va ) + 1e-6f ) ) / ( yb - ya );
	alpha = va / expf( beta * ya );
	return alpha * expf( beta * y );
}

static float ab_bilinear3( const float *slice, int n, float u, float v, int comp )
{
	int x0, z0, x1, z1;
	float fx, fz;
	float h00, h10, h01, h11;

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

	h00 = slice[( z0 * n + x0 ) * 3 + comp];
	h10 = slice[( z0 * n + x1 ) * 3 + comp];
	h01 = slice[( z1 * n + x0 ) * 3 + comp];
	h11 = slice[( z1 * n + x1 ) * 3 + comp];
	return ( 1.0f - fx ) * ( 1.0f - fz ) * h00 + fx * ( 1.0f - fz ) * h10
		+ ( 1.0f - fx ) * fz * h01 + fx * fz * h11;
}

void AB_Ocean_SampleVelocityWorld( const abOceanState_t *ocean, float worldX, float worldY, float worldZ, vec3_t outVel )
{
	float u, v, wx, wz;
	int s0, s1;
	float y0, y1, t;

	if ( !ocean || AB_VELOCITY_SAMPLES < 2 ) {
		VectorClear( outVel );
		return;
	}

	wx = fmodf( worldX, ocean->tileSize );
	wz = fmodf( worldZ, ocean->tileSize );
	if ( wx < 0.0f ) {
		wx += ocean->tileSize;
	}
	if ( wz < 0.0f ) {
		wz += ocean->tileSize;
	}
	u = wx / ocean->tileSize;
	v = wz / ocean->tileSize;

	s0 = 0;
	s1 = AB_VELOCITY_SAMPLES - 1;
	for ( s0 = 0; s0 < AB_VELOCITY_SAMPLES - 1; s0++ ) {
		if ( worldY <= ocean->depthSamples[s0 + 1] ) {
			s1 = s0 + 1;
			break;
		}
	}
	y0 = ocean->depthSamples[s0];
	y1 = ocean->depthSamples[s1];
	t = ( fabsf( y1 - y0 ) > 1e-6f ) ? ( worldY - y0 ) / ( y1 - y0 ) : 0.0f;
	if ( t < 0.0f ) {
		t = 0.0f;
	}
	if ( t > 1.0f ) {
		t = 1.0f;
	}

	{
		const float *sl0 = ocean->velocitySlices[s0];
		const float *sl1 = ocean->velocitySlices[s1];
		int n = ocean->gridN;
		float vx0 = ab_bilinear3( sl0, n, u, v, 0 );
		float vy0 = ab_bilinear3( sl0, n, u, v, 1 );
		float vz0 = ab_bilinear3( sl0, n, u, v, 2 );
		float vx1 = ab_bilinear3( sl1, n, u, v, 0 );
		float vy1 = ab_bilinear3( sl1, n, u, v, 1 );
		float vz1 = ab_bilinear3( sl1, n, u, v, 2 );
		float m0 = sqrtf( vx0 * vx0 + vy0 * vy0 + vz0 * vz0 );
		float m1 = sqrtf( vx1 * vx1 + vy1 * vy1 + vz1 * vz1 );
		float mag = ab_lerp_exp_mag( y0, y1, m0, m1, worldY );
		float ang0 = atan2f( vz0, vx0 );
		float ang1 = atan2f( vz1, vx1 );
		float ang = ang0 + t * ( ang1 - ang0 );

		outVel[0] = mag * cosf( ang );
		outVel[1] = vy0 + t * ( vy1 - vy0 );
		outVel[2] = mag * sinf( ang );
	}
}
