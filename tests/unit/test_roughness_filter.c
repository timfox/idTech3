/*
 * Unit test: Toksvig + geometric roughness filters (pbr_brdf_core.glsl).
 */
#include <math.h>
#include <stdio.h>

#define ASSERT(cond, msg) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while (0)

#define ASSERTF(cond, msg, ...) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: " msg "\n", __VA_ARGS__); return 1; } \
} while (0)

static float toksvig_roughness( float roughness, float variance, float strength )
{
	float v;
	float toksvig;
	float alpha;

	if ( strength <= 0.0f ) {
		return roughness;
	}
	v = variance;
	if ( v > 0.5f ) {
		v = 0.5f;
	}
	toksvig = v / ( 1.0f + v );
	alpha = roughness * roughness;
	if ( alpha < 0.0004f ) {
		alpha = 0.0004f;
	}
	alpha += toksvig * strength;
	if ( alpha < 0.0004f ) {
		alpha = 0.0004f;
	}
	if ( alpha > 1.0f ) {
		alpha = 1.0f;
	}
	roughness = sqrtf( alpha );
	if ( roughness < 0.02f ) {
		roughness = 0.02f;
	}
	if ( roughness > 1.0f ) {
		roughness = 1.0f;
	}
	return roughness;
}

static float geometric_roughness( float roughness, float geoVariance )
{
	float alpha = roughness * roughness;
	float geoAlpha = geoVariance * 0.25f;

	if ( geoAlpha < 0.0f ) {
		geoAlpha = 0.0f;
	}
	if ( geoAlpha > 0.25f ) {
		geoAlpha = 0.25f;
	}
	if ( alpha < 0.0004f ) {
		alpha = 0.0004f;
	}
	if ( geoAlpha > alpha ) {
		alpha = geoAlpha;
	}
	roughness = sqrtf( alpha );
	if ( roughness < 0.02f ) {
		roughness = 0.02f;
	}
	if ( roughness > 1.0f ) {
		roughness = 1.0f;
	}
	return roughness;
}

int main( void )
{
	float r;

	ASSERTF( fabsf( toksvig_roughness( 0.1f, 0.0f, 1.0f ) - 0.1f ) < 1e-5f,
		"zero variance unchanged (got %.4f)", toksvig_roughness( 0.1f, 0.0f, 1.0f ) );

	r = toksvig_roughness( 0.02f, 0.5f, 1.0f );
	ASSERT( r >= 0.02f && r <= 1.0f, "toksvig clamped to [0.02,1]" );
	ASSERT( r > 0.02f, "high variance inflates roughness" );

	r = geometric_roughness( 0.02f, 4.0f );
	ASSERTF( r >= 0.25f - 1e-4f, "geo floor raises sharp roughness (got %.4f)", r );

	r = geometric_roughness( 0.8f, 0.0f );
	ASSERTF( fabsf( r - 0.8f ) < 1e-4f, "zero geoVariance preserves roughness (got %.4f)", r );

	printf( "unit_roughness_filter: PASS\n" );
	return 0;
}
