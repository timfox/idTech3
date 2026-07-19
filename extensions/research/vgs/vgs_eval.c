/*
===========================================================================
VGS — Algorithm 1 CPU projector (McGraw MIG 2024).
===========================================================================
*/

#include "vgs/vgs_internal.h"

#include <math.h>

static void vgs_sub( float *o, const float *a, const float *b )
{
	o[0] = a[0] - b[0];
	o[1] = a[1] - b[1];
	o[2] = a[2] - b[2];
}

static void vgs_scale( float *o, const float *a, float s )
{
	o[0] = a[0] * s;
	o[1] = a[1] * s;
	o[2] = a[2] * s;
}

static float vgs_dot( const float *a, const float *b )
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static float vgs_len( const float *a )
{
	return sqrtf( vgs_dot( a, a ) );
}

static void vgs_cross( float *o, const float *a, const float *b )
{
	o[0] = a[1] * b[2] - a[2] * b[1];
	o[1] = a[2] * b[0] - a[0] * b[2];
	o[2] = a[0] * b[1] - a[1] * b[0];
}

/* proj_x(y) = ((x·y)/(x·x)) x */
static void vgs_proj( float *o, const float *x, const float *y )
{
	float xx = vgs_dot( x, x );
	float s;

	if ( xx < 1e-20f ) {
		o[0] = o[1] = o[2] = 0.0f;
		return;
	}
	s = vgs_dot( x, y ) / xx;
	vgs_scale( o, x, s );
}

float Vgs_ParallelepipedVolume( const float *p )
{
	float v0[3], v1[3], v2[3], cross[3];
	const float *p0 = p;
	const float *p1 = p + 3;
	const float *p2 = p + 6;
	const float *p4 = p + 12;

	/* Use first corner basis: e0=p1-p0, e1=p2-p0, e2=p4-p0 (Fig. 2 z-order). */
	vgs_sub( v0, p1, p0 );
	vgs_sub( v1, p2, p0 );
	vgs_sub( v2, p4, p0 );
	vgs_cross( cross, v0, v1 );
	return vgs_dot( cross, v2 );
}

int Vgs_ProjectVoxel( float alpha, float beta, int vgsIt,
	float *p, const float *w, float r, float v0Volume )
{
	int it, i;
	float c[3];
	float v[3][3];
	float u[3][3];
	float tmp[3], tmp2[3];
	float V;
	float scale;

	if ( !p || !w || vgsIt < 1 || r <= 0.0f || v0Volume <= 0.0f ) {
		return -1;
	}
	if ( alpha < 0.0f ) {
		alpha = 0.0f;
	}
	if ( alpha > 1.0f ) {
		alpha = 1.0f;
	}
	if ( beta < 0.0f ) {
		beta = 0.0f;
	}
	if ( beta > 1.0f ) {
		beta = 1.0f;
	}

	for ( it = 0; it < vgsIt; it++ ) {
		/* Centroid */
		c[0] = c[1] = c[2] = 0.0f;
		for ( i = 0; i < 8; i++ ) {
			c[0] += p[i * 3 + 0];
			c[1] += p[i * 3 + 1];
			c[2] += p[i * 3 + 2];
		}
		c[0] *= 0.125f;
		c[1] *= 0.125f;
		c[2] *= 0.125f;

		/*
		 * Averaged edges (Fig. 2 z-order):
		 * v0: (p1-p0)+(p3-p2)+(p5-p4)+(p7-p6)
		 * v1: (p2-p0)+(p3-p1)+(p6-p4)+(p7-p5)
		 * v2: (p4-p0)+(p5-p1)+(p6-p2)+(p7-p3)
		 */
		{
			float e[3];

			v[0][0] = v[0][1] = v[0][2] = 0.0f;
			vgs_sub( e, p + 3, p );           v[0][0] += e[0]; v[0][1] += e[1]; v[0][2] += e[2];
			vgs_sub( e, p + 9, p + 6 );       v[0][0] += e[0]; v[0][1] += e[1]; v[0][2] += e[2];
			vgs_sub( e, p + 15, p + 12 );     v[0][0] += e[0]; v[0][1] += e[1]; v[0][2] += e[2];
			vgs_sub( e, p + 21, p + 18 );     v[0][0] += e[0]; v[0][1] += e[1]; v[0][2] += e[2];
			v[0][0] *= 0.25f; v[0][1] *= 0.25f; v[0][2] *= 0.25f;

			v[1][0] = v[1][1] = v[1][2] = 0.0f;
			vgs_sub( e, p + 6, p );           v[1][0] += e[0]; v[1][1] += e[1]; v[1][2] += e[2];
			vgs_sub( e, p + 9, p + 3 );       v[1][0] += e[0]; v[1][1] += e[1]; v[1][2] += e[2];
			vgs_sub( e, p + 18, p + 12 );     v[1][0] += e[0]; v[1][1] += e[1]; v[1][2] += e[2];
			vgs_sub( e, p + 21, p + 15 );     v[1][0] += e[0]; v[1][1] += e[1]; v[1][2] += e[2];
			v[1][0] *= 0.25f; v[1][1] *= 0.25f; v[1][2] *= 0.25f;

			v[2][0] = v[2][1] = v[2][2] = 0.0f;
			vgs_sub( e, p + 12, p );          v[2][0] += e[0]; v[2][1] += e[1]; v[2][2] += e[2];
			vgs_sub( e, p + 15, p + 3 );      v[2][0] += e[0]; v[2][1] += e[1]; v[2][2] += e[2];
			vgs_sub( e, p + 18, p + 6 );      v[2][0] += e[0]; v[2][1] += e[1]; v[2][2] += e[2];
			vgs_sub( e, p + 21, p + 9 );      v[2][0] += e[0]; v[2][1] += e[1]; v[2][2] += e[2];
			v[2][0] *= 0.25f; v[2][1] *= 0.25f; v[2][2] *= 0.25f;
		}

		/* Relaxed Gram-Schmidt */
		vgs_proj( tmp, v[1], v[0] );
		vgs_proj( tmp2, v[2], v[0] );
		u[0][0] = v[0][0] - alpha * ( tmp[0] + tmp2[0] );
		u[0][1] = v[0][1] - alpha * ( tmp[1] + tmp2[1] );
		u[0][2] = v[0][2] - alpha * ( tmp[2] + tmp2[2] );

		vgs_proj( tmp, v[2], v[1] );
		vgs_proj( tmp2, v[0], v[1] );
		u[1][0] = v[1][0] - alpha * ( tmp[0] + tmp2[0] );
		u[1][1] = v[1][1] - alpha * ( tmp[1] + tmp2[1] );
		u[1][2] = v[1][2] - alpha * ( tmp[2] + tmp2[2] );

		vgs_proj( tmp, v[0], v[2] );
		vgs_proj( tmp2, v[1], v[2] );
		u[2][0] = v[2][0] - alpha * ( tmp[0] + tmp2[0] );
		u[2][1] = v[2][1] - alpha * ( tmp[1] + tmp2[1] );
		u[2][2] = v[2][2] - alpha * ( tmp[2] + tmp2[2] );

		/* Equal edge length: ||u|| = (1-β)r + β(||v||/2) */
		for ( i = 0; i < 3; i++ ) {
			float ul = vgs_len( u[i] );
			float vl = vgs_len( v[i] );
			float target = ( 1.0f - beta ) * r + beta * ( vl * 0.5f );
			if ( ul < 1e-12f ) {
				u[i][0] = target;
				u[i][1] = 0.0f;
				u[i][2] = 0.0f;
			} else {
				vgs_scale( u[i], u[i], target / ul );
			}
		}

		/* Volume constraint */
		vgs_cross( tmp, u[0], u[1] );
		V = vgs_dot( tmp, u[2] );
		if ( fabsf( V ) < 1e-20f ) {
			continue;
		}
		scale = 0.5f * cbrtf( v0Volume / V );
		for ( i = 0; i < 3; i++ ) {
			vgs_scale( u[i], u[i], scale );
		}

		/* Reconstruct parallelepiped corners */
		if ( w[0] != 0.0f ) {
			p[0] = c[0] - u[0][0] - u[1][0] - u[2][0];
			p[1] = c[1] - u[0][1] - u[1][1] - u[2][1];
			p[2] = c[2] - u[0][2] - u[1][2] - u[2][2];
		}
		if ( w[1] != 0.0f ) {
			p[3] = c[0] + u[0][0] - u[1][0] - u[2][0];
			p[4] = c[1] + u[0][1] - u[1][1] - u[2][1];
			p[5] = c[2] + u[0][2] - u[1][2] - u[2][2];
		}
		if ( w[2] != 0.0f ) {
			p[6] = c[0] - u[0][0] + u[1][0] - u[2][0];
			p[7] = c[1] - u[0][1] + u[1][1] - u[2][1];
			p[8] = c[2] - u[0][2] + u[1][2] - u[2][2];
		}
		if ( w[3] != 0.0f ) {
			p[9] = c[0] + u[0][0] + u[1][0] - u[2][0];
			p[10] = c[1] + u[0][1] + u[1][1] - u[2][1];
			p[11] = c[2] + u[0][2] + u[1][2] - u[2][2];
		}
		if ( w[4] != 0.0f ) {
			p[12] = c[0] - u[0][0] - u[1][0] + u[2][0];
			p[13] = c[1] - u[0][1] - u[1][1] + u[2][1];
			p[14] = c[2] - u[0][2] - u[1][2] + u[2][2];
		}
		if ( w[5] != 0.0f ) {
			p[15] = c[0] + u[0][0] - u[1][0] + u[2][0];
			p[16] = c[1] + u[0][1] - u[1][1] + u[2][1];
			p[17] = c[2] + u[0][2] - u[1][2] + u[2][2];
		}
		if ( w[6] != 0.0f ) {
			p[18] = c[0] - u[0][0] + u[1][0] + u[2][0];
			p[19] = c[1] - u[0][1] + u[1][1] + u[2][1];
			p[20] = c[2] - u[0][2] + u[1][2] + u[2][2];
		}
		if ( w[7] != 0.0f ) {
			p[21] = c[0] + u[0][0] + u[1][0] + u[2][0];
			p[22] = c[1] + u[0][1] + u[1][1] + u[2][1];
			p[23] = c[2] + u[0][2] + u[1][2] + u[2][2];
		}
	}

	return 0;
}
