/*
 * CPU reference tests for Phase 2.2 OIT alpha normalization.
 * Mirrors oit_source_normalize.glsl / vk_oit_normalize_source equations.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#define ASSERT(cond, msg) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while (0)

#define OIT_SOURCE_ALPHA_STRAIGHT 0
#define OIT_SOURCE_ALPHA_PREMULTIPLIED 1

typedef struct {
	float unassoc[3];
	float assoc[3];
	float opacity;
	unsigned flags;
} sample_t;

static void normalize_straight( const float rgba[4], sample_t *s )
{
	float a = rgba[3];
	if ( a < 0.f ) a = 0.f;
	if ( a > 1.f ) a = 1.f;
	s->opacity = a;
	s->unassoc[0] = rgba[0]; s->unassoc[1] = rgba[1]; s->unassoc[2] = rgba[2];
	s->assoc[0] = rgba[0] * a; s->assoc[1] = rgba[1] * a; s->assoc[2] = rgba[2] * a;
	s->flags = 0;
}

static void normalize_premul( const float rgba[4], float eps, sample_t *s )
{
	float a = rgba[3];
	if ( a < 0.f ) a = 0.f;
	if ( a > 1.f ) a = 1.f;
	s->opacity = a;
	s->assoc[0] = rgba[0]; s->assoc[1] = rgba[1]; s->assoc[2] = rgba[2];
	s->flags = 0;
	if ( a > eps ) {
		s->unassoc[0] = rgba[0] / a;
		s->unassoc[1] = rgba[1] / a;
		s->unassoc[2] = rgba[2] / a;
		s->assoc[0] = s->unassoc[0] * a;
		s->assoc[1] = s->unassoc[1] * a;
		s->assoc[2] = s->unassoc[2] * a;
	} else {
		s->unassoc[0] = s->unassoc[1] = s->unassoc[2] = 0.f;
		s->flags = 1; /* clamped div */
		s->assoc[0] = s->assoc[1] = s->assoc[2] = 0.f;
	}
}

static void source_over( const float u[3], float o, const float bg[3], float out[3] )
{
	out[0] = u[0] * o + bg[0] * ( 1.f - o );
	out[1] = u[1] * o + bg[1] * ( 1.f - o );
	out[2] = u[2] * o + bg[2] * ( 1.f - o );
}

static float max3abs( const float a[3], const float b[3] )
{
	float m = 0.f, d;
	d = fabsf( a[0] - b[0] ); if ( d > m ) m = d;
	d = fabsf( a[1] - b[1] ); if ( d > m ) m = d;
	d = fabsf( a[2] - b[2] ); if ( d > m ) m = d;
	return m;
}

int main( void )
{
	sample_t s, p;
	float straight[4] = { 0.8f, 0.2f, 0.1f, 0.5f };
	float premul[4];
	float bg[3] = { 0.1f, 0.2f, 0.3f };
	float outS[3], outP[3];
	float opacities[] = { 0.f, 1.f / 255.f, 0.01f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 0.99f, 1.f };
	int i;

	normalize_straight( straight, &s );
	ASSERT( fabsf( s.assoc[0] - s.unassoc[0] * s.opacity ) < 1e-6f, "straight identity" );
	ASSERT( fabsf( s.opacity - 0.5f ) < 1e-6f, "straight opacity" );

	premul[0] = straight[0] * straight[3];
	premul[1] = straight[1] * straight[3];
	premul[2] = straight[2] * straight[3];
	premul[3] = straight[3];
	normalize_premul( premul, 1e-5f, &p );
	ASSERT( max3abs( s.unassoc, p.unassoc ) < 1e-5f, "straight/premul unassoc match" );
	ASSERT( max3abs( s.assoc, p.assoc ) < 1e-5f, "straight/premul assoc match" );
	ASSERT( fabsf( s.opacity - p.opacity ) < 1e-6f, "opacity match" );

	/* Zero-alpha premul: no div by zero */
	{
		float z[4] = { 0.5f, 0.0f, 0.0f, 0.0f };
		normalize_premul( z, 1e-5f, &p );
		ASSERT( p.flags == 1, "premul zero clamp flag" );
		ASSERT( p.unassoc[0] == 0.f && p.unassoc[1] == 0.f && p.unassoc[2] == 0.f, "cleared unassoc" );
	}

	/* Double-premultiply bug signature */
	{
		sample_t bad;
		normalize_straight( straight, &bad );
		bad.unassoc[0] *= bad.opacity;
		bad.unassoc[1] *= bad.opacity;
		bad.unassoc[2] *= bad.opacity;
		ASSERT( max3abs( bad.unassoc, s.unassoc ) > 0.1f, "double premul diverges" );
	}

	/* Missing alpha multiply: accum with opacity forced 1 */
	{
		float accumBad[3] = { s.unassoc[0], s.unassoc[1], s.unassoc[2] }; /* forgot * opacity */
		float accumGood[3] = { s.unassoc[0] * s.opacity, s.unassoc[1] * s.opacity, s.unassoc[2] * s.opacity };
		ASSERT( max3abs( accumBad, accumGood ) > 0.1f, "missing alpha multiply diverges" );
	}

	/* Single-layer source-over sweep */
	for ( i = 0; i < (int)( sizeof( opacities ) / sizeof( opacities[0] ) ); i++ ) {
		float rgba[4] = { 1.f, 0.f, 0.f, opacities[i] };
		float rgbaP[4];
		normalize_straight( rgba, &s );
		rgbaP[0] = rgba[0] * opacities[i];
		rgbaP[1] = rgba[1] * opacities[i];
		rgbaP[2] = rgba[2] * opacities[i];
		rgbaP[3] = opacities[i];
		normalize_premul( rgbaP, 1e-5f, &p );
		source_over( s.unassoc, s.opacity, bg, outS );
		source_over( p.unassoc, p.opacity, bg, outP );
		ASSERT( max3abs( outS, outP ) < 1e-4f, "source-over encoding equivalence" );
		if ( opacities[i] <= 0.f ) {
			ASSERT( max3abs( outS, bg ) < 1e-6f, "alpha0 preserves bg" );
		}
		if ( opacities[i] >= 1.f ) {
			ASSERT( max3abs( outS, s.unassoc ) < 1e-6f, "alpha1 = surface" );
		}
	}

	printf( "OK: oit alpha normalize + source-over equivalence\n" );
	return 0;
}
