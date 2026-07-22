/*
 * CPU round-trip for G-buffer octahedral encode/decode (mirrors gbuffer_octahedral.glsl).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

static void normalize3( float *v )
{
	float len = sqrtf( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
	if ( len < 1e-8f ) {
		v[0] = 0.0f;
		v[1] = 0.0f;
		v[2] = 1.0f;
		return;
	}
	v[0] /= len;
	v[1] /= len;
	v[2] /= len;
}

static void octahedron_wrap( float *xy )
{
	float ax = fabsf( xy[0] );
	float ay = fabsf( xy[1] );
	float ox = ( 1.0f - ay ) * ( xy[0] >= 0.0f ? 1.0f : -1.0f );
	float oy = ( 1.0f - ax ) * ( xy[1] >= 0.0f ? 1.0f : -1.0f );
	xy[0] = ox;
	xy[1] = oy;
}

static void encode_octahedral( const float *nIn, float *e )
{
	float n[3] = { nIn[0], nIn[1], nIn[2] };
	float sum;
	normalize3( n );
	sum = fabsf( n[0] ) + fabsf( n[1] ) + fabsf( n[2] );
	n[0] /= sum;
	n[1] /= sum;
	if ( n[2] < 0.0f ) {
		float xy[2] = { n[0], n[1] };
		octahedron_wrap( xy );
		n[0] = xy[0];
		n[1] = xy[1];
	}
	e[0] = n[0] * 0.5f + 0.5f;
	e[1] = n[1] * 0.5f + 0.5f;
}

static void decode_octahedral( const float *e, float *n )
{
	float f[2] = { e[0] * 2.0f - 1.0f, e[1] * 2.0f - 1.0f };
	float t;
	n[0] = f[0];
	n[1] = f[1];
	n[2] = 1.0f - fabsf( f[0] ) - fabsf( f[1] );
	t = clampf( -n[2], 0.0f, 1.0f );
	n[0] += ( n[0] >= 0.0f ) ? -t : t;
	n[1] += ( n[1] >= 0.0f ) ? -t : t;
	normalize3( n );
}

static int fail_count;

static void check_roundtrip( float x, float y, float z, float maxErr )
{
	float n0[3] = { x, y, z };
	float e[2];
	float n1[3];
	float dot;
	normalize3( n0 );
	encode_octahedral( n0, e );
	decode_octahedral( e, n1 );
	dot = n0[0] * n1[0] + n0[1] * n1[1] + n0[2] * n1[2];
	if ( dot < ( 1.0f - maxErr ) ) {
		fprintf( stderr, "FAIL: oct roundtrip (%.4f,%.4f,%.4f) dot=%.6f enc=(%.4f,%.4f)\n",
			n0[0], n0[1], n0[2], dot, e[0], e[1] );
		fail_count++;
	}
}

int main( void )
{
	fail_count = 0;
	check_roundtrip( 0.0f, 0.0f, 1.0f, 1e-4f );
	check_roundtrip( 0.0f, 0.0f, -1.0f, 2e-3f );
	check_roundtrip( 1.0f, 0.0f, 0.0f, 2e-3f );
	check_roundtrip( 0.0f, 1.0f, 0.0f, 2e-3f );
	check_roundtrip( -0.577f, 0.577f, 0.577f, 2e-3f );
	check_roundtrip( 0.2f, -0.5f, 0.8f, 2e-3f );
	check_roundtrip( -0.9f, -0.1f, -0.3f, 3e-3f );
	if ( fail_count ) {
		fprintf( stderr, "%d octahedral checks failed\n", fail_count );
		return 1;
	}
	printf( "PASS: gbuffer octahedral roundtrip\n" );
	return 0;
}
