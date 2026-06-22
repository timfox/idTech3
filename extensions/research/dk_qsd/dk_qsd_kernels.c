/*
===========================================================================
Domany–Kinzel local kernels W, V (bond-DP line).
===========================================================================
*/

#include "dk_qsd/dk_qsd_internal.h"

#include <math.h>

void DK_Kernels_Fill( float p, float W[2][2][2], float V[2][2] )
{
	const float p1 = p;
	const float p2 = p * ( 2.0f - p );
	int s0;
	int s1;
	int sout;
	int s;

	for ( s0 = 0; s0 < 2; s0++ ) {
		for ( s1 = 0; s1 < 2; s1++ ) {
			const int n = s0 + s1;
			const float pn = ( n == 0 ) ? 0.0f : ( n == 1 ) ? p1 : p2;
			W[s0][s1][0] = 1.0f - pn;
			W[s0][s1][1] = pn;
		}
	}

	for ( s = 0; s < 2; s++ ) {
		const float pn = ( s == 0 ) ? 0.0f : p1;
		V[s][0] = 1.0f - pn;
		V[s][1] = pn;
	}
}

float DK_Kernels_W( float p, int s0, int s1, int out )
{
	float W[2][2][2];
	float V[2][2];
	DK_Kernels_Fill( p, W, V );
	return W[s0 & 1][s1 & 1][out & 1];
}

float DK_Kernels_V( float p, int s, int out )
{
	float W[2][2][2];
	float V[2][2];
	DK_Kernels_Fill( p, W, V );
	return V[s & 1][out & 1];
}
