/*
 * CPU reference tests for Phase 2.3.2 depth_view.glsl / vk_depth_* helpers.
 * Linearize formula must match taa.frag / postfx depthParams.
 */
#include <stdio.h>
#include <math.h>

#define ASSERT(cond, msg) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while (0)

static float linearize_reversed_z( float deviceDepth, float zNear, float zFar )
{
	float zn = zNear > 1e-4f ? zNear : 1e-4f;
	float zf = ( zFar > zn + 1e-3f ) ? zFar : ( zn + 1e-3f );
	float d = deviceDepth;
	float denom;
	if ( d < 0.0f ) d = 0.0f;
	else if ( d > 1.0f ) d = 1.0f;
	denom = zn + d * ( zf - zn );
	if ( denom < 1e-6f ) denom = 1e-6f;
	return ( zn * zf ) / denom;
}

static float positive_view_from_world( const float world[3], const float org[3],
	const float forward[3] )
{
	float dx = world[0] - org[0];
	float dy = world[1] - org[1];
	float dz = world[2] - org[2];
	float len = sqrtf( forward[0] * forward[0] + forward[1] * forward[1] +
		forward[2] * forward[2] );
	float fx, fy, fz, d;
	if ( len < 1e-8f ) {
		return 0.0f;
	}
	fx = forward[0] / len;
	fy = forward[1] / len;
	fz = forward[2] / len;
	d = dx * fx + dy * fy + dz * fz;
	return d > 0.0f ? d : 0.0f;
}

static float traditional01( float viewDepth, float zNear, float zFar )
{
	float zn = zNear > 1e-4f ? zNear : 1e-4f;
	float zf = ( zFar > zn + 1e-3f ) ? zFar : ( zn + 1e-3f );
	float t = ( viewDepth - zn ) / ( zf - zn );
	if ( t < 0.0f ) return 0.0f;
	if ( t > 1.0f ) return 1.0f;
	return t;
}

int main( void )
{
	const float zn = 8.0f;
	const float zf = 8192.0f;
	float nearLin, farLin, midLin;
	float world[3] = { 100.0f, 0.0f, 0.0f };
	float org[3] = { 0.0f, 0.0f, 0.0f };
	float fwd[3] = { 1.0f, 0.0f, 0.0f };
	float viewD, offAxis;

	/* deviceZ=1 → near plane; deviceZ=0 → far plane (reversed-Z). */
	nearLin = linearize_reversed_z( 1.0f, zn, zf );
	farLin = linearize_reversed_z( 0.0f, zn, zf );
	ASSERT( fabsf( nearLin - zn ) < 1e-3f, "deviceZ=1 should linearize to zNear" );
	ASSERT( fabsf( farLin - zf ) < 1e-2f, "deviceZ=0 should linearize to zFar" );

	midLin = linearize_reversed_z( 0.5f, zn, zf );
	ASSERT( midLin > zn && midLin < zf, "mid deviceZ between near and far" );
	/* Harmonic mean at d=0.5: 2*zn*zf/(zn+zf) */
	ASSERT( fabsf( midLin - ( 2.0f * zn * zf ) / ( zn + zf ) ) < 1e-2f,
		"d=0.5 matches harmonic form" );

	viewD = positive_view_from_world( world, org, fwd );
	ASSERT( fabsf( viewD - 100.0f ) < 1e-4f, "on-axis view-depth equals forward distance" );

	{
		float side[3] = { 0.0f, 50.0f, 0.0f };
		offAxis = positive_view_from_world( side, org, fwd );
		ASSERT( fabsf( offAxis ) < 1e-4f, "pure lateral offset → view-depth 0" );
	}

	ASSERT( fabsf( traditional01( zn, zn, zf ) ) < 1e-5f, "near → traditional 0" );
	ASSERT( fabsf( traditional01( zf, zn, zf ) - 1.0f ) < 1e-5f, "far → traditional 1" );
	ASSERT( traditional01( ( zn + zf ) * 0.5f, zn, zf ) > 0.4f &&
		traditional01( ( zn + zf ) * 0.5f, zn, zf ) < 0.6f,
		"mid view-depth maps near 0.5" );

	printf( "OK: depth_view unit checks passed\n" );
	return 0;
}
