/*
 * CPU reference tests for Phase 2.5 bounded WBOIT weight
 * (mirrors oit_weight.glsl / vk_oit_weight_evaluate).
 */
#include <stdio.h>
#include <math.h>

#define ASSERT(cond, msg) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while (0)

static float bounded_weight( float opacity, float viewDepth, float zn, float zf )
{
	float a = opacity;
	float zTrad, aFactor, zFactor, w;
	if ( a < 0.f ) a = 0.f;
	else if ( a > 1.f ) a = 1.f;
	zTrad = ( viewDepth - zn ) / ( zf - zn );
	if ( zTrad < 0.f ) zTrad = 0.f;
	else if ( zTrad > 1.f ) zTrad = 1.f;
	{
		float t = a * 10.f;
		if ( t > 1.f ) t = 1.f;
		aFactor = powf( t + 0.01f, 3.f );
	}
	zFactor = powf( 1.f - zTrad * 0.9f, 3.f );
	w = aFactor * 1e3f * zFactor;
	if ( w < 1e-2f ) w = 1e-2f;
	else if ( w > 3e3f ) w = 3e3f;
	return w;
}

int main( void )
{
	const float zn = 8.f, zf = 8192.f;
	float w0, w1, wNear, wFar, wThin;

	w0 = bounded_weight( 0.f, zn, zn, zf );
	w1 = bounded_weight( 1.f, zn, zn, zf );
	ASSERT( w0 >= 1e-2f - 1e-6f, "zero alpha still clamped to minWeight" );
	ASSERT( w1 > w0, "opaque near should outweigh zero-alpha" );

	wNear = bounded_weight( 0.5f, zn, zn, zf );
	wFar = bounded_weight( 0.5f, zf, zn, zf );
	ASSERT( wNear > wFar, "near fragments must outweigh far (same alpha)" );

	wThin = bounded_weight( 0.05f, zn, zn, zf );
	ASSERT( wThin < wNear, "lower opacity → lower weight at same depth" );
	ASSERT( wThin >= 1e-2f && wThin <= 3e3f, "weight stays in [min,max]" );

	/* Single-layer identity: coverage uses alpha; weight scales accum only. */
	{
		float alpha = 0.4f;
		float w = bounded_weight( alpha, 100.f, zn, zf );
		float accumA = alpha * w;
		ASSERT( accumA > 0.f, "accum.a positive for visible layer" );
	}

	printf( "OK: oit_weight unit checks passed\n" );
	return 0;
}
