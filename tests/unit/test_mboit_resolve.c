/*
 * CPU reference checks for MBOIT optical-depth resolve coverage.
 */
#include <math.h>
#include <stdio.h>

#define ASSERT(cond, msg) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while (0)

static float clampf( float v, float lo, float hi )
{
	if ( v < lo ) {
		return lo;
	}
	if ( v > hi ) {
		return hi;
	}
	return v;
}

static float mboit_depth_from_alpha( float alpha )
{
	float a = clampf( alpha, 0.0f, 0.999f );
	return -logf( fmaxf( 1.0f - a, 1e-5f ) );
}

static float mboit_coverage_from_b0( float b0 )
{
	float opticalDepth;
	float coverage;
	if ( b0 != b0 ) {
		return 0.0f;
	}
	opticalDepth = clampf( b0, 0.0f, 32.0f );
	coverage = 1.0f - expf( -opticalDepth );
	if ( coverage != coverage ) {
		return 0.0f;
	}
	return clampf( coverage, 0.0f, 1.0f );
}

static float mboit_mean_depth( float b0, float b1 )
{
	if ( !( b0 > 1e-5f ) || b1 != b1 ) {
		return 0.0f;
	}
	return clampf( b1 / fmaxf( b0, 1e-5f ), 0.0f, 1.0f );
}

int main( void )
{
	float a = 0.4f;
	float d = mboit_depth_from_alpha( a );
	float coverage = mboit_coverage_from_b0( d );
	ASSERT( fabsf( coverage - a ) < 1e-5f, "single layer coverage matches alpha" );

	{
		float a0 = 0.25f;
		float a1 = 0.5f;
		float b0 = mboit_depth_from_alpha( a0 ) + mboit_depth_from_alpha( a1 );
		float expected = 1.0f - ( 1.0f - a0 ) * ( 1.0f - a1 );
		ASSERT( fabsf( mboit_coverage_from_b0( b0 ) - expected ) < 1e-5f,
			"stacked optical depths match product revealage coverage" );
	}

	{
		float d0 = mboit_depth_from_alpha( 0.25f );
		float d1 = mboit_depth_from_alpha( 0.50f );
		float z0 = 0.2f;
		float z1 = 0.8f;
		float b0 = d0 + d1;
		float b1 = d0 * z0 + d1 * z1;
		float mean = mboit_mean_depth( b0, b1 );
		ASSERT( mean > z0 && mean < z1, "mean depth lies inside weighted layer depths" );
		ASSERT( fabsf( mean - ( b1 / b0 ) ) < 1e-6f, "mean depth equals b1/b0" );
	}

	ASSERT( mboit_coverage_from_b0( -1.0f ) == 0.0f, "negative optical depth clamps empty" );
	ASSERT( mboit_coverage_from_b0( 1000.0f ) <= 1.0f, "large optical depth clamps finite" );
	ASSERT( mboit_mean_depth( 0.0f, 1.0f ) == 0.0f, "empty moments have zero mean depth" );

	printf( "unit_mboit_resolve: PASS\n" );
	return 0;
}
