/*
 * Unit tests: Arc Blanc ocean framework (Algis et al. 2025).
 */
#include <stdio.h>
#include <math.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"
#include "world/arc_blanc/arc_blanc.h"
#include "world/arc_blanc/arc_blanc_internal.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static int test_jonswap_peak( void )
{
	float sLow, sPeak, sHigh;

	sLow = AB_Spectrum_JONSWAP( 0.5f, 20.0f, 1000.0f );
	sPeak = AB_Spectrum_JONSWAP( 2.0f, 20.0f, 1000.0f );
	sHigh = AB_Spectrum_JONSWAP( 8.0f, 20.0f, 1000.0f );
	ASSERT( sPeak > sLow, "JONSWAP peak exceeds low frequency" );
	ASSERT( sPeak > 0.0f && sHigh >= 0.0f, "JONSWAP samples non-negative" );
	return 0;
}

static int test_fft_roundtrip( void )
{
	abComplex_t data[16];
	int i;
	for ( i = 0; i < 16; i++ ) {
		data[i].re = (float)( i % 4 );
		data[i].im = 0.0f;
	}
	AB_FFT_Complex1D( data, 16, qfalse );
	AB_FFT_Complex1D( data, 16, qtrue );
	ASSERT( fabsf( data[0].re - 0.0f ) < 0.01f, "FFT round-trip bin 0" );
	return 0;
}

static int test_hermitian_height_parity( void )
{
	const abOceanState_t *ocean;
	float err;

	ArcBlanc_ResetForTest();
	ocean = ArcBlanc_GetOceanForTest();
	err = AB_Ocean_MaxHeightGridErrorHermitian( ocean, 0 );
	ASSERT( err < 0.05f, "Hermitian height matches separate IFFT" );
	return 0;
}

static int test_ocean_height_deterministic( void )
{
	const abOceanState_t *ocean;
	float h0, h1;

	ArcBlanc_ResetForTest();
	ocean = ArcBlanc_GetOceanForTest();
	h0 = AB_Ocean_SampleHeightWorld( ocean, 17.0f, 23.0f );
	ArcBlanc_ResetForTest();
	ocean = ArcBlanc_GetOceanForTest();
	h1 = AB_Ocean_SampleHeightWorld( ocean, 17.0f, 23.0f );
	ASSERT( fabsf( h0 - h1 ) < 1e-4f, "deterministic ocean height sample" );
	return 0;
}

static int test_ocean_height_finite( void )
{
	const abOceanState_t *ocean;
	float h;

	ArcBlanc_ResetForTest();
	ocean = ArcBlanc_GetOceanForTest();
	ASSERT( ocean != NULL, "ocean state" );
	h = AB_Ocean_SampleHeightWorld( ocean, 32.0f, 48.0f );
	ASSERT( fabsf( h ) < 100.0f, "sampled height bounded" );
	return 0;
}

static int test_velocity_slices( void )
{
	const abOceanState_t *ocean;
	vec3_t vel;

	ArcBlanc_ResetForTest();
	ocean = ArcBlanc_GetOceanForTest();
	AB_Ocean_SampleVelocityWorld( ocean, 10.0f, -5.0f, 10.0f, vel );
	ASSERT( fabsf( vel[0] ) < 50.0f && fabsf( vel[1] ) < 50.0f, "velocity bounded" );
	ASSERT( fabsf( vel[2] ) < 50.0f, "horizontal vz velocity bounded" );
	return 0;
}

static int test_hermitian_pair_ifft( void )
{
	abComplex_t a[16];
	abComplex_t b[16];
	abComplex_t soloA[16];
	abComplex_t soloB[16];
	float outA[16], outB[16];
	float refA[16], refB[16];
	int i;

	for ( i = 0; i < 16; i++ ) {
		a[i].re = (float)( i % 3 );
		a[i].im = 0.0f;
		b[i].re = (float)( i % 2 );
		b[i].im = 0.0f;
	}

	memcpy( soloA, a, sizeof( soloA ) );
	memcpy( soloB, b, sizeof( soloB ) );
	AB_FFT_IFFT2D( soloA, 4 );
	AB_FFT_IFFT2D( soloB, 4 );
	for ( i = 0; i < 16; i++ ) {
		refA[i] = soloA[i].re;
		refB[i] = soloB[i].re;
	}
	AB_FFT_ApplyCheckerboard( refA, 4 );
	AB_FFT_ApplyCheckerboard( refB, 4 );

	AB_FFT_IFFT2D_HermitianPair( a, b, outA, outB, 4 );
	ASSERT( fabsf( outA[0] - refA[0] ) < 0.05f, "Hermitian pair outA[0]" );
	ASSERT( fabsf( outB[0] - refB[0] ) < 0.05f, "Hermitian pair outB[0]" );
	return 0;
}

static int test_spectrum_negk_symmetry( void )
{
	abSpectrumState_t spec;
	int n = 16;
	int ix, iz;

	AB_Spectrum_Seed( 0xABCDu );
	AB_Spectrum_GenerateH0( &spec, n, 256.0f, 20.0f, 1000.0f, 0.0f, 0.5f, 1.0f, 0.0f, 1.0e6f );

	for ( iz = 0; iz < n; iz++ ) {
		for ( ix = 0; ix < n; ix++ ) {
			const int idx = iz * n + ix;
			const int negIdx = AB_Spectrum_NegKIndex( n, ix, iz );
			ASSERT( fabsf( spec.h0conj[idx].re - spec.h0[negIdx].re ) < 1e-5f, "h0conj re symmetry" );
			ASSERT( fabsf( spec.h0conj[idx].im + spec.h0[negIdx].im ) < 1e-5f, "h0conj im symmetry" );
		}
	}
	return 0;
}

static int test_ittc_water_density( void )
{
	float rhoSurface = AB_Spectrum_WaterDensity( 0.0f );
	float rhoDeep = AB_Spectrum_WaterDensity( -500.0f );
	ASSERT( rhoSurface >= 1024.0f && rhoSurface <= 1026.0f, "surface density ITTC band" );
	ASSERT( rhoDeep > rhoSurface, "density increases with depth" );
	return 0;
}

static int test_velocity_drive_sign( void )
{
	abSpectrumState_t spec;
	abComplex_t ht, drive;

	Com_Memset( &spec, 0, sizeof( spec ) );
	spec.valid = qtrue;
	spec.h0[0].re = 1.0f;
	spec.h0[0].im = 0.5f;
	spec.h0conj[0].re = 1.0f;
	spec.h0conj[0].im = -0.5f;
	spec.omega[0] = 2.0f;

	AB_Spectrum_TimeHt( &spec, 1, 256.0f, 0.25f, &ht, NULL, NULL );
	AB_Spectrum_TimeVelocityDrive( &spec, 1, 256.0f, 0.25f, 0, &drive );
	ASSERT( fabsf( drive.re - ht.re ) > 1e-4f || fabsf( drive.im - ht.im ) > 1e-4f,
		"velocity drive differs from height spectrum sum" );
	return 0;
}

int main( void )
{
	if ( test_jonswap_peak() ) return 1;
	if ( test_fft_roundtrip() ) return 1;
	if ( test_hermitian_pair_ifft() ) return 1;
	if ( test_spectrum_negk_symmetry() ) return 1;
	if ( test_ittc_water_density() ) return 1;
	if ( test_velocity_drive_sign() ) return 1;
	if ( test_hermitian_height_parity() ) return 1;
	if ( test_ocean_height_deterministic() ) return 1;
	if ( test_ocean_height_finite() ) return 1;
	if ( test_velocity_slices() ) return 1;
	printf( "unit_arc_blanc: all tests passed\n" );
	return 0;
}
