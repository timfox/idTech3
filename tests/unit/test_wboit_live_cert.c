/*
 * CPU reference helpers for Phase 2.6 live cert / lab (no GPU).
 */
#include <stdio.h>
#include <math.h>
#include <string.h>

#define ASSERT(cond, msg) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while (0)

static float revealage_product( const float *alphas, int count )
{
	float r = 1.0f;
	int i;
	for ( i = 0; i < count; i++ ) {
		float a = alphas[i];
		if ( a < 0.f ) a = 0.f;
		else if ( a > 1.f ) a = 1.f;
		r *= ( 1.f - a );
	}
	return r;
}

static void source_over( const float layer[3], float o, const float fog[3], float out[3] )
{
	float t = 1.f - o;
	out[0] = layer[0] * o + fog[0] * t;
	out[1] = layer[1] * o + fog[1] * t;
	out[2] = layer[2] * o + fog[2] * t;
}

static float fresnel_schlick( float cosTheta, float f0 )
{
	float c = cosTheta;
	float m;
	if ( c < 0.f ) c = 0.f;
	else if ( c > 1.f ) c = 1.f;
	m = 1.f - c;
	return f0 + ( 1.f - f0 ) * m * m * m * m * m;
}

static void beer_lambert( const float color[3], float distance, float absorptionDistance, float outT[3] )
{
	int i;
	float inv = distance / absorptionDistance;
	for ( i = 0; i < 3; i++ ) {
		outT[i] = expf( -( color[i] * inv ) );
	}
}

static float bound_offset( float o, float m )
{
	if ( o > m ) return m;
	if ( o < -m ) return -m;
	return o;
}

static float image_rmse_rgb( const float *a, const float *b, int pixels )
{
	double sum = 0.0;
	int i, c;
	for ( i = 0; i < pixels; i++ ) {
		for ( c = 0; c < 3; c++ ) {
			double d = (double)a[i * 4 + c] - (double)b[i * 4 + c];
			sum += d * d;
		}
	}
	return (float)sqrt( sum / (double)( pixels * 3 ) );
}

static int image_diff_passes( float rmse, float maxAbs, float meanRelLum )
{
	return rmse <= 0.035f && maxAbs <= 0.08f && meanRelLum <= 0.08f;
}

/* Mirror vk_special_blend_select / shadow policy enums numerically. */
enum {
	SPECIAL_BLEND_NONE = 0,
	SPECIAL_BLEND_MULTIPLY,
	SPECIAL_BLEND_FILTER,
	SPECIAL_BLEND_DST_COLOR,
	SPECIAL_BLEND_INVERSE,
	SPECIAL_BLEND_MULTISTAGE
};

#define GLS_SRCBLEND_DST_COLOR 0x00000003
#define GLS_DSTBLEND_ZERO 0x00000010
#define GLS_SRCBLEND_ZERO 0x00000001
#define GLS_DSTBLEND_SRC_COLOR 0x00000030

static int special_blend_select( int src, int dst, int multi )
{
	if ( multi ) return SPECIAL_BLEND_MULTISTAGE;
	if ( ( src == GLS_SRCBLEND_DST_COLOR && dst == GLS_DSTBLEND_ZERO ) ||
		( src == GLS_SRCBLEND_ZERO && dst == GLS_DSTBLEND_SRC_COLOR ) )
		return SPECIAL_BLEND_MULTIPLY;
	return SPECIAL_BLEND_NONE;
}

#define TRANSPARENT_SHADOW_RECEIVE (1u << 0)
#define TRANSPARENT_SHADOW_NONE (1u << 3)

int main( void )
{
	float a[] = { 0.25f, 0.5f, 0.1f };
	float expect = ( 1.f - 0.25f ) * ( 1.f - 0.5f ) * ( 1.f - 0.1f );
	float got = revealage_product( a, 3 );
	float layer[3] = { 0.8f, 0.2f, 0.1f };
	float fog[3] = { 0.1f, 0.2f, 0.3f };
	float out[3];
	float opacities[] = { 0.f, 1.f / 255.f, 0.01f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 0.99f, 1.f };
	float T[3];
	float ref[8] = {
		0.20f, 0.30f, 0.40f, 1.0f,
		0.60f, 0.50f, 0.40f, 1.0f
	};
	float good[8] = {
		0.205f, 0.295f, 0.400f, 1.0f,
		0.590f, 0.505f, 0.405f, 1.0f
	};
	float bad[8] = {
		0.40f, 0.30f, 0.40f, 1.0f,
		0.60f, 0.20f, 0.40f, 1.0f
	};
	int i;

	ASSERT( fabsf( got - expect ) < 1e-5f, "revealage product" );

	for ( i = 0; i < (int)( sizeof( opacities ) / sizeof( opacities[0] ) ); i++ ) {
		float o = opacities[i];
		source_over( layer, o, fog, out );
		ASSERT( fabsf( out[0] - ( layer[0] * o + fog[0] * ( 1.f - o ) ) ) < 1e-6f, "source-over R" );
	}
	source_over( layer, 0.f, fog, out );
	ASSERT( fabsf( out[0] - fog[0] ) < 1e-6f, "alpha0 preserves fog" );
	source_over( layer, 1.f, fog, out );
	ASSERT( fabsf( out[0] - layer[0] ) < 1e-6f, "alpha1 is layer" );

	ASSERT( fabsf( fresnel_schlick( 1.f, 0.04f ) - 0.04f ) < 1e-5f, "fresnel at normal" );
	ASSERT( fresnel_schlick( 0.f, 0.04f ) > 0.9f, "fresnel at grazing" );

	float absCol[3] = { 0.2f, 0.2f, 0.2f };
	beer_lambert( absCol, 10.f, 10.f, T );
	ASSERT( T[0] > 0.f && T[0] < 1.f, "beer-lambert transmittance" );

	ASSERT( bound_offset( 100.f, 8.f ) == 8.f, "refraction offset bound+" );
	ASSERT( bound_offset( -100.f, 8.f ) == -8.f, "refraction offset bound-" );

	ASSERT( special_blend_select( GLS_SRCBLEND_DST_COLOR, GLS_DSTBLEND_ZERO, 0 ) == SPECIAL_BLEND_MULTIPLY,
		"multiply route" );
	ASSERT( special_blend_select( 0, 0, 1 ) == SPECIAL_BLEND_MULTISTAGE, "multistage route" );

	ASSERT( ( TRANSPARENT_SHADOW_RECEIVE & TRANSPARENT_SHADOW_NONE ) == 0, "shadow flags distinct" );
	ASSERT( image_diff_passes( image_rmse_rgb( ref, good, 2 ), 0.01f, 0.02f ),
		"GPU image-diff accepts small deterministic error" );
	ASSERT( !image_diff_passes( image_rmse_rgb( ref, bad, 2 ), 0.30f, 0.20f ),
		"GPU image-diff rejects visible error" );

	printf( "unit_wboit_live_cert: OK\n" );
	return 0;
}
