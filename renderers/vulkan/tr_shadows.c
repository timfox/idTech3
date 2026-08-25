/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "tr_local.h"

#include <math.h>


/*
=================
R_ShadowClipDist

Trace a ray through the clip BSP (which includes detail brushes) to find
the nearest solid surface, then place the shadow back face just past it.
=================
*/
static float R_ShadowClipDist( const vec3_t start, const vec3_t dir, float maxDist ) {
	trace_t	trace;
	vec3_t	end;

	VectorMA( start, maxDist, dir, end );

	ri.CM_PointTrace( &trace, start, end, CONTENTS_SOLID );

	if ( trace.fraction >= 1.0f ) {
		return maxDist;	// no solid hit, use full distance
	}

	// place back face just past the wall surface
	return trace.fraction * maxDist + r_shadowClipPenetration->value;
}


/*

  for a projection shadow:

  point[x] += light vector * ( z - shadow plane )
  point[y] +=
  point[z] = shadow plane

  1 0 light[x] / light[z]

*/

typedef struct {
	int		i2;
	int		count;	// signed directed-edge accumulator
} edgeDef_t;

#define	MAX_EDGE_DEFS	32

static	edgeDef_t	edgeDefs[SHADER_MAX_VERTEXES][MAX_EDGE_DEFS];
static	int			numEdgeDefs[SHADER_MAX_VERTEXES];
static	int			facing[SHADER_MAX_INDEXES/3];
static	int			numLitTris;
static	int			litTriIndexes[SHADER_MAX_INDEXES];
static	float		clipDists[SHADER_MAX_VERTEXES];
static	qboolean	needsTrace[SHADER_MAX_VERTEXES];
static	int			weld[SHADER_MAX_VERTEXES];
static	float		clipDistsPreB[SHADER_MAX_VERTEXES];	// per-vertex clip distances before Phase B

// Accumulate an undirected edge with a signed count, oriented by lo<hi.
// A manifold edge between two lit tris gets +1 and -1 (opposite windings) ->
// net 0 -> not a silhouette. A boundary/silhouette edge gets one sign only ->
// net +/-1 -> emitted. Non-manifold edges sum to a consistent net value, keeping
// stencil parity balanced instead of leaking.
static void R_AddSilEdge( int a, int b ) {
	int		lo, hi, dir, c, k;

	if ( a == b ) {
		return;		// degenerate after welding
	}
	if ( a < b ) { lo = a; hi = b; dir =  1; }
	else         { lo = b; hi = a; dir = -1; }

	c = numEdgeDefs[ lo ];
	for ( k = 0; k < c; k++ ) {
		if ( edgeDefs[ lo ][ k ].i2 == hi ) {	// existing undirected edge
			edgeDefs[ lo ][ k ].count += dir;
			return;
		}
	}
	if ( c < MAX_EDGE_DEFS ) {
		edgeDefs[ lo ][ c ].i2    = hi;
		edgeDefs[ lo ][ c ].count = dir;
		numEdgeDefs[ lo ]++;
	}
}


static void R_CalcShadowEdges( void ) {
	int		lo, k;

	tess.numIndexes = 0;

	// emit a silhouette quad for every edge with a non-zero signed count.
	// net>0 keeps the lo->hi orientation, net<0 reverses it, so the emitted
	// winding always matches the lit triangle that contributed the edge.
	for ( lo = 0; lo < tess.numVertexes; lo++ ) {
		int c = numEdgeDefs[ lo ];
		for ( k = 0; k < c; k++ ) {
			int hi  = edgeDefs[ lo ][ k ].i2;
			int net = edgeDefs[ lo ][ k ].count;
			int ia, ib, n;

			if ( net == 0 ) {
				continue;
			}
			if ( net > 0 ) { ia = lo; ib = hi; }
			else           { ia = hi; ib = lo; net = -net; }

			for ( n = 0; n < net; n++ ) {		// net is ~always 1
				if ( tess.numIndexes > ARRAY_LEN( tess.indexes ) - 6 ) {
					goto done;
				}
				tess.indexes[ tess.numIndexes + 0 ] = ia;
				tess.indexes[ tess.numIndexes + 1 ] = ib;
				tess.indexes[ tess.numIndexes + 2 ] = ia + tess.numVertexes;
				tess.indexes[ tess.numIndexes + 3 ] = ib;
				tess.indexes[ tess.numIndexes + 4 ] = ib + tess.numVertexes;
				tess.indexes[ tess.numIndexes + 5 ] = ia + tess.numVertexes;
				tess.numIndexes += 6;
			}
		}
	}
done:
	tess.numVertexes *= 2;
	// Shadow pipelines have colorWriteMask = 0, so only position data is needed.
	// Binding 1 (color) and 2 (texcoord) are declared by TYPE_SINGLE_TEXTURE but
	// left unbound — the GPU never reads them.
	;
}


/*
=================
RB_ShadowTessEnd

triangleFromEdge[ v1 ][ v2 ]


  set triangle from edge( v1, v2, tri )
  if ( facing[ triangleFromEdge[ v1 ][ v2 ] ] && !facing[ triangleFromEdge[ v2 ][ v1 ] ) {
  }
=================
*/
void RB_ShadowTessEnd( void ) {
	int		i;
	int		numTris;
	vec3_t	lightDir;
	uint32_t pipeline[2];

	if ( glConfig.stencilBits < 4 ) {
		return;
	}

		VectorCopy( backEnd.currentEntity->lightDir, lightDir );

	// project vertexes away from light direction, clipped to BSP walls
	{
		float	extrusionDist = r_shadowDistance->value;
		vec3_t	worldNegLightDir;

		// decide which triangles face the light (must happen before clip distance
		// computation so we can identify silhouette edge vertices and only trace those)
		Com_Memset( numEdgeDefs, 0, tess.numVertexes * sizeof( numEdgeDefs[0] ) );

		// build positional weld map: weld[i] = canonical vertex index sharing i's
		// position. UV seams split one logical vertex into several index copies;
		// welding by position collapses them so the silhouette forms closed loops.
		// Hashed on tess.xyz before extrusion (only indices < tess.numVertexes).
		{
			#define WELD_EPS    0.05f
			#define WELD_INVEPS ( 1.0f / WELD_EPS )
			// hsize must be a power of two for the open-addressing probe to cover
			// every slot. next_pow2( 2*numVertexes ) < 4*SHADER_MAX_VERTEXES, so the
			// table always fits and no (non-pow2) cap is needed.
			static int htab[ 4 * SHADER_MAX_VERTEXES ];
			int hsize = 1;
			while ( hsize < tess.numVertexes * 2 ) hsize <<= 1;
			Com_Memset( htab, 0xff, hsize * sizeof( htab[0] ) );	// fill with -1

			for ( i = 0; i < tess.numVertexes; i++ ) {
				int kx = (int)floorf( tess.xyz[i][0] * WELD_INVEPS + 0.5f );
				int ky = (int)floorf( tess.xyz[i][1] * WELD_INVEPS + 0.5f );
				int kz = (int)floorf( tess.xyz[i][2] * WELD_INVEPS + 0.5f );
				unsigned h = ( (unsigned)kx * 73856093u ) ^ ( (unsigned)ky * 19349663u )
						   ^ ( (unsigned)kz * 83492791u );
				h &= (unsigned)( hsize - 1 );
				weld[i] = i;
				for ( ; htab[h] != -1; h = ( h + 1 ) & ( hsize - 1 ) ) {
					int j = htab[h];
					if ( (int)floorf( tess.xyz[j][0] * WELD_INVEPS + 0.5f ) == kx &&
						 (int)floorf( tess.xyz[j][1] * WELD_INVEPS + 0.5f ) == ky &&
						 (int)floorf( tess.xyz[j][2] * WELD_INVEPS + 0.5f ) == kz ) {
						weld[i] = weld[j];	// same position -> share canonical id
						break;
					}
				}
				if ( weld[i] == i ) htab[h] = i;	// first occurrence claims the slot
			}
		}

		numTris = tess.numIndexes / 3;
		for ( i = 0 ; i < numTris ; i++ ) {
			int		i1, i2, i3;
			vec3_t	d1, d2, normal;
			float	*v1, *v2, *v3;
			float	d;

			i1 = tess.indexes[ i*3 + 0 ];
			i2 = tess.indexes[ i*3 + 1 ];
			i3 = tess.indexes[ i*3 + 2 ];

			v1 = tess.xyz[ i1 ];
			v2 = tess.xyz[ i2 ];
			v3 = tess.xyz[ i3 ];

			VectorSubtract( v2, v1, d1 );
			VectorSubtract( v3, v1, d2 );
			CrossProduct( d1, d2, normal );

			d = DotProduct( normal, lightDir );
			if ( d > 0 ) {
				facing[ i ] = 1;
			} else {
				facing[ i ] = 0;
			}

			// create the silhouette edges from welded indices, lit tris only
			if ( facing[ i ] ) {
				R_AddSilEdge( weld[i1], weld[i2] );
				R_AddSilEdge( weld[i2], weld[i3] );
				R_AddSilEdge( weld[i3], weld[i1] );
			}
		}

			// save lit-facing triangle indices for back cap
		numLitTris = 0;
		for ( i = 0; i < numTris; i++ ) {
			if ( facing[i] ) {
				litTriIndexes[ numLitTris*3 + 0 ] = tess.indexes[ i*3 + 0 ];
				litTriIndexes[ numLitTris*3 + 1 ] = tess.indexes[ i*3 + 1 ];
				litTriIndexes[ numLitTris*3 + 2 ] = tess.indexes[ i*3 + 2 ];
				numLitTris++;
			}
		}

		if ( r_shadowClip->integer && tr.world ) {
			int j;
			// entity-local lightDir to world space, negated for extrusion
			for ( j = 0; j < 3; j++ )
				worldNegLightDir[j] = -( lightDir[0] * backEnd.or.axis[0][j]
									   + lightDir[1] * backEnd.or.axis[1][j]
									   + lightDir[2] * backEnd.or.axis[2][j] );

			// mark vertices on lit-facing triangles — back-facing vertices
			// don't contribute to silhouette edges or back caps, so skip them
			Com_Memset( needsTrace, 0, tess.numVertexes * sizeof( needsTrace[0] ) );
			for ( i = 0; i < numTris; i++ ) {
				if ( facing[i] ) {
					needsTrace[ tess.indexes[ i*3 + 0 ] ] = qtrue;
					needsTrace[ tess.indexes[ i*3 + 1 ] ] = qtrue;
					needsTrace[ tess.indexes[ i*3 + 2 ] ] = qtrue;
				}
			}
		}

		// Phase A: compute per-vertex clip distances (skip back-facing vertices)
		for ( i = 0; i < tess.numVertexes; i++ ) {
			clipDists[i] = extrusionDist;

			if ( r_shadowClip->integer && tr.world && needsTrace[i] ) {
				int j;
				vec3_t worldPos;
				float clipped;
				for ( j = 0; j < 3; j++ )
					worldPos[j] = tess.xyz[i][0] * backEnd.or.axis[0][j]
								+ tess.xyz[i][1] * backEnd.or.axis[1][j]
								+ tess.xyz[i][2] * backEnd.or.axis[2][j]
								+ backEnd.or.origin[j];

				clipped = R_ShadowClipDist( worldPos, worldNegLightDir, clipDists[i] );
				if ( clipped < clipDists[i] )
					clipDists[i] = clipped;
			}
		}

		// snapshot the traced clip distances so Phase B extends from this fixed base
		Com_Memcpy( clipDistsPreB, clipDists, tess.numVertexes * sizeof( clipDists[0] ) );

		// Phase B: extend each vertex toward its triangle's max clip distance, capped
		// at r_shadowClipExtension past its own traced distance. Reading the snapshot
		// bounds the total extension across the triangles a vertex shares.
		if ( r_shadowClip->integer && tr.world ) {
			float ext = r_shadowClipExtension->value;
			int t, numTrisLocal = tess.numIndexes / 3;
			for ( t = 0; t < numTrisLocal; t++ ) {
				int e;
				int idx[3];
				float maxD;
				idx[0] = tess.indexes[ t*3 + 0 ];
				idx[1] = tess.indexes[ t*3 + 1 ];
				idx[2] = tess.indexes[ t*3 + 2 ];
				maxD = clipDistsPreB[ idx[0] ];
				if ( clipDistsPreB[ idx[1] ] > maxD ) maxD = clipDistsPreB[ idx[1] ];
				if ( clipDistsPreB[ idx[2] ] > maxD ) maxD = clipDistsPreB[ idx[2] ];
				// skip if any vertex didn't hit solid (would pull the face to full distance)
				if ( maxD >= extrusionDist )
					continue;
				for ( e = 0; e < 3; e++ ) {
					int v = idx[e];
					float target = maxD;
					if ( target > clipDistsPreB[v] + ext )
						target = clipDistsPreB[v] + ext;
					if ( target > clipDists[v] )
						clipDists[v] = target;
				}
			}
		}

		// Unify each weld group to one clip distance so the welded silhouette sides
		// and the original-index caps extrude to the same place.
		if ( r_shadowClip->integer && tr.world ) {
			static float gmaxTraced[SHADER_MAX_VERTEXES];
			for ( i = 0; i < tess.numVertexes; i++ )
				gmaxTraced[i] = -1.0f;
			// give each group the farthest clip distance among its hit members, so
			// coincident seam copies extrude together. Members still at full distance
			// (back-facing or missed) are excluded.
			for ( i = 0; i < tess.numVertexes; i++ )
				if ( clipDists[i] < extrusionDist && clipDists[i] > gmaxTraced[ weld[i] ] )
					gmaxTraced[ weld[i] ] = clipDists[i];
			for ( i = 0; i < tess.numVertexes; i++ )
				if ( gmaxTraced[ weld[i] ] >= 0.0f )
					clipDists[i] = gmaxTraced[ weld[i] ];
		}

		// Phase C: extrude vertices using final clip distances
		for ( i = 0; i < tess.numVertexes; i++ ) {
			VectorMA( tess.xyz[i], -clipDists[i], lightDir, tess.xyz[i+tess.numVertexes] );
		}
	}

	R_CalcShadowEdges();

	// back cap: lit-facing tris at extruded positions (original winding for VK Y-flip)
	{
		int nvOrig = tess.numVertexes / 2;
		for ( i = 0; i < numLitTris; i++ ) {
			if ( tess.numIndexes > ARRAY_LEN( tess.indexes ) - 3 )
				break;
			tess.indexes[ tess.numIndexes + 0 ] = litTriIndexes[ i*3 + 0 ] + nvOrig;
			tess.indexes[ tess.numIndexes + 1 ] = litTriIndexes[ i*3 + 1 ] + nvOrig;
			tess.indexes[ tess.numIndexes + 2 ] = litTriIndexes[ i*3 + 2 ] + nvOrig;
			tess.numIndexes += 3;
		}
	}

	// near cap: lit-facing tris at original positions, wound opposite the far
	// cap, closing the volume on the light side as z-fail requires.
	{
		for ( i = 0; i < numLitTris; i++ ) {
			if ( tess.numIndexes > ARRAY_LEN( tess.indexes ) - 3 )
				break;
			tess.indexes[ tess.numIndexes + 0 ] = litTriIndexes[ i*3 + 0 ];
			tess.indexes[ tess.numIndexes + 1 ] = litTriIndexes[ i*3 + 2 ];
			tess.indexes[ tess.numIndexes + 2 ] = litTriIndexes[ i*3 + 1 ];
			tess.numIndexes += 3;
		}
	}

	// draw the silhouette edges
	GL_Bind( tr.whiteImage );

	// mirrors have the culling order reversed
	if ( backEnd.viewParms.portalView == PV_MIRROR ) {
		pipeline[0] = vk.shadow_volume_pipelines[0][1];
		pipeline[1] = vk.shadow_volume_pipelines[1][1];
	} else {
		pipeline[0] = vk.shadow_volume_pipelines[0][0];
		pipeline[1] = vk.shadow_volume_pipelines[1][0];

	}
	vk_bind_pipeline( pipeline[0] ); // back-sided
	vk_bind_index();
	vk_bind_geometry( TESS_XYZ );
	vk_draw_geometry( DEPTH_RANGE_NORMAL, qtrue );
	vk_bind_pipeline( pipeline[1] ); // front-sided
	vk_draw_geometry( DEPTH_RANGE_NORMAL, qtrue );

	tess.numVertexes /= 2;

	backEnd.doneShadows = qtrue;

	tess.numIndexes = 0;
}


/*
=================
RB_ShadowFinish

Darken everything that is is a shadow volume.
We have to delay this until everything has been shadowed,
because otherwise shadows from different body parts would
overlap and double darken.
=================
*/
void RB_ShadowFinish( void ) {
	float tmp[16];
	int i;
	static const vec3_t verts[4] = {
		{ -100, 100, -10 },
		{  100, 100, -10 },
		{ -100,-100, -10 },
		{  100,-100, -10 }
	};

	if ( !backEnd.doneShadows ) {
		return;
	}

	backEnd.doneShadows = qfalse;

	if ( r_shadows->integer != 2 ) {
		return;
	}
	if ( glConfig.stencilBits < 4 ) {
		return;
	}

	GL_Bind( tr.whiteImage );

	for ( i = 0; i < 4; i++ )
	{
		VectorCopy( verts[i], tess.xyz[i] );
		Vector4Set( tess.svars.colors[0][i].rgba, 153, 153, 153, 255 );
	}

	tess.numVertexes = 4;

	Com_Memcpy( tmp, vk_world.modelview_transform, 64 );
	Com_Memset( vk_world.modelview_transform, 0, 64 );

	vk_world.modelview_transform[0] = 1.0f;
	vk_world.modelview_transform[5] = 1.0f;
	vk_world.modelview_transform[10] = 1.0f;
	vk_world.modelview_transform[15] = 1.0f;

	vk_bind_pipeline( vk.shadow_finish_pipeline );

	vk_update_mvp( NULL );

	vk_bind_geometry( TESS_XYZ | TESS_RGBA0 /*| TESS_ST0 */ );
	vk_draw_geometry( DEPTH_RANGE_NORMAL, qfalse );

	Com_Memcpy( vk_world.modelview_transform, tmp, 64 );

	tess.numIndexes = 0;
	tess.numVertexes = 0;

}


/*
=================
RB_ProjectionShadowDeform

=================
*/
void RB_ProjectionShadowDeform( void ) {
	float	*xyz;
	int		i;
	float	h;
	vec3_t	ground;
	vec3_t	light;
	float	groundDist;
	float	d;
	vec3_t	lightDir;

	xyz = ( float * ) tess.xyz;

	ground[0] = backEnd.or.axis[0][2];
	ground[1] = backEnd.or.axis[1][2];
	ground[2] = backEnd.or.axis[2][2];

	groundDist = backEnd.or.origin[2] - backEnd.currentEntity->e.shadowPlane;

		VectorCopy( backEnd.currentEntity->lightDir, lightDir );

	d = DotProduct( lightDir, ground );
	// don't let the shadows get too long or go negative
	if ( d < 0.5 ) {
		VectorMA( lightDir, (0.5 - d), ground, lightDir );
		d = DotProduct( lightDir, ground );
	}
	d = 1.0 / d;

	light[0] = lightDir[0] * d;
	light[1] = lightDir[1] * d;
	light[2] = lightDir[2] * d;

	for ( i = 0; i < tess.numVertexes; i++, xyz += 4 ) {
		h = DotProduct( xyz, ground ) + groundDist;

		xyz[0] -= light[0] * h;
		xyz[1] -= light[1] * h;
		xyz[2] -= light[2] * h;
	}
}
