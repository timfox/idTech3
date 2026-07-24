/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

BSP30 planar face triangulation (ear clipping + hub-fan fallback).

GoldSrc / BSP30 faces may be non-convex n-gons. Triangle-fan from vertex 0
emits exterior triangles that appear as hard black wedges around letter
meshes (e.g. surf_aztec func_illusionary *17 "AZ"). Ear clipping keeps
triangles inside the polygon when it succeeds; when it does not, try every
vertex as a fan hub and keep the first triangulation whose centroids lie
inside the polygon.
===========================================================================
*/

#include "tr_bsp30_triangulate.h"

#include <math.h>
#include <string.h>

#define BSP30_TRI_EPS 1.0e-6f

static float Bsp30_Cross2( float ax, float ay, float bx, float by, float cx, float cy ) {
	return ( bx - ax ) * ( cy - ay ) - ( by - ay ) * ( cx - ax );
}

static float Bsp30_Dot3( const float *a, const float *b ) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void Bsp30_Cross3( const float *a, const float *b, float *out ) {
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

static void Bsp30_Sub3( const float *a, const float *b, float *out ) {
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}

static int Bsp30_Normalize3( float *v ) {
	float len = sqrtf( Bsp30_Dot3( v, v ) );
	if ( len < BSP30_TRI_EPS ) {
		return 0;
	}
	v[0] /= len;
	v[1] /= len;
	v[2] /= len;
	return 1;
}

static int Bsp30_ProjectFace( const float *xyz, int numPoints, float *out2,
	float *outArea2 ) {
	float e0[3], e1[3], normal[3], tmp[3], u[3], v[3];
	float area2 = 0.0f;
	int i, best;

	if ( numPoints < 3 ) {
		return 0;
	}

	/* Accumulated Newell normal — stable for non-convex coplanar rings. */
	normal[0] = normal[1] = normal[2] = 0.0f;
	for ( i = 0; i < numPoints; i++ ) {
		const float *p0 = xyz + i * 3;
		const float *p1 = xyz + ( ( i + 1 ) % numPoints ) * 3;
		normal[0] += ( p0[1] - p1[1] ) * ( p0[2] + p1[2] );
		normal[1] += ( p0[2] - p1[2] ) * ( p0[0] + p1[0] );
		normal[2] += ( p0[0] - p1[0] ) * ( p0[1] + p1[1] );
	}
	if ( !Bsp30_Normalize3( normal ) ) {
		Bsp30_Sub3( xyz + 3, xyz, e0 );
		for ( i = 2; i < numPoints; i++ ) {
			Bsp30_Sub3( xyz + i * 3, xyz, e1 );
			Bsp30_Cross3( e0, e1, normal );
			if ( Bsp30_Normalize3( normal ) ) {
				break;
			}
		}
		if ( i >= numPoints ) {
			return 0;
		}
	}

	best = 0;
	if ( fabsf( normal[1] ) < fabsf( normal[best] ) ) {
		best = 1;
	}
	if ( fabsf( normal[2] ) < fabsf( normal[best] ) ) {
		best = 2;
	}
	tmp[0] = tmp[1] = tmp[2] = 0.0f;
	tmp[best] = 1.0f;
	Bsp30_Cross3( normal, tmp, u );
	if ( !Bsp30_Normalize3( u ) ) {
		return 0;
	}
	Bsp30_Cross3( normal, u, v );

	for ( i = 0; i < numPoints; i++ ) {
		const float *p = xyz + i * 3;
		out2[i * 2 + 0] = Bsp30_Dot3( p, u );
		out2[i * 2 + 1] = Bsp30_Dot3( p, v );
	}

	for ( i = 0; i < numPoints; i++ ) {
		const float *p0 = out2 + i * 2;
		const float *p1 = out2 + ( ( i + 1 ) % numPoints ) * 2;
		area2 += p0[0] * p1[1] - p1[0] * p0[1];
	}
	*outArea2 = area2 * 0.5f;
	return 1;
}

static int Bsp30_IsEar( const float *poly2, const int *idx, int n, int u, int v, int w,
	int ccw ) {
	float ax = poly2[idx[u] * 2 + 0], ay = poly2[idx[u] * 2 + 1];
	float bx = poly2[idx[v] * 2 + 0], by = poly2[idx[v] * 2 + 1];
	float cx = poly2[idx[w] * 2 + 0], cy = poly2[idx[w] * 2 + 1];
	float cross = Bsp30_Cross2( ax, ay, bx, by, cx, cy );
	int i;

	if ( ccw ) {
		if ( cross <= BSP30_TRI_EPS ) {
			return 0;
		}
	} else {
		if ( cross >= -BSP30_TRI_EPS ) {
			return 0;
		}
	}

	for ( i = 0; i < n; i++ ) {
		float px, py, c0, c1, c2;
		if ( i == u || i == v || i == w ) {
			continue;
		}
		px = poly2[idx[i] * 2 + 0];
		py = poly2[idx[i] * 2 + 1];
		c0 = Bsp30_Cross2( ax, ay, bx, by, px, py );
		c1 = Bsp30_Cross2( bx, by, cx, cy, px, py );
		c2 = Bsp30_Cross2( cx, cy, ax, ay, px, py );
		if ( ccw ) {
			if ( c0 >= -BSP30_TRI_EPS && c1 >= -BSP30_TRI_EPS && c2 >= -BSP30_TRI_EPS ) {
				return 0;
			}
		} else {
			if ( c0 <= BSP30_TRI_EPS && c1 <= BSP30_TRI_EPS && c2 <= BSP30_TRI_EPS ) {
				return 0;
			}
		}
	}
	return 1;
}

static int Bsp30_EarClip( const float *poly2, int numPoints, int ccw, int *outIndices,
	int maxIndices ) {
	int idx[BSP30_TRIANGULATE_MAX_POINTS];
	int remaining;
	int guard;
	int v = 0;
	int tris = 0;
	int i;

	if ( numPoints < 3 || numPoints > BSP30_TRIANGULATE_MAX_POINTS ) {
		return -1;
	}
	if ( maxIndices < ( numPoints - 2 ) * 3 ) {
		return -1;
	}

	for ( i = 0; i < numPoints; i++ ) {
		idx[i] = i;
	}
	remaining = numPoints;
	guard = numPoints * numPoints;

	while ( remaining > 2 && guard-- > 0 ) {
		int u = ( v + remaining - 1 ) % remaining;
		int w = ( v + 1 ) % remaining;

		if ( !Bsp30_IsEar( poly2, idx, remaining, u, v, w, ccw ) ) {
			v = ( v + 1 ) % remaining;
			continue;
		}

		outIndices[tris * 3 + 0] = idx[u];
		outIndices[tris * 3 + 1] = idx[v];
		outIndices[tris * 3 + 2] = idx[w];
		tris++;

		for ( u = v; u < remaining - 1; u++ ) {
			idx[u] = idx[u + 1];
		}
		remaining--;
		v = 0;
	}

	if ( remaining != 2 || tris != numPoints - 2 ) {
		return -1;
	}
	return tris * 3;
}

static void Bsp30_FanFromHub( int numPoints, int hub, int *outIndices ) {
	int j, t = 0;
	for ( j = 1; j < numPoints - 1; j++ ) {
		int a = ( hub + j ) % numPoints;
		int b = ( hub + j + 1 ) % numPoints;
		outIndices[t * 3 + 0] = hub;
		outIndices[t * 3 + 1] = a;
		outIndices[t * 3 + 2] = b;
		t++;
	}
}

static void Bsp30_FanFallback( int numPoints, int *outIndices ) {
	Bsp30_FanFromHub( numPoints, 0, outIndices );
}

int R_Bsp30_TriangleCentroidInside( const float *xyz, int numPoints,
	const int *indices, int numIndices ) {
	float poly2[BSP30_TRIANGULATE_MAX_POINTS * 2];
	float area2;
	int t, i;

	if ( !xyz || !indices || numPoints < 3 || numIndices < 3 ||
		numPoints > BSP30_TRIANGULATE_MAX_POINTS ) {
		return 0;
	}
	if ( !Bsp30_ProjectFace( xyz, numPoints, poly2, &area2 ) ) {
		return 0;
	}

	for ( t = 0; t < numIndices; t += 3 ) {
		float cx, cy;
		int inside = 0;
		int i0 = indices[t + 0];
		int i1 = indices[t + 1];
		int i2 = indices[t + 2];
		if ( i0 < 0 || i1 < 0 || i2 < 0 || i0 >= numPoints || i1 >= numPoints ||
			i2 >= numPoints ) {
			return 0;
		}
		cx = ( poly2[i0 * 2] + poly2[i1 * 2] + poly2[i2 * 2] ) / 3.0f;
		cy = ( poly2[i0 * 2 + 1] + poly2[i1 * 2 + 1] + poly2[i2 * 2 + 1] ) / 3.0f;
		for ( i = 0; i < numPoints; i++ ) {
			float x1 = poly2[i * 2], y1 = poly2[i * 2 + 1];
			float x2 = poly2[( ( i + 1 ) % numPoints ) * 2];
			float y2 = poly2[( ( i + 1 ) % numPoints ) * 2 + 1];
			if ( ( ( y1 > cy ) != ( y2 > cy ) ) &&
				( cx < ( x2 - x1 ) * ( cy - y1 ) / ( y2 - y1 + 1.0e-30f ) + x1 ) ) {
				inside = !inside;
			}
		}
		if ( !inside ) {
			return 0;
		}
	}
	return 1;
}

int R_Bsp30_TriangulateFace( const float *xyz, int numPoints, int *outIndices,
	int maxIndices ) {
	float poly2[BSP30_TRIANGULATE_MAX_POINTS * 2];
	float area2;
	int n;
	int hub;
	int need;

	if ( !xyz || !outIndices || numPoints < 3 ) {
		return -1;
	}
	need = ( numPoints - 2 ) * 3;
	if ( maxIndices < need ) {
		return -1;
	}
	if ( numPoints == 3 ) {
		outIndices[0] = 0;
		outIndices[1] = 1;
		outIndices[2] = 2;
		return 3;
	}
	if ( numPoints > BSP30_TRIANGULATE_MAX_POINTS ) {
		Bsp30_FanFallback( numPoints, outIndices );
		return need;
	}

	if ( Bsp30_ProjectFace( xyz, numPoints, poly2, &area2 ) ) {
		n = Bsp30_EarClip( poly2, numPoints, area2 >= 0.0f, outIndices, maxIndices );
		if ( n > 0 && R_Bsp30_TriangleCentroidInside( xyz, numPoints, outIndices, n ) ) {
			return n;
		}
		n = Bsp30_EarClip( poly2, numPoints, area2 < 0.0f, outIndices, maxIndices );
		if ( n > 0 && R_Bsp30_TriangleCentroidInside( xyz, numPoints, outIndices, n ) ) {
			return n;
		}
	}

	/* Hub search: any vertex that fans without exterior centroids. */
	for ( hub = 0; hub < numPoints; hub++ ) {
		Bsp30_FanFromHub( numPoints, hub, outIndices );
		if ( R_Bsp30_TriangleCentroidInside( xyz, numPoints, outIndices, need ) ) {
			return need;
		}
	}

	/* Last resort: vertex-0 fan (may still wedge on pathological rings). */
	Bsp30_FanFallback( numPoints, outIndices );
	return need;
}
