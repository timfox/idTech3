/*
 * Unit tests: CM_BoundsIntersect / CM_BoundsIntersectPoint (cm_bounds.cpp).
 * No collision world or cm globals required.
 */
#include <cstdio>
#include <cstdlib>

#include "qcommon/q_shared.h"

qboolean CM_BoundsIntersect( const vec3_t mins, const vec3_t maxs, const vec3_t mins2, const vec3_t maxs2 );
qboolean CM_BoundsIntersectPoint( const vec3_t mins, const vec3_t maxs, const vec3_t point );

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		std::fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while(0)

static void v3( vec3_t o, float x, float y, float z )
{
	o[0] = x;
	o[1] = y;
	o[2] = z;
}

static int test_bounds_intersect_overlap(void)
{
	vec3_t a0, a1, b0, b1;

	v3( a0, 0.0f, 0.0f, 0.0f );
	v3( a1, 1.0f, 1.0f, 1.0f );
	v3( b0, 0.5f, 0.5f, 0.5f );
	v3( b1, 2.0f, 2.0f, 2.0f );
	ASSERT( CM_BoundsIntersect( a0, a1, b0, b1 ) == qtrue, "AABB overlap" );
	ASSERT( CM_BoundsIntersect( b0, b1, a0, a1 ) == qtrue, "AABB overlap symmetric" );
	return 0;
}

static int test_bounds_intersect_separated(void)
{
	vec3_t a0, a1, b0, b1;

	v3( a0, 0.0f, 0.0f, 0.0f );
	v3( a1, 1.0f, 1.0f, 1.0f );
	v3( b0, 3.0f, 0.0f, 0.0f );
	v3( b1, 4.0f, 1.0f, 1.0f );
	ASSERT( CM_BoundsIntersect( a0, a1, b0, b1 ) == qfalse, "separated on X" );

	v3( b0, 0.0f, 5.0f, 0.0f );
	v3( b1, 1.0f, 6.0f, 1.0f );
	ASSERT( CM_BoundsIntersect( a0, a1, b0, b1 ) == qfalse, "separated on Y" );

	v3( b0, 0.0f, 0.0f, 8.0f );
	v3( b1, 1.0f, 1.0f, 9.0f );
	ASSERT( CM_BoundsIntersect( a0, a1, b0, b1 ) == qfalse, "separated on Z" );
	return 0;
}

static int test_bounds_intersect_touching_face(void)
{
	vec3_t a0, a1, b0, b1;

	/* Face-touch at x=1 uses BOUNDS_CLIP_EPSILON (0.25): still counts as intersect */
	v3( a0, 0.0f, 0.0f, 0.0f );
	v3( a1, 1.0f, 1.0f, 1.0f );
	v3( b0, 1.0f, 0.0f, 0.0f );
	v3( b1, 2.0f, 1.0f, 1.0f );
	ASSERT( CM_BoundsIntersect( a0, a1, b0, b1 ) == qtrue, "touching face intersects" );
	return 0;
}

static int test_bounds_intersect_epsilon_gap(void)
{
	vec3_t a0, a1, b0, b1;

	/* Gap 0.3 > 0.25 epsilon on X -> no intersect */
	v3( a0, 0.0f, 0.0f, 0.0f );
	v3( a1, 1.0f, 1.0f, 1.0f );
	v3( b0, 1.3f, 0.0f, 0.0f );
	v3( b1, 2.0f, 1.0f, 1.0f );
	ASSERT( CM_BoundsIntersect( a0, a1, b0, b1 ) == qfalse, "epsilon gap separates" );
	return 0;
}

static int test_bounds_intersect_identical(void)
{
	vec3_t a0, a1, b0, b1;

	v3( a0, -1.0f, 2.0f, 3.0f );
	v3( a1, 4.0f, 5.0f, 6.0f );
	VectorCopy( a0, b0 );
	VectorCopy( a1, b1 );
	ASSERT( CM_BoundsIntersect( a0, a1, b0, b1 ) == qtrue, "identical bounds" );
	return 0;
}

static int test_bounds_point_inside_outside(void)
{
	vec3_t mn, mx, p;

	v3( mn, 0.0f, 0.0f, 0.0f );
	v3( mx, 10.0f, 10.0f, 10.0f );
	v3( p, 5.0f, 5.0f, 5.0f );
	ASSERT( CM_BoundsIntersectPoint( mn, mx, p ) == qtrue, "point center" );

	v3( p, 0.0f, 0.0f, 0.0f );
	ASSERT( CM_BoundsIntersectPoint( mn, mx, p ) == qtrue, "point corner min" );

	v3( p, 10.0f, 10.0f, 10.0f );
	ASSERT( CM_BoundsIntersectPoint( mn, mx, p ) == qtrue, "point corner max" );

	v3( p, 100.0f, 0.0f, 0.0f );
	ASSERT( CM_BoundsIntersectPoint( mn, mx, p ) == qfalse, "point outside" );
	return 0;
}

static int test_bounds_point_on_surface(void)
{
	vec3_t mn, mx, p;

	v3( mn, 0.0f, 0.0f, 0.0f );
	v3( mx, 1.0f, 1.0f, 1.0f );
	v3( p, 1.0f, 0.5f, 0.5f );
	ASSERT( CM_BoundsIntersectPoint( mn, mx, p ) == qtrue, "point on max X face" );
	return 0;
}

static int test_bounds_point_epsilon_outside(void)
{
	vec3_t mn, mx, p;

	v3( mn, 0.0f, 0.0f, 0.0f );
	v3( mx, 1.0f, 1.0f, 1.0f );
	/* 1.26 > 1 + 0.25 */
	v3( p, 1.26f, 0.5f, 0.5f );
	ASSERT( CM_BoundsIntersectPoint( mn, mx, p ) == qfalse, "point past epsilon outside max" );
	return 0;
}

int main( void )
{
	if ( test_bounds_intersect_overlap() ) return 1;
	if ( test_bounds_intersect_separated() ) return 1;
	if ( test_bounds_intersect_touching_face() ) return 1;
	if ( test_bounds_intersect_epsilon_gap() ) return 1;
	if ( test_bounds_intersect_identical() ) return 1;
	if ( test_bounds_point_inside_outside() ) return 1;
	if ( test_bounds_point_on_surface() ) return 1;
	if ( test_bounds_point_epsilon_outside() ) return 1;
	return 0;
}