/*
===========================================================================
CEM — G_I / G_II CPU evaluators (Xie et al. arXiv:2508.04076 Eqs. 17–18).
===========================================================================
*/

#include "cem/cem_internal.h"

#include <math.h>

static float cem_dot( const float *a, const float *b )
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static float cem_len( const float *a )
{
	return sqrtf( cem_dot( a, a ) );
}

static void cem_sub( float *o, const float *a, const float *b )
{
	o[0] = a[0] - b[0];
	o[1] = a[1] - b[1];
	o[2] = a[2] - b[2];
}

/*
 * Project vector v onto unit normal n: (v·n) n, returned as scalar (v·n)
 * for the energy product σ̂·δ̂ = (σ·n)(δ·n) when n is unit.
 */
static float cem_proj_scalar( const float *v, const float *n )
{
	return cem_dot( v, n );
}

void Cem_EdgeStretch( float outDelta[3],
	const float uI[3], const float uJ[3],
	const float xI[3], const float xJ[3],
	const float XI[3], const float XJ[3] )
{
	float dx[3], dX[3];
	float lx, lX;
	float stretchRatio;

	if ( !outDelta || !uI || !uJ || !xI || !xJ || !XI || !XJ ) {
		return;
	}

	cem_sub( dx, xJ, xI );
	cem_sub( dX, XJ, XI );
	lx = cem_len( dx );
	lX = cem_len( dX );
	if ( lX < 1e-20f ) {
		outDelta[0] = outDelta[1] = outDelta[2] = 0.0f;
		return;
	}
	stretchRatio = lx / lX - 1.0f;
	if ( stretchRatio <= 0.0f ) {
		outDelta[0] = outDelta[1] = outDelta[2] = 0.0f;
		return;
	}
	/* δ = (u_i - u_j) when lengthened (Heaviside on stretch) */
	outDelta[0] = uI[0] - uJ[0];
	outDelta[1] = uI[1] - uJ[1];
	outDelta[2] = uI[2] - uJ[2];
}

float Cem_EvalGI( const float n[3],
	const float delta1[3], const float delta2[3],
	const float sigmaG3[3], const float sigmaG4[3] )
{
	float nn[3];
	float nl;
	float s3, s4, d1, d2;

	if ( !n || !delta1 || !delta2 || !sigmaG3 || !sigmaG4 ) {
		return 0.0f;
	}
	nl = cem_len( n );
	if ( nl < 1e-20f ) {
		return 0.0f;
	}
	nn[0] = n[0] / nl;
	nn[1] = n[1] / nl;
	nn[2] = n[2] / nl;

	s3 = cem_proj_scalar( sigmaG3, nn );
	s4 = cem_proj_scalar( sigmaG4, nn );
	d1 = cem_proj_scalar( delta1, nn );
	d2 = cem_proj_scalar( delta2, nn );
	return 0.5f * ( s3 * d1 + s4 * d2 );
}

float Cem_EvalGII( const float n[3],
	const float delta1[3], const float delta2[3],
	const float sigmaG6[3] )
{
	float nn[3];
	float nl;
	float s6, d1, d2;

	if ( !n || !delta1 || !delta2 || !sigmaG6 ) {
		return 0.0f;
	}
	nl = cem_len( n );
	if ( nl < 1e-20f ) {
		return 0.0f;
	}
	nn[0] = n[0] / nl;
	nn[1] = n[1] / nl;
	nn[2] = n[2] / nl;

	s6 = cem_proj_scalar( sigmaG6, nn );
	d1 = cem_proj_scalar( delta1, nn );
	d2 = cem_proj_scalar( delta2, nn );
	return 0.5f * ( s6 * d1 + s6 * d2 );
}

int Cem_ShouldFail( float G, float Gc )
{
	if ( Gc < 0.0f ) {
		Gc = 0.0f;
	}
	return ( G >= Gc ) ? 1 : 0;
}
