/*
 * Unit test: GGX NDF reference values (pbr_brdf_core.glsl ndf_ggx).
 */
#include <math.h>
#include <stdio.h>

#define ASSERT(cond, msg) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while (0)

#define ASSERTF(cond, msg, ...) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: " msg "\n", __VA_ARGS__); return 1; } \
} while (0)

#define PBR_PI 3.14159265358979323846

static double ndf_ggx( double nh, double roughness )
{
	double alpha = roughness * roughness;
	if ( alpha < 1e-4 ) {
		alpha = 1e-4;
	}
	double alphaSq = alpha * alpha;
	double d = ( nh * alphaSq - nh ) * nh + 1.0;
	return alphaSq / ( PBR_PI * d * d );
}

int main( void )
{
	double v;

	v = ndf_ggx( 1.0, 0.5 );
	ASSERTF( fabs( v - 5.092958179 ) < 1e-6, "NH=1 rough=0.5 expected ~5.093 got %.9f", v );

	v = ndf_ggx( 1.0, 1.0 );
	ASSERTF( fabs( v - ( 1.0 / PBR_PI ) ) < 1e-6, "NH=1 rough=1 expected 1/pi got %.9f", v );

	v = ndf_ggx( 0.0, 0.25 );
	ASSERTF( fabs( v - ( 0.00390625 / PBR_PI ) ) < 1e-6,
		"NH=0 rough=0.25 expected alpha^2/pi got %.9f", v );

	v = ndf_ggx( 0.8, 0.2 );
	ASSERT( v > 0.0 && v < 10.0, "mid lobe stays finite" );

	printf( "unit_brdf_reference: PASS\n" );
	return 0;
}
