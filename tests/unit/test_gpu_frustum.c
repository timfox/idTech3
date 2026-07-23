/*
 * CPU reference: frustum sphere visibility (GPU Visibility M1).
 */
#include <stdio.h>
#include <math.h>
#include "../../renderers/vulkan/vk_gpu_frustum_math.h"

static int fails;

static void expect( int cond, const char *msg )
{
	if ( !cond ) {
		printf( "FAIL: %s\n", msg );
		fails++;
	}
}

int main( void )
{
	float n[4][3] = {
		{ 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }
	};
	float d[4] = { -10, -10, -10, -10 }; /* planes at x=±10, y=±10 facing inward */
	float inside[4] = { 0, 0, 0, 1 };
	float outside[4] = { 20, 0, 0, 1 };
	float edge[4] = { 10.5f, 0, 0, 1 }; /* center outside but radius reaches */
	float nanSphere[4] = { 0.0f / 0.0f, 0, 0, 1 };
	float huge[4] = { 0, 0, 0, 1.0e7f };

	expect( GpuFrustum_SphereVisible( inside, n, d ) == 1, "inside visible" );
	expect( GpuFrustum_SphereVisible( outside, n, d ) == 0, "outside culled" );
	expect( GpuFrustum_SphereVisible( edge, n, d ) == 1, "edge intersecting visible" );
	expect( GpuFrustum_SphereVisible( nanSphere, n, d ) == 1, "NaN conservative keep" );
	expect( GpuFrustum_SphereVisible( huge, n, d ) == 1, "huge bounds keep" );

	if ( fails ) {
		printf( "%d failed\n", fails );
		return 1;
	}
	printf( "PASS: unit_gpu_frustum\n" );
	return 0;
}
