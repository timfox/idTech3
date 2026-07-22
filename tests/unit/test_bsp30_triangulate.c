/*
 * Unit test: BSP30 planar ear-clip triangulation.
 *
 * Regression for surf_aztec AZ letter wedges: triangle-fan from vertex 0 can
 * produce exterior triangles on reflex n-gons. Ear clipping must keep every
 * triangle centroid inside the polygon.
 */
#include <stdio.h>

#include "tr_bsp30_triangulate.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while (0)

static void fan( int n, int *idx ) {
	int j;
	for ( j = 0; j < n - 2; j++ ) {
		idx[j * 3 + 0] = 0;
		idx[j * 3 + 1] = j + 1;
		idx[j * 3 + 2] = j + 2;
	}
}

int main( void ) {
	/* Convex quad — fan and ear-clip both valid. */
	{
		const float xyz[] = {
			0, 0, 0,
			1, 0, 0,
			1, 1, 0,
			0, 1, 0
		};
		int idx[6];
		int n = R_Bsp30_TriangulateFace( xyz, 4, idx, 6 );
		ASSERT( n == 6, "convex quad index count" );
		ASSERT( R_Bsp30_TriangleCentroidInside( xyz, 4, idx, n ),
			"convex quad centroids inside" );
	}

	/*
	 * Concave quad (chevron). Fan hub at vertex 0 fills the reflex bay:
	 * triangle (0,2,3) lies outside the polygon.
	 *
	 *   2
	 *   |\
	 *   | \
	 *   |  1
	 *   | /
	 *   |/
	 *   0----3   (3 is to the right of 0; reflex at 0–1–2 bay uses 3)
	 *
	 * Better known dart:
	 *   0(0,0), 1(3,0), 2(1,1), 3(3,2), 4(0,2)
	 * Fan (0,2,3) and (0,3,4) — (0,2,3) centroid is outside the C notch.
	 */
	{
		const float xyz[] = {
			0, 0, 0,
			3, 0, 0,
			1, 1, 0,
			3, 2, 0,
			0, 2, 0
		};
		int ear[9];
		int fanIdx[9];
		int n;

		fan( 5, fanIdx );
		ASSERT( !R_Bsp30_TriangleCentroidInside( xyz, 5, fanIdx, 9 ),
			"fan must produce exterior triangle on concave pentagon" );

		n = R_Bsp30_TriangulateFace( xyz, 5, ear, 9 );
		ASSERT( n == 9, "concave pentagon index count" );
		ASSERT( R_Bsp30_TriangleCentroidInside( xyz, 5, ear, n ),
			"ear-clip concave pentagon centroids inside" );
	}

	/* Triangle passthrough. */
	{
		const float xyz[] = { 0, 0, 0, 1, 0, 0, 0, 1, 0 };
		int idx[3];
		int n = R_Bsp30_TriangulateFace( xyz, 3, idx, 3 );
		ASSERT( n == 3, "triangle count" );
		ASSERT( idx[0] == 0 && idx[1] == 1 && idx[2] == 2, "triangle indices" );
	}

	printf( "unit_bsp30_triangulate: PASS\n" );
	return 0;
}
