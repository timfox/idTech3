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
// tr_surf.c
#include "tr_local.h"
#include "tr_model_gltf.h"
#include "tr_gltf_topo.h"
#include "vk_draw_state.h"
#include <math.h>

#ifdef USE_VBO
/*
===============
RB_QueueSurfaceVBO
===============
*/
static qboolean RB_QueueSurfaceVBO( int vboItemIndex, surfaceType_t surfType )
{
	if ( !tess.allowVBO || !vboItemIndex ) {
		return qfalse;
	}

	if ( VBO_ItemIsStream( vboItemIndex ) ) {
		const stream_vbo_item_t *item = VBO_StreamGetItem( VBO_ItemStreamIndex( vboItemIndex ) );

		if ( !item ) {
			return qfalse;
		}
		if ( tess.vboIndex == 0 ) {
			RB_EndSurface();
			RB_BeginSurface( tess.shader, tess.fogNum );
			tess.numIndexes = 1;
			tess.numVertexes = 0;
		}
		tess.surfType = surfType;
		tess.vboIndex = vboItemIndex;
		tess.vboStreamItem = item;
		return qtrue;
	}

	if ( tess.vboIndex == 0 ) {
		RB_EndSurface();
		RB_BeginSurface( tess.shader, tess.fogNum );
		tess.numIndexes = 1;
		tess.numVertexes = 0;
		VBO_ClearQueue();
	}
	tess.surfType = surfType;
	tess.vboIndex = vboItemIndex;
	tess.vboStreamItem = NULL;
	VBO_QueueItem( vboItemIndex );
	return qtrue;
}
#endif

/*

  THIS ENTIRE FILE IS BACK END

backEnd.currentEntity will be valid.

Tess_Begin has already been called for the surface's shader.

The modelview matrix will be set.

It is safe to actually issue drawing commands here if you don't want to
use the shader system.
*/


//============================================================================


/*
==============
RB_CheckOverflow
==============
*/
void RB_CheckOverflow( int verts, int indexes ) {
	if (tess.numVertexes + verts < SHADER_MAX_VERTEXES
		&& tess.numIndexes + indexes < SHADER_MAX_INDEXES) {
		return;
	}

	RB_EndSurface();

	if ( verts >= SHADER_MAX_VERTEXES ) {
		ri.Error( ERR_DROP, "RB_CheckOverflow: verts > MAX (%d > %d)", verts, SHADER_MAX_VERTEXES );
	}

	if ( indexes >= SHADER_MAX_INDEXES ) {
		ri.Error( ERR_DROP, "RB_CheckOverflow: indices > MAX (%d > %d)", indexes, SHADER_MAX_INDEXES );
	}

	RB_BeginSurface( tess.shader, tess.fogNum );
}


/*
==============
RB_AddQuadStampExt
==============
*/
void RB_AddQuadStampExt( const vec3_t origin, const vec3_t left, const vec3_t up, color4ub_t color, float s1, float t1, float s2, float t2 ) {
	vec3_t		normal;
	int			ndx;

#ifdef USE_VBO
	VBO_Flush();
#endif

	RB_CHECKOVERFLOW( 4, 6 );

#ifdef USE_VBO
	tess.surfType = SF_TRIANGLES;
#endif

	ndx = tess.numVertexes;

	// triangle indexes for a simple quad
	tess.indexes[ tess.numIndexes + 0 ] = ndx + 0;
	tess.indexes[ tess.numIndexes + 1 ] = ndx + 1;
	tess.indexes[ tess.numIndexes + 2 ] = ndx + 3;

	tess.indexes[ tess.numIndexes + 3 ] = ndx + 3;
	tess.indexes[ tess.numIndexes + 4 ] = ndx + 1;
	tess.indexes[ tess.numIndexes + 5 ] = ndx + 2;

	tess.xyz[ndx][0] = origin[0] + left[0] + up[0];
	tess.xyz[ndx][1] = origin[1] + left[1] + up[1];
	tess.xyz[ndx][2] = origin[2] + left[2] + up[2];

	tess.xyz[ndx+1][0] = origin[0] - left[0] + up[0];
	tess.xyz[ndx+1][1] = origin[1] - left[1] + up[1];
	tess.xyz[ndx+1][2] = origin[2] - left[2] + up[2];

	tess.xyz[ndx+2][0] = origin[0] - left[0] - up[0];
	tess.xyz[ndx+2][1] = origin[1] - left[1] - up[1];
	tess.xyz[ndx+2][2] = origin[2] - left[2] - up[2];

	tess.xyz[ndx+3][0] = origin[0] + left[0] - up[0];
	tess.xyz[ndx+3][1] = origin[1] + left[1] - up[1];
	tess.xyz[ndx+3][2] = origin[2] + left[2] - up[2];

	// constant normal all the way around
	VectorSubtract( vec3_origin, backEnd.viewParms.or.axis[0], normal );

	tess.normal[ndx][0] = tess.normal[ndx+1][0] = tess.normal[ndx+2][0] = tess.normal[ndx+3][0] = normal[0];
	tess.normal[ndx][1] = tess.normal[ndx+1][1] = tess.normal[ndx+2][1] = tess.normal[ndx+3][1] = normal[1];
	tess.normal[ndx][2] = tess.normal[ndx+1][2] = tess.normal[ndx+2][2] = tess.normal[ndx+3][2] = normal[2];
	
	// standard square texture coordinates
	tess.texCoords[0][ndx+0][0] = tess.texCoords[1][ndx+0][0] = s1;
	tess.texCoords[0][ndx+0][1] = tess.texCoords[1][ndx+0][1] = t1;

	tess.texCoords[0][ndx+1][0] = tess.texCoords[1][ndx+1][0] = s2;
	tess.texCoords[0][ndx+1][1] = tess.texCoords[1][ndx+1][1] = t1;

	tess.texCoords[0][ndx+2][0] = tess.texCoords[1][ndx+2][0] = s2;
	tess.texCoords[0][ndx+2][1] = tess.texCoords[1][ndx+2][1] = t2;

	tess.texCoords[0][ndx+3][0] = tess.texCoords[1][ndx+3][0] = s1;
	tess.texCoords[0][ndx+3][1] = tess.texCoords[1][ndx+3][1] = t2;

	// constant color all the way around
	// should this be identity and let the shader specify from entity?
	tess.vertexColors[ndx + 0] =
	tess.vertexColors[ndx + 1] =
	tess.vertexColors[ndx + 2] =
	tess.vertexColors[ndx + 3] = color;

	tess.numVertexes += 4;
	tess.numIndexes += 6;
}


void RB_AddQuadStamp2( float x, float y, float w, float h, float s1, float t1, float s2, float t2, color4ub_t color ) {
	int			numIndexes;
	int			numVerts;

#ifdef USE_VBO
	VBO_Flush();
#endif

	RB_CHECKOVERFLOW( 4, 6 );

#ifdef USE_VBO
	tess.surfType = SF_TRIANGLES;
#endif

	numIndexes = tess.numIndexes;
	numVerts = tess.numVertexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	tess.indexes[numIndexes + 0] = numVerts + 3;
	tess.indexes[numIndexes + 1] = numVerts + 0;
	tess.indexes[numIndexes + 2] = numVerts + 2;
	tess.indexes[numIndexes + 3] = numVerts + 2;
	tess.indexes[numIndexes + 4] = numVerts + 0;
	tess.indexes[numIndexes + 5] = numVerts + 1;

	tess.vertexColors[numVerts + 0] =
	tess.vertexColors[numVerts + 1] =
	tess.vertexColors[numVerts + 2] =
	tess.vertexColors[numVerts + 3] = color;

	tess.xyz[numVerts + 0][0] = x;
	tess.xyz[numVerts + 0][1] = y;
	tess.xyz[numVerts + 0][2] = 0;

	tess.xyz[numVerts + 1][0] = x + w;
	tess.xyz[numVerts + 1][1] = y;
	tess.xyz[numVerts + 1][2] = 0;

	tess.xyz[numVerts + 2][0] = x + w;
	tess.xyz[numVerts + 2][1] = y + h;
	tess.xyz[numVerts + 2][2] = 0;

	tess.xyz[numVerts + 3][0] = x;
	tess.xyz[numVerts + 3][1] = y + h;
	tess.xyz[numVerts + 3][2] = 0;

	tess.texCoords[0][numVerts + 0][0] = s1;
	tess.texCoords[0][numVerts + 0][1] = t1;
	tess.texCoords[0][numVerts + 1][0] = s2;
	tess.texCoords[0][numVerts + 1][1] = t1;
	tess.texCoords[0][numVerts + 2][0] = s2;
	tess.texCoords[0][numVerts + 2][1] = t2;
	tess.texCoords[0][numVerts + 3][0] = s1;
	tess.texCoords[0][numVerts + 3][1] = t2;
}


/*
==============
RB_AddQuadStamp
==============
*/
void RB_AddQuadStamp( const vec3_t origin, const vec3_t left, const vec3_t up, color4ub_t color ) {
	RB_AddQuadStampExt( origin, left, up, color, 0, 0, 1, 1 );
}


/*
==============
RB_SurfaceSprite
==============
*/
static void RB_SurfaceSprite( void ) {
	vec3_t		left, up;
	float		radius;
	float		s1, t1, s2, t2;
	qboolean	flipbookUV;

	flipbookUV = ( backEnd.currentEntity->e.renderfx & RF_SPRITE_FLIPBOOK ) ? qtrue : qfalse;

	// calculate the xyz locations for the four corners
	radius = backEnd.currentEntity->e.radius;
	if ( backEnd.currentEntity->e.renderfx & RF_SPRITE_YAWLOCK ) {
		vec3_t dir;
		static const vec3_t worldUp = { 0.0f, 0.0f, 1.0f };

		VectorSubtract( backEnd.viewParms.or.origin, backEnd.currentEntity->e.origin, dir );
		dir[2] = 0.0f;
		if ( VectorNormalize2( dir, dir ) > 0.0f ) {
			CrossProduct( dir, worldUp, left );
			VectorScale( left, radius, left );
			VectorScale( worldUp, radius, up );
		} else {
			VectorScale( backEnd.viewParms.or.axis[1], radius, left );
			VectorScale( backEnd.viewParms.or.axis[2], radius, up );
		}
	} else if ( backEnd.currentEntity->e.rotation == 0.0 ) {
		VectorScale( backEnd.viewParms.or.axis[1], radius, left );
		VectorScale( backEnd.viewParms.or.axis[2], radius, up );
	} else {
		float	s, c;
		float	ang;
		
		ang = M_PI * backEnd.currentEntity->e.rotation / 180.0;
		s = sin( ang );
		c = cos( ang );

		VectorScale( backEnd.viewParms.or.axis[1], c * radius, left );
		VectorMA( left, -s * radius, backEnd.viewParms.or.axis[2], left );

		VectorScale( backEnd.viewParms.or.axis[2], c * radius, up );
		VectorMA( up, s * radius, backEnd.viewParms.or.axis[1], up );
	}

	if ( backEnd.viewParms.portalView == PV_MIRROR ) {
		VectorSubtract( vec3_origin, left, left );
	}

	if ( flipbookUV ) {
		int cols = backEnd.currentEntity->e.oldframe;
		int rows = backEnd.currentEntity->e.skinNum;
		int frame = backEnd.currentEntity->e.frame;
		int col, row;

		if ( cols < 1 ) {
			cols = 1;
		}
		if ( rows < 1 ) {
			rows = 1;
		}
		col = frame % cols;
		row = ( frame / cols ) % rows;
		s1 = (float)col / (float)cols;
		t1 = (float)row / (float)rows;
		s2 = (float)( col + 1 ) / (float)cols;
		t2 = (float)( row + 1 ) / (float)rows;
		RB_AddQuadStampExt( backEnd.currentEntity->e.origin, left, up,
			backEnd.currentEntity->e.shader, s1, t1, s2, t2 );
	} else {
		RB_AddQuadStamp( backEnd.currentEntity->e.origin, left, up, backEnd.currentEntity->e.shader );
	}
}


/*
=============
RB_SurfacePolychain
=============
*/
static void RB_SurfacePolychain( const srfPoly_t *p ) {
	int		i;
	int		numv;

#ifdef USE_VBO
	VBO_Flush();
#endif

	RB_CHECKOVERFLOW( p->numVerts, 3*(p->numVerts - 2) );

#ifdef USE_VBO
	tess.surfType = SF_POLY;
#endif

	// fan triangles into the tess array
	numv = tess.numVertexes;
	for ( i = 0; i < p->numVerts; i++ ) {
		VectorCopy( p->verts[i].xyz, tess.xyz[numv] );
		tess.texCoords[0][numv][0] = p->verts[i].st[0];
		tess.texCoords[0][numv][1] = p->verts[i].st[1];
		tess.vertexColors[numv] = p->verts[ i ].modulate;

		numv++;
	}

	// generate fan indexes into the tess array
	for ( i = 0; i < p->numVerts-2; i++ ) {
		tess.indexes[tess.numIndexes + 0] = tess.numVertexes;
		tess.indexes[tess.numIndexes + 1] = tess.numVertexes + i + 1;
		tess.indexes[tess.numIndexes + 2] = tess.numVertexes + i + 2;
		tess.numIndexes += 3;
	}

	tess.numVertexes = numv;
}


/*
=============
RB_SurfaceTriangles
=============
*/
static void RB_SurfaceTriangles( const srfTriangles_t *srf ) {
	int			i;
	const srfVert_t	*dv;
	float		*xyz, *normal;
	float				*qtangent;
	float				*lightdir;
	float		*texCoords0;
	float		*texCoords1;
	uint32_t	*color;
	int			dlightBits;

#ifdef USE_VBO
	if ( tess.allowVBO && srf->vboItemIndex && !srf->dlightBits &&
		RB_QueueSurfaceVBO( srf->vboItemIndex, SF_TRIANGLES ) ) {
		return;
	}

	VBO_Flush();
#endif // USE_VBO

	RB_CHECKOVERFLOW( srf->numVerts, srf->numIndexes );

	dlightBits = srf->dlightBits;
	tess.dlightBits |= dlightBits;

#ifdef USE_VBO
	tess.surfType = SF_TRIANGLES;
#endif

	for ( i = 0 ; i < srf->numIndexes ; i += 3 ) {
		tess.indexes[ tess.numIndexes + i + 0 ] = tess.numVertexes + srf->indexes[ i + 0 ];
		tess.indexes[ tess.numIndexes + i + 1 ] = tess.numVertexes + srf->indexes[ i + 1 ];
		tess.indexes[ tess.numIndexes + i + 2 ] = tess.numVertexes + srf->indexes[ i + 2 ];
	}
	tess.numIndexes += srf->numIndexes;

	dv = srf->verts;
	xyz = tess.xyz[ tess.numVertexes ];
	normal = tess.normal[ tess.numVertexes ];
	qtangent = tess.qtangent[ tess.numVertexes ];
	lightdir = tess.lightdir[ tess.numVertexes ];
	texCoords0 = tess.texCoords[0][ tess.numVertexes ];
	texCoords1 = tess.texCoords[1][ tess.numVertexes ];
	color = &tess.vertexColors[ tess.numVertexes ].u32;

	for ( i = 0; i < srf->numVerts; i++, dv++, xyz += 4, normal += 4, texCoords0 += 2, color++ ) {
		xyz[0] = dv->xyz[0];
		xyz[1] = dv->xyz[1];
		xyz[2] = dv->xyz[2];

		{
			normal[0] = dv->normal[0];
			normal[1] = dv->normal[1];
			normal[2] = dv->normal[2];
		}

		if( vk.pbrActive ) {
			qtangent[0] = dv->qtangent[0];
			qtangent[1] = dv->qtangent[1];
			qtangent[2] = dv->qtangent[2];
			qtangent[3] = dv->qtangent[3];
			qtangent += 4;

			lightdir[0] = dv->lightdir[0];
			lightdir[1] = dv->lightdir[1];
			lightdir[2] = dv->lightdir[2];
			lightdir[3] = 0.0;
			lightdir += 4;
		}

		texCoords0[0] = dv->st[0];
		texCoords0[1] = dv->st[1];

		{
			texCoords1[0] = dv->lightmap[0];
			texCoords1[1] = dv->lightmap[1];
			texCoords1 += 2;
		}

		*color = dv->color.u32;
	}
	for ( i = 0 ; i < srf->numVerts ; i++ ) {
		tess.vertexDlightBits[ tess.numVertexes + i] = dlightBits;
	}
	tess.numVertexes += srf->numVerts;
}


/*
==============
RB_SurfaceBeam
==============
*/
static void RB_SurfaceBeam( void )
{
#define NUM_BEAM_SEGS 6
	const refEntity_t *e;
	int	i;
	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t	points[NUM_BEAM_SEGS+1][2];
	vec3_t oldorigin, origin;

	e = &backEnd.currentEntity->e;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );

	VectorScale( perpvec, 4, perpvec );

	for ( i = 0; i <= NUM_BEAM_SEGS; i++ )
	{
		RotatePointAroundVector( points[i][0], normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i );
		VectorAdd( points[i][0], direction, points[i][1] );
	}

#ifdef USE_VULKAN
	tess.numIndexes = 0;
	tess.numVertexes = 0;

	GL_Bind( tr.whiteImage );

	for ( i = 0; i < (NUM_BEAM_SEGS+1)*2; i++ ) {
		Vector4Set( tess.svars.colors[0][i].rgba, 255, 0, 0, 255 );
	}

	for ( i = 0; i <= NUM_BEAM_SEGS; i++ ) {
		VectorCopy( points[i][0], tess.xyz[ i * 2 + 0 ] );
		VectorCopy( points[i][1], tess.xyz[ i * 2 + 1 ] );
	}
	tess.numVertexes = (NUM_BEAM_SEGS + 1) * 2;

	vk_bind_pipeline( vk.surface_beam_pipeline );
	vk_bind_geometry( TESS_XYZ | TESS_RGBA0 );
	vk_draw_geometry( DEPTH_RANGE_NORMAL, qfalse );

	tess.numIndexes = 0;
	tess.numVertexes = 0;
#else
	qglDisable( GL_TEXTURE_2D );

	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE );

	qglColor4f( 1, 0, 0, 1 );

	GL_ClientState( 0, CLS_NONE );

	qglVertexPointer( 3, GL_FLOAT, 0, &points[0][0] );
	qglDrawArrays( GL_TRIANGLE_STRIP, 0, (NUM_BEAM_SEGS+1)*2 );

	qglEnable( GL_TEXTURE_2D );
#endif
}

//================================================================================

static void DoRailCore( const vec3_t start, const vec3_t end, const vec3_t up, float len, float spanWidth )
{
	float		spanWidth2;
	int			vbase;
	float		t = len / 256.0f;

	RB_CHECKOVERFLOW( 4, 6 );

	vbase = tess.numVertexes;

	spanWidth2 = -spanWidth;

	/* Could use quad stamp for efficiency. */
	VectorMA( start, spanWidth, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[0][tess.numVertexes][0] = 0;
	tess.texCoords[0][tess.numVertexes][1] = 0;
	tess.vertexColors[tess.numVertexes].rgba[0] = backEnd.currentEntity->e.shader.rgba[0] * 0.25;
	tess.vertexColors[tess.numVertexes].rgba[1] = backEnd.currentEntity->e.shader.rgba[1] * 0.25;
	tess.vertexColors[tess.numVertexes].rgba[2] = backEnd.currentEntity->e.shader.rgba[2] * 0.25;
	tess.numVertexes++;

	VectorMA( start, spanWidth2, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[0][tess.numVertexes][0] = 0;
	tess.texCoords[0][tess.numVertexes][1] = 1;
	tess.vertexColors[tess.numVertexes].rgba[0] = backEnd.currentEntity->e.shader.rgba[0];
	tess.vertexColors[tess.numVertexes].rgba[1] = backEnd.currentEntity->e.shader.rgba[1];
	tess.vertexColors[tess.numVertexes].rgba[2] = backEnd.currentEntity->e.shader.rgba[2];
	tess.numVertexes++;

	VectorMA( end, spanWidth, up, tess.xyz[tess.numVertexes] );

	tess.texCoords[0][tess.numVertexes][0] = t;
	tess.texCoords[0][tess.numVertexes][1] = 0;
	tess.vertexColors[tess.numVertexes].rgba[0] = backEnd.currentEntity->e.shader.rgba[0];
	tess.vertexColors[tess.numVertexes].rgba[1] = backEnd.currentEntity->e.shader.rgba[1];
	tess.vertexColors[tess.numVertexes].rgba[2] = backEnd.currentEntity->e.shader.rgba[2];
	tess.numVertexes++;

	VectorMA( end, spanWidth2, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[0][tess.numVertexes][0] = t;
	tess.texCoords[0][tess.numVertexes][1] = 1;
	tess.vertexColors[tess.numVertexes].rgba[0] = backEnd.currentEntity->e.shader.rgba[0];
	tess.vertexColors[tess.numVertexes].rgba[1] = backEnd.currentEntity->e.shader.rgba[1];
	tess.vertexColors[tess.numVertexes].rgba[2] = backEnd.currentEntity->e.shader.rgba[2];
	tess.numVertexes++;

	tess.indexes[tess.numIndexes++] = vbase;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 2;

	tess.indexes[tess.numIndexes++] = vbase + 2;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 3;
}


static void DoRailDiscs( int numSegs, const vec3_t start, const vec3_t dir, const vec3_t right, const vec3_t up )
{
	int i;
	vec3_t	pos[4];
	vec3_t	v;
	int		spanWidth = r_railWidth->integer;
	float c, s;
	float		scale;

	if ( numSegs > 1 )
		numSegs--;
	if ( !numSegs )
		return;

	scale = 0.25;

	for ( i = 0; i < 4; i++ )
	{
		c = cos( DEG2RAD( 45 + i * 90 ) );
		s = sin( DEG2RAD( 45 + i * 90 ) );
		v[0] = ( right[0] * c + up[0] * s ) * scale * spanWidth;
		v[1] = ( right[1] * c + up[1] * s ) * scale * spanWidth;
		v[2] = ( right[2] * c + up[2] * s ) * scale * spanWidth;
		VectorAdd( start, v, pos[i] );

		if ( numSegs > 1 )
		{
			// offset by 1 segment if we're doing a long distance shot
			VectorAdd( pos[i], dir, pos[i] );
		}
	}

	for ( i = 0; i < numSegs; i++ )
	{
		int j;

		RB_CHECKOVERFLOW( 4, 6 );

		for ( j = 0; j < 4; j++ )
		{
			VectorCopy( pos[j], tess.xyz[tess.numVertexes] );
			tess.texCoords[0][tess.numVertexes][0] = ( j < 2 );
			tess.texCoords[0][tess.numVertexes][1] = ( j && j != 3 );
			tess.vertexColors[tess.numVertexes].rgba[0] = backEnd.currentEntity->e.shader.rgba[0];
			tess.vertexColors[tess.numVertexes].rgba[1] = backEnd.currentEntity->e.shader.rgba[1];
			tess.vertexColors[tess.numVertexes].rgba[2] = backEnd.currentEntity->e.shader.rgba[2];
			tess.numVertexes++;

			VectorAdd( pos[j], dir, pos[j] );
		}

		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 0;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 1;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 3;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 3;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 1;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 2;
	}
}


/*
** RB_SurfaceRailRinges
*/
static void RB_SurfaceRailRings( void ) {
	const refEntity_t *e;
	int			numSegs;
	int			len;
	vec3_t		vec;
	vec3_t		right, up;
	vec3_t		start, end;

	e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, start );
	VectorCopy( e->origin, end );

	// compute variables
	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );
	MakeNormalVectors( vec, right, up );
	numSegs = ( len ) / r_railSegmentLength->value;
	if ( numSegs <= 0 ) {
		numSegs = 1;
	}

	VectorScale( vec, r_railSegmentLength->value, vec );

	DoRailDiscs( numSegs, start, vec, right, up );
}


/*
** RB_SurfaceRailCore
*/
static void RB_SurfaceRailCore( void ) {
	const refEntity_t *e;
	int			len;
	vec3_t		right;
	vec3_t		vec;
	vec3_t		start, end;
	vec3_t		v1, v2;

	e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, start );
	VectorCopy( e->origin, end );

	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );

	// compute side vector
	VectorSubtract( start, backEnd.viewParms.or.origin, v1 );
	VectorNormalize( v1 );
	VectorSubtract( end, backEnd.viewParms.or.origin, v2 );
	VectorNormalize( v2 );
	CrossProduct( v1, v2, right );
	VectorNormalize( right );

	DoRailCore( start, end, right, len, r_railCoreWidth->integer );
}


/*
** RB_SurfaceLightningBolt
*/
static void RB_SurfaceLightningBolt( void ) {
	const refEntity_t *e;
	int			len;
	vec3_t		right;
	vec3_t		vec;
	vec3_t		start, end;
	vec3_t		v1, v2;
	int			i;

	e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, end );
	VectorCopy( e->origin, start );

	// compute variables
	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );

	// compute side vector
	VectorSubtract( start, backEnd.viewParms.or.origin, v1 );
	VectorNormalize( v1 );
	VectorSubtract( end, backEnd.viewParms.or.origin, v2 );
	VectorNormalize( v2 );
	CrossProduct( v1, v2, right );
	VectorNormalize( right );

	for ( i = 0 ; i < 4 ; i++ ) {
		vec3_t	temp;

		DoRailCore( start, end, right, len, 8 );
		RotatePointAroundVector( temp, vec, right, 45 );
		VectorCopy( temp, right );
	}
}


/*
** VectorArrayNormalize
*
* The inputs to this routing seem to always be close to length = 1.0 (about 0.6 to 2.0)
* This means that we don't have to worry about zero length or enormously long vectors.
*/
static void VectorArrayNormalize(vec4_t *normals, unsigned int count)
{
//    assert(count);
	// given the input, it's safe to call VectorNormalizeFast
    while ( count-- ) {
        VectorNormalizeFast(normals[0]);
        normals++;
    }
}


/*
** LerpMeshVertexes
*/
static void LerpMeshVertexes_scalar(md3Surface_t *surf, float backlerp)
{
	short	*oldXyz, *newXyz, *oldNormals, *newNormals;
	float	*outXyz, *outNormal;
	float	oldXyzScale, newXyzScale;
	float	oldNormalScale, newNormalScale;
	int		vertNum;
	unsigned lat, lng;
	int		numVerts;

	outXyz = tess.xyz[tess.numVertexes];
	outNormal = tess.normal[tess.numVertexes];

	newXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
		+ (backEnd.currentEntity->e.frame * surf->numVerts * 4);
	newNormals = newXyz + 3;

	newXyzScale = MD3_XYZ_SCALE * (1.0 - backlerp);
	newNormalScale = 1.0 - backlerp;

	numVerts = surf->numVerts;

	if ( backlerp == 0 ) {
		//
		// just copy the vertexes
		//
		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			newXyz += 4, newNormals += 4,
			outXyz += 4, outNormal += 4) 
		{

			outXyz[0] = newXyz[0] * newXyzScale;
			outXyz[1] = newXyz[1] * newXyzScale;
			outXyz[2] = newXyz[2] * newXyzScale;

			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= (FUNCTABLE_SIZE/256);
			lng *= (FUNCTABLE_SIZE/256);

			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )

			outNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			outNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			outNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];
		}
	} else {
		//
		// interpolate and copy the vertex and normal
		//
		oldXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
			+ (backEnd.currentEntity->e.oldframe * surf->numVerts * 4);
		oldNormals = oldXyz + 3;

		oldXyzScale = MD3_XYZ_SCALE * backlerp;
		oldNormalScale = backlerp;

		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			oldXyz += 4, newXyz += 4, oldNormals += 4, newNormals += 4,
			outXyz += 4, outNormal += 4) 
		{
			vec3_t uncompressedOldNormal, uncompressedNewNormal;

			// interpolate the xyz
			outXyz[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
			outXyz[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
			outXyz[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

			/* Could interpolate lat/long instead. */
			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;
			uncompressedNewNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedNewNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedNewNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			lat = ( oldNormals[0] >> 8 ) & 0xff;
			lng = ( oldNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;

			uncompressedOldNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedOldNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedOldNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			outNormal[0] = uncompressedOldNormal[0] * oldNormalScale + uncompressedNewNormal[0] * newNormalScale;
			outNormal[1] = uncompressedOldNormal[1] * oldNormalScale + uncompressedNewNormal[1] * newNormalScale;
			outNormal[2] = uncompressedOldNormal[2] * oldNormalScale + uncompressedNewNormal[2] * newNormalScale;

//			VectorNormalize (outNormal);
		}
    	VectorArrayNormalize((vec4_t *)tess.normal[tess.numVertexes], numVerts);
   	}
}


static void LerpMeshVertexes(md3Surface_t *surf, float backlerp)
{
	LerpMeshVertexes_scalar( surf, backlerp );
}


/*
=============
RB_SurfaceMesh
=============
*/
static void RB_SurfaceMesh(md3Surface_t *surface) {
	int				j;
	float			backlerp;
	int				*triangles;
	float			*texCoords;
	int				indexes;
	int				Bob, Doug;
	int				numVerts;

#ifdef USE_VBO
	VBO_Flush();
#endif

	RB_CHECKOVERFLOW( surface->numVerts, surface->numTriangles * 3 );

#ifdef USE_VBO
	tess.surfType = SF_MD3;
#endif

	if (  backEnd.currentEntity->e.oldframe == backEnd.currentEntity->e.frame ) {
		backlerp = 0;
	} else  {
		backlerp = backEnd.currentEntity->e.backlerp;
	}

	LerpMeshVertexes (surface, backlerp);

	triangles = (int *) ((byte *)surface + surface->ofsTriangles);
	indexes = surface->numTriangles * 3;
	Bob = tess.numIndexes;
	Doug = tess.numVertexes;
	for (j = 0 ; j < indexes ; j++) {
		tess.indexes[Bob + j] = Doug + triangles[j];
	}
	tess.numIndexes += indexes;

	texCoords = (float *) ((byte *)surface + surface->ofsSt);

	numVerts = surface->numVerts;
	for ( j = 0; j < numVerts; j++ ) {
		tess.texCoords[0][Doug + j][0] = texCoords[j*2+0];
		tess.texCoords[0][Doug + j][1] = texCoords[j*2+1];
		/* lightmapST could be filled for completeness. */
	}

	tess.numVertexes += surface->numVerts;

}


/*
==============
RB_SurfaceFace
==============
*/
static void RB_SurfaceFace( const srfSurfaceFace_t *surf ) {
	int			i;
	const unsigned	*indices;
	glIndex_t	*tessIndexes;
	const float	*v;
	const float	*normal;
	int			ndx;
	int			Bob;
	int			numPoints;
	int			dlightBits;

#ifdef USE_VBO
	if ( tess.allowVBO && surf->vboItemIndex && !surf->dlightBits &&
		RB_QueueSurfaceVBO( surf->vboItemIndex, SF_FACE ) ) {
		return;
	}

	VBO_Flush();
#endif // USE_VBO

	RB_CHECKOVERFLOW( surf->numPoints, surf->numIndices );

#ifdef USE_VBO
	tess.surfType = SF_FACE;
#endif

	dlightBits = surf->dlightBits;
	tess.dlightBits |= dlightBits;

	indices = ( const unsigned * ) ( ( ( const char  * ) surf ) + surf->ofsIndices );

	Bob = tess.numVertexes;
	tessIndexes = tess.indexes + tess.numIndexes;
	for ( i = surf->numIndices-1 ; i >= 0  ; i-- ) {
		tessIndexes[i] = indices[i] + Bob;
	}

	tess.numIndexes += surf->numIndices;

	numPoints = surf->numPoints;

	{
		if ( surf->normals ) {
			// per-vertex normals for non-coplanar faces
			memcpy( &tess.normal[ tess.numVertexes ], surf->normals, numPoints * sizeof( vec4_t ) );
		} else {
			normal = surf->plane.normal;
			for ( i = 0, ndx = tess.numVertexes; i < numPoints; i++, ndx++ ) {
				VectorCopy( normal, tess.normal[ndx] );
			}
		}
	}

		if( vk.pbrActive && surf->qtangents )	
			memcpy( &tess.qtangent[ tess.numVertexes ], surf->qtangents, numPoints * sizeof( vec4_t ) );	

		if( vk.pbrActive && surf->lightdir )	
			memcpy( &tess.lightdir[ tess.numVertexes ], surf->lightdir, numPoints * sizeof( vec4_t ) );

	for ( i = 0, v = surf->points[0], ndx = tess.numVertexes; i < numPoints; i++, v += VERTEXSIZE, ndx++ ) {
		VectorCopy( v, tess.xyz[ndx]);

		tess.texCoords[0][ndx][0] = v[6];
		tess.texCoords[0][ndx][1] = v[7];
		{
			tess.texCoords[1][ndx][0] = v[8];
			tess.texCoords[1][ndx][1] = v[9];
		}
		{
			uint32_t color;
			Com_Memcpy( &color, &v[10], sizeof( color ) );
			Com_Memcpy( &tess.vertexColors[ndx], &color, sizeof( color ) );
		}

		tess.vertexDlightBits[ndx] = dlightBits;
	}

	tess.numVertexes += surf->numPoints;
}


static float LodErrorForVolume( vec3_t local, float radius ) {
	vec3_t	world;
	float	d;

	// never let it go negative
	if ( r_lodCurveError->value < 0 ) {
		return 0;
	}

	world[0] = local[0] * backEnd.or.axis[0][0] + local[1] * backEnd.or.axis[1][0] + 
		local[2] * backEnd.or.axis[2][0] + backEnd.or.origin[0];
	world[1] = local[0] * backEnd.or.axis[0][1] + local[1] * backEnd.or.axis[1][1] + 
		local[2] * backEnd.or.axis[2][1] + backEnd.or.origin[1];
	world[2] = local[0] * backEnd.or.axis[0][2] + local[1] * backEnd.or.axis[1][2] + 
		local[2] * backEnd.or.axis[2][2] + backEnd.or.origin[2];

	VectorSubtract( world, backEnd.viewParms.or.origin, world );
	d = DotProduct( world, backEnd.viewParms.or.axis[0] );

	if ( d < 0 ) {
		d = -d;
	}
	d -= radius;
	if ( d < 1 ) {
		d = 1;
	}

	return r_lodCurveError->value / d;
}

void RB_SurfaceGridEstimate( srfGridMesh_t *cv, int *numVertexes, int *numIndexes )
{
	int		lodWidth, lodHeight;
	float	lodError;
	int		i, used, rows;
	int		nVertexes = 0;
	int		nIndexes = 0;
	int		irows, vrows;

	lodError = r_lodCurveError->value; // fixed quality for VBO

	lodWidth = 1;
	for ( i = 1 ; i < cv->width-1 ; i++ ) {
		if ( cv->widthLodError[i] <= lodError ) {
			lodWidth++;
		}
	}
	lodWidth++;

	lodHeight = 1;
	for ( i = 1 ; i < cv->height-1 ; i++ ) {
		if ( cv->heightLodError[i] <= lodError ) {
			lodHeight++;
		}
	}
	lodHeight++;

	used = 0;
	while ( used < lodHeight - 1 ) {
		// see how many rows of both verts and indexes we can add without overflowing
		do {
			vrows = ( SHADER_MAX_VERTEXES - tess.numVertexes ) / lodWidth;
			irows = ( SHADER_MAX_INDEXES - tess.numIndexes ) / ( lodWidth * 6 );

			// if we don't have enough space for at least one strip, flush the buffer
			if ( vrows < 2 || irows < 1 ) {
				nVertexes += tess.numVertexes;
				nIndexes += tess.numIndexes;
				tess.numIndexes = 0;
				tess.numVertexes = 0;
			} else {
				break;
			}
		} while ( 1 );
		
		rows = irows;
		if ( vrows < irows + 1 ) {
			rows = vrows - 1;
		}
		if ( used + rows > lodHeight ) {
			rows = lodHeight - used;
		}

		tess.numIndexes += (rows-1)*(lodWidth-1)*6;
		tess.numVertexes += rows * lodWidth;
		used += rows - 1;
	}

	*numVertexes = nVertexes + tess.numVertexes;
	*numIndexes = nIndexes + tess.numIndexes;
	tess.numVertexes = 0;
	tess.numIndexes = 0;
}

/*
=============
RB_SurfaceGrid

Just copy the grid of points and triangulate
=============
*/
static void RB_SurfaceGrid( srfGridMesh_t *cv ) {
	int		i, j;
	float	*xyz;
	float	*texCoords0;
	float	*texCoords1;
	float	*normal;
	float	*qtangent;
	float	*lightdir;
	uint32_t *color;
	srfVert_t *dv;
	int		rows, irows, vrows;
	int		used;
	int		widthTable[MAX_GRID_SIZE];
	int		heightTable[MAX_GRID_SIZE];
	float	lodError;
	int		lodWidth, lodHeight;
	int		numVertexes;
	int		dlightBits;
	int		*vDlightBits;

	if ( tess.allowVBO && cv->vboItemIndex && !cv->dlightBits &&
		RB_QueueSurfaceVBO( cv->vboItemIndex, SF_GRID ) ) {
		return;
	}

	VBO_Flush();

	dlightBits = cv->dlightBits;
	tess.dlightBits |= dlightBits;

	tess.surfType = SF_GRID;

	// determine the allowable discrepance
	if ( cv->vboItemIndex && ( tr.mapLoading || ( tess.dlightPass && tess.shader->isStaticShader ) ) )
		lodError = r_lodCurveError->value; // fixed quality for VBO
	else
		lodError = LodErrorForVolume( cv->lodOrigin, cv->lodRadius );

	// determine which rows and columns of the subdivision
	// we are actually going to use
	widthTable[0] = 0;
	lodWidth = 1;
	for ( i = 1 ; i < cv->width-1 ; i++ ) {
		if ( cv->widthLodError[i] <= lodError ) {
			widthTable[lodWidth] = i;
			lodWidth++;
		}
	}
	widthTable[lodWidth] = cv->width-1;
	lodWidth++;

	heightTable[0] = 0;
	lodHeight = 1;
	for ( i = 1 ; i < cv->height-1 ; i++ ) {
		if ( cv->heightLodError[i] <= lodError ) {
			heightTable[lodHeight] = i;
			lodHeight++;
		}
	}
	heightTable[lodHeight] = cv->height-1;
	lodHeight++;

	// very large grids may have more points or indexes than can be fit
	// in the tess structure, so we may have to issue it in multiple passes

	used = 0;
	while ( used < lodHeight - 1 ) {
		// see how many rows of both verts and indexes we can add without overflowing
		do {
			vrows = ( SHADER_MAX_VERTEXES - tess.numVertexes ) / lodWidth;
			irows = ( SHADER_MAX_INDEXES - tess.numIndexes ) / ( lodWidth * 6 );

			// if we don't have enough space for at least one strip, flush the buffer
			if ( vrows < 2 || irows < 1 ) {
				if ( tr.mapLoading ) {
					// estimate and flush
					if ( cv->vboItemIndex ) {
						VBO_PushData( cv->vboItemIndex, &tess );
						tess.numIndexes = 0;
						tess.numVertexes = 0;
					} else
						ri.Error( ERR_DROP, "Unexpected grid flush during map loading!\n" );
				} else {
					RB_EndSurface();
					RB_BeginSurface( tess.shader, tess.fogNum );
				}
			} else {
				break;
			}
		} while ( 1 );
		
		rows = irows;
		if ( vrows < irows + 1 ) {
			rows = vrows - 1;
		}
		if ( used + rows > lodHeight ) {
			rows = lodHeight - used;
		}

		numVertexes = tess.numVertexes;

		xyz = tess.xyz[numVertexes];
		normal = tess.normal[numVertexes];
		qtangent = tess.qtangent[numVertexes];
		lightdir = tess.lightdir[numVertexes];
		texCoords0 = tess.texCoords[0][numVertexes];
		texCoords1 = tess.texCoords[1][numVertexes];
		color = &tess.vertexColors[numVertexes].u32;
		vDlightBits = &tess.vertexDlightBits[numVertexes];
		for ( i = 0 ; i < rows ; i++ ) {
			for ( j = 0 ; j < lodWidth ; j++ ) {
				dv = cv->verts + heightTable[ used + i ] * cv->width
					+ widthTable[ j ];

				xyz[0] = dv->xyz[0];
				xyz[1] = dv->xyz[1];
				xyz[2] = dv->xyz[2];
				texCoords0[0] = dv->st[0];
				texCoords0[1] = dv->st[1];
				{
					texCoords1[0] = dv->lightmap[0];
					texCoords1[1] = dv->lightmap[1];
					texCoords1 += 2;
				}
				{
					normal[0] = dv->normal[0];
					normal[1] = dv->normal[1];
					normal[2] = dv->normal[2];
					normal += 4;
				}

				if( vk.pbrActive ) {
					qtangent[0] = dv->qtangent[0];
					qtangent[1] = dv->qtangent[1];
					qtangent[2] = dv->qtangent[2];
					qtangent[3] = dv->qtangent[3];
					qtangent += 4;

					lightdir[0] = dv->lightdir[0];
					lightdir[1] = dv->lightdir[1];
					lightdir[2] = dv->lightdir[2];
					lightdir[3] = 0.0;
					lightdir += 4;
				}

				*color = dv->color.u32;
				*vDlightBits++ = dlightBits;
				xyz += 4;
				texCoords0 += 2;
				color++;
			}
		}

		// add the indexes
		{
			int		numIndexes;
			int		w, h;

			h = rows - 1;
			w = lodWidth - 1;
			numIndexes = tess.numIndexes;
			for (i = 0 ; i < h ; i++) {
				for (j = 0 ; j < w ; j++) {
					int		v1, v2, v3, v4;
			
					// vertex order to be reckognized as tristrips
					v1 = numVertexes + i*lodWidth + j + 1;
					v2 = v1 - 1;
					v3 = v2 + lodWidth;
					v4 = v3 + 1;

					tess.indexes[numIndexes] = v2;
					tess.indexes[numIndexes+1] = v3;
					tess.indexes[numIndexes+2] = v1;
					
					tess.indexes[numIndexes+3] = v1;
					tess.indexes[numIndexes+4] = v3;
					tess.indexes[numIndexes+5] = v4;
					numIndexes += 6;
				}
			}

			tess.numIndexes = numIndexes;
		}

		tess.numVertexes += rows * lodWidth;

		used += rows - 1;
	}
}


/*
===========================================================================

NULL MODEL

===========================================================================
*/

/*
===================
RB_SurfaceAxis

Draws x/y/z lines from the origin for orientation debugging
===================
*/
static void RB_SurfaceAxis( void ) {
#ifdef USE_VULKAN
	int i;

	RB_EndSurface();

	GL_Bind( tr.whiteImage );
	Com_Memset( tess.xyz, 0, 6 * sizeof( tess.xyz[0] ) );
	tess.xyz[1][0] = 16.0;
	tess.xyz[3][1] = 16.0;
	tess.xyz[5][2] = 16.0;

	Com_Memset( tess.svars.colors[0], 0, 6 * sizeof( color4ub_t ) );
	for ( i = 0; i < 6; i++ )
		tess.svars.colors[0][i].rgba[3] = 255;

	tess.svars.colors[0][0].rgba[0] = 255;
	tess.svars.colors[0][1].rgba[0] = 255;
	tess.svars.colors[0][2].rgba[1] = 255;
	tess.svars.colors[0][3].rgba[1] = 255;
	tess.svars.colors[0][4].rgba[2] = 255;
	tess.svars.colors[0][5].rgba[2] = 255;

	tess.numVertexes = 6;

	vk_bind_pipeline( vk.surface_axis_pipeline );
	/* Could use common layout and avoid ST0 binding. */
	vk_bind_geometry( TESS_XYZ | TESS_RGBA0 | TESS_ST0 );
	vk_draw_geometry( DEPTH_RANGE_NORMAL, qfalse );

	tess.numVertexes = 0;
#else
	vec3_t xyz[6];
	color4ub_t colors[6];
	int i;

	GL_ClientState( 0, CLS_COLOR_ARRAY );

	qglDisable( GL_TEXTURE_2D );
	GL_State( GLS_DEFAULT );

	qglLineWidth( 3 );

	Com_Memset( xyz, 0, sizeof( xyz ) );
	xyz[1][0] = 16.0;
	xyz[3][1] = 16.0;
	xyz[5][2] = 16.0;

	Com_Memset( colors, 0, sizeof( colors ) );
	for ( i = 0; i < 6; i++ ) {
		colors[i].rgba[3] = 255;
	}

	colors[0].rgba[0] = 255;
	colors[1].rgba[0] = 255;
	colors[2].rgba[1] = 255;
	colors[3].rgba[1] = 255;
	colors[4].rgba[2] = 255;
	colors[5].rgba[2] = 255;

	qglVertexPointer( 3, GL_FLOAT, 0, xyz );
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colors[0].rgba );

	qglDrawArrays( GL_LINES, 0, 6 );

	qglLineWidth( 1 );

	qglEnable( GL_TEXTURE_2D );
#endif
}

//===========================================================================

/*
====================
RB_SurfaceEntity

Entities that have a single procedurally generated surface
====================
*/
static void RB_SurfaceEntity( const surfaceType_t *surfType ) {
#ifdef USE_VBO
	VBO_Flush();
#endif
	(void)surfType;
	switch( backEnd.currentEntity->e.reType ) {
	case RT_SPRITE:
		RB_SurfaceSprite();
		break;
	case RT_BEAM:
		RB_SurfaceBeam();
		break;
	case RT_RAIL_CORE:
		RB_SurfaceRailCore();
		break;
	case RT_RAIL_RINGS:
		RB_SurfaceRailRings();
		break;
	case RT_LIGHTNING:
		RB_SurfaceLightningBolt();
		break;
	default:
		RB_SurfaceAxis();
		break;
	}
#ifdef USE_VBO
	tess.surfType = SF_ENTITY;
#endif
}


static void RB_SurfaceBad( const surfaceType_t *surfType ) {
	ri.Printf( PRINT_ALL, "Bad surface tesselated.\n" );
	(void)surfType;
}


static void RB_SurfaceFlare( srfFlare_t *surf ) {
	if ( r_flares->integer ) {
#ifdef USE_VBO
		VBO_Flush();
		tess.surfType = SF_FLARE;
#endif
		RB_AddFlare( surf, tess.fogNum, surf->origin, surf->color, surf->normal );
	}
}


static void RB_SurfaceSkip( void *surf ) {
	(void)surf;
}

/*
================
RB_GLTFRecomputeQtangentsForTessRange
================
MikkTSpace-style tangent basis from deformed positions + UV0 (after CPU morph/skin).
Fills tess.qtangent and zeros lightdir for [vertBase, vertBase+numVerts).
================
*/
static void RB_GLTFRecomputeQtangentsForTessRange( int vertBase, int numVerts, int indexStart, int numIndexes ) {
	float (*tanAcc)[3];
	float (*btAcc)[3];
	int tri, k;

	if ( numVerts <= 0 || numIndexes < 3 ) {
		return;
	}

	tanAcc = (float (*)[3])ri.Hunk_AllocateTempMemory( numVerts * sizeof( *tanAcc ) );
	btAcc = (float (*)[3])ri.Hunk_AllocateTempMemory( numVerts * sizeof( *btAcc ) );
	Com_Memset( tanAcc, 0, numVerts * sizeof( *tanAcc ) );
	Com_Memset( btAcc, 0, numVerts * sizeof( *btAcc ) );

	for ( tri = 0; tri < numIndexes / 3; tri++ ) {
		int ia = tess.indexes[indexStart + tri * 3 + 0];
		int ib = tess.indexes[indexStart + tri * 3 + 1];
		int ic = tess.indexes[indexStart + tri * 3 + 2];
		vec3_t e1, e2, sdir, tdir;
		vec2_t duv1, duv2;
		float denom, f;
		int corner;

		VectorSubtract( tess.xyz[ib], tess.xyz[ia], e1 );
		VectorSubtract( tess.xyz[ic], tess.xyz[ia], e2 );
		duv1[0] = tess.texCoords[0][ib][0] - tess.texCoords[0][ia][0];
		duv1[1] = tess.texCoords[0][ib][1] - tess.texCoords[0][ia][1];
		duv2[0] = tess.texCoords[0][ic][0] - tess.texCoords[0][ia][0];
		duv2[1] = tess.texCoords[0][ic][1] - tess.texCoords[0][ia][1];
		denom = duv1[0] * duv2[1] - duv2[0] * duv1[1];
		if ( fabsf( denom ) < 1e-12f ) {
			continue;
		}
		f = 1.0f / denom;
		sdir[0] = f * ( duv2[1] * e1[0] - duv1[1] * e2[0] );
		sdir[1] = f * ( duv2[1] * e1[1] - duv1[1] * e2[1] );
		sdir[2] = f * ( duv2[1] * e1[2] - duv1[1] * e2[2] );
		tdir[0] = f * ( -duv2[0] * e1[0] + duv1[0] * e2[0] );
		tdir[1] = f * ( -duv2[0] * e1[1] + duv1[0] * e2[1] );
		tdir[2] = f * ( -duv2[0] * e1[2] + duv1[0] * e2[2] );

		for ( corner = 0; corner < 3; corner++ ) {
			int vx = ( corner == 0 ) ? ia : ( ( corner == 1 ) ? ib : ic );
			int li;

			if ( vx < vertBase || vx >= vertBase + numVerts ) {
				continue;
			}
			li = vx - vertBase;
			for ( k = 0; k < 3; k++ ) {
				tanAcc[li][k] += sdir[k];
				btAcc[li][k] += tdir[k];
			}
		}
	}

	for ( k = 0; k < numVerts; k++ ) {
		int vi = vertBase + k;
		vec3_t n, t, b, up;
		float d;

		VectorCopy( tess.normal[vi], n );
		VectorNormalize( n );
		d = DotProduct( n, tanAcc[k] );
		t[0] = tanAcc[k][0] - n[0] * d;
		t[1] = tanAcc[k][1] - n[1] * d;
		t[2] = tanAcc[k][2] - n[2] * d;
		if ( VectorLength( t ) < 1e-8f ) {
			if ( fabsf( n[2] ) < 0.9f ) {
				VectorSet( up, 0.0f, 0.0f, 1.0f );
			} else {
				VectorSet( up, 1.0f, 0.0f, 0.0f );
			}
			CrossProduct( n, up, t );
		}
		VectorNormalize( t );
		CrossProduct( n, t, b );
		d = DotProduct( b, btAcc[k] );
		tess.qtangent[vi][0] = t[0];
		tess.qtangent[vi][1] = t[1];
		tess.qtangent[vi][2] = t[2];
		tess.qtangent[vi][3] = ( d < 0.0f ) ? -1.0f : 1.0f;
		Vector4Set( tess.lightdir[vi], 0.0f, 0.0f, 0.0f, 0.0f );
	}

	ri.Hunk_FreeTempMemory( btAcc );
	ri.Hunk_FreeTempMemory( tanAcc );
} /* USE_VK_PBR */

/*
=============
RB_GLTFSurface
=============
glTF mesh primitive. Uses VBO when available (static, no skinning/morph); otherwise tess path.
Clip selection: refEntity.frame / oldframe index animations (RF_WRAP_FRAMES); time from shaderTime
or refdef.time, scaled by r_gltfAnim. Cross-clip blend uses refEntity.backlerp (toward oldframe).
Morph: glTF weight animation + RE_SetEntityMorphWeight name hashes vs mesh target_names.
*/
static int RB_GLTFMorphIndexByHash( const gltfMorphTarget_t *targets, int num, uint32_t hash ) {
	int i;
	char lower[MAX_QPATH];

	for ( i = 0; i < num; i++ ) {
		if ( !targets[i].name[0] ) {
			continue;
		}
		Q_strncpyz( lower, targets[i].name, sizeof( lower ) );
		Q_strlwr( lower );
		if ( (uint32_t)Com_GenerateHashValue( lower, 0x7fffffffU ) == hash ) {
			return i;
		}
	}
	return -1;
}

void RB_GLTFSurface( const surfaceType_t *surface ) {
	const srfGLTFPrimitive_t *surf = (const srfGLTFPrimitive_t *)surface;
	const gltfVertex_t *v;
	const trRefEntity_t *ent;
	int i, j;
	int base;
	float morphW[GLTF_MAX_MORPH_TARGETS];
	float morphWPrev[GLTF_MAX_MORPH_TARGETS];
	qboolean useMorph;
	const gltfModel_t *model;
	float jointMatrix[GLTF_MAX_JOINTS * 12];
	float jointMatrixPrev[GLTF_MAX_JOINTS * 12];
	qboolean haveJoints;
	int animCur, animOld;
	float timeCur, timeOld, backlerp;
	float speed;
	ent = backEnd.currentEntity;
	model = ( tr.currentModel && tr.currentModel->modelData )
		? R_GetGLTFModelFromModelData( tr.currentModel->modelData ) : NULL;

	Com_Memset( morphW, 0, sizeof( morphW ) );
	Com_Memset( morphWPrev, 0, sizeof( morphWPrev ) );
	useMorph = ( surf->numMorphTargets > 0 && surf->morphTargets != NULL && r_morph && r_morph->integer );

	speed = ( r_gltfAnim && r_gltfAnim->value > 0.0f ) ? r_gltfAnim->value : 1.0f;
	if ( ent->intShaderTime ) {
		timeCur = ent->e.shaderTime.i * 0.001f * speed;
	} else {
		timeCur = ent->e.shaderTime.f * speed;
	}
	if ( !timeCur && tr.refdef.time > 0 ) {
		timeCur = tr.refdef.time * 0.001f * speed;
	}
	timeOld = timeCur;
	backlerp = ent->e.backlerp;
	if ( backlerp < 0.0f ) {
		backlerp = 0.0f;
	} else if ( backlerp > 1.0f ) {
		backlerp = 1.0f;
	}

	animCur = ent->e.frame;
	animOld = ent->e.oldframe;
	if ( model && model->numAnimations > 0 ) {
		if ( ent->e.renderfx & RF_WRAP_FRAMES ) {
			if ( animCur < 0 ) {
				animCur = 0;
			} else {
				animCur %= model->numAnimations;
			}
			if ( animOld < 0 ) {
				animOld = 0;
			} else {
				animOld %= model->numAnimations;
			}
		} else {
			if ( animCur < 0 || animCur >= model->numAnimations ) {
				animCur = 0;
			}
			if ( animOld < 0 || animOld >= model->numAnimations ) {
				animOld = animCur;
			}
		}
	} else {
		/* No clips: ignore frame indices (avoids using game entity frame numbers as anim indices). */
		animCur = -1;
		animOld = -1;
	}

	if ( useMorph && model && animCur >= 0 ) {
		float wB[GLTF_MAX_MORPH_TARGETS];
		(void)R_SampleGLTFMeshMorphWeights( model, animCur, timeCur, surf->meshIndex, morphW, surf->numMorphTargets );
		if ( backlerp > 0.0f && animOld >= 0 && animOld != animCur ) {
			Com_Memset( wB, 0, sizeof( wB ) );
			(void)R_SampleGLTFMeshMorphWeights( model, animOld, timeOld, surf->meshIndex, wB, surf->numMorphTargets );
			for ( i = 0; i < surf->numMorphTargets; i++ ) {
				morphW[i] = morphW[i] * ( 1.0f - backlerp ) + wB[i] * backlerp;
			}
			Com_Memcpy( morphWPrev, wB, sizeof( float ) * (size_t)surf->numMorphTargets );
		} else {
			Com_Memcpy( morphWPrev, morphW, sizeof( float ) * (size_t)surf->numMorphTargets );
		}
	}
	if ( useMorph && ent->morphChannelCount > 0 ) {
		for ( i = 0; i < ent->morphChannelCount; i++ ) {
			int ti = RB_GLTFMorphIndexByHash( surf->morphTargets, surf->numMorphTargets, ent->morphChannelHashes[i] );
			if ( ti >= 0 ) {
				morphW[ti] += ent->morphChannelWeights[i];
				if ( i < IQM_MORPH_MAX_CHANNELS ) {
					morphWPrev[ti] += ent->morphChannelWeightPrev[i];
				}
			}
		}
	}
	if ( useMorph && model && surf->meshIndex >= 0 && surf->meshIndex < model->numMeshes ) {
		const gltfMesh_t *gm = &model->meshes[surf->meshIndex];
		int nw = gm->numDefaultMorphWeights;
		if ( nw > surf->numMorphTargets ) {
			nw = surf->numMorphTargets;
		}
		for ( i = 0; i < nw; i++ ) {
			morphW[i] += gm->defaultMorphWeights[i];
			morphWPrev[i] += gm->defaultMorphWeights[i];
		}
	}

	haveJoints = ( qboolean )( surf->hasSkinning && model && model->skeleton.numJoints > 0 );
	if ( haveJoints ) {
		if ( animCur >= 0 && model->numAnimations > 0 ) {
			if ( backlerp > 0.001f && animOld >= 0 && animOld != animCur ) {
				R_ComputeGLTFJointMatricesBlend( model, animCur, timeCur, animOld, timeOld, backlerp, jointMatrix );
				R_ComputeGLTFJointMatrices( model, animOld, timeOld, jointMatrixPrev );
			} else {
				R_ComputeGLTFJointMatrices( model, animCur, timeCur, jointMatrix );
				Com_Memcpy( jointMatrixPrev, jointMatrix, sizeof( jointMatrixPrev ) );
			}
		} else {
			R_ComputeGLTFJointMatrices( model, -1, 0.0f, jointMatrix );
			Com_Memcpy( jointMatrixPrev, jointMatrix, sizeof( jointMatrixPrev ) );
		}
	}

	if ( r_gltfGpu && r_gltfGpu->integer && vk.cmd && vk.pbrActive && tess.shader && tess.shader->hasPBR &&
		surf->vbo_vertex != TR_GLTF_VBO_HANDLE_INVALID && surf->vbo_index != TR_GLTF_VBO_HANDLE_INVALID &&
		( haveJoints || useMorph ) &&
		surf->numVertices > 0 && surf->numVertices <= SHADER_MAX_VERTEXES ) {
		qboolean gpuOk = qtrue;
		int nj = ( model && model->skeleton.numJoints > 0 ) ? model->skeleton.numJoints : 0;
		size_t skinFloats = 1u + 2u * ( (size_t)nj * 12u );
		size_t skinBytes = skinFloats * sizeof( float );
		uint32_t skinOff = 0;
		float *skinPayload = (float *)vk_alloc_storage( skinBytes, &skinOff );
		int morphN = surf->numMorphTargets;
		float mwCopy[GLTF_MAX_MORPH_TARGETS];
		float mwPrevCopy[GLTF_MAX_MORPH_TARGETS];
		int topMorphIdx[IQM_MORPH_TOP_K];
		float topMorphW[IQM_MORPH_TOP_K];
		float topMorphWPrev[IQM_MORPH_TOP_K];
		int morphActive = 0;
		int mk;
		size_t morphFloats;
		size_t morphBytes;
		uint32_t morphOff = 0;
		float *morphPayload;
		uint32_t idxOff;
		uint32_t topoOff = 0;

		if ( !skinPayload ) {
			gpuOk = qfalse;
		} else {
			skinPayload[0] = (float)nj;
			if ( nj > 0 ) {
				Com_Memcpy( skinPayload + 1, jointMatrix, (size_t)nj * 12u * sizeof( float ) );
				Com_Memcpy( skinPayload + 1 + (size_t)nj * 12u, jointMatrixPrev, (size_t)nj * 12u * sizeof( float ) );
			}
		}

		if ( morphN > GLTF_MAX_MORPH_TARGETS ) {
			morphN = GLTF_MAX_MORPH_TARGETS;
		}
		Com_Memcpy( mwCopy, morphW, sizeof( mwCopy ) );
		Com_Memcpy( mwPrevCopy, morphWPrev, sizeof( mwPrevCopy ) );
		for ( mk = 0; mk < IQM_MORPH_TOP_K; mk++ ) {
			int best = -1;
			float bestW = 0.0f;
			for ( int ti = 0; ti < morphN; ti++ ) {
				if ( mwCopy[ti] > bestW ) {
					bestW = mwCopy[ti];
					best = ti;
				}
			}
			if ( best < 0 || bestW <= 1e-6f ) {
				break;
			}
			topMorphIdx[morphActive] = best;
			topMorphW[morphActive] = bestW;
			topMorphWPrev[morphActive] = mwPrevCopy[best];
			mwCopy[best] = 0.0f;
			mwPrevCopy[best] = 0.0f;
			morphActive++;
		}

		morphFloats = (size_t)( 2 + 2u * (size_t)IQM_MORPH_TOP_K ) + (size_t)surf->numVertices * (size_t)IQM_MORPH_TOP_K * 6u;
		morphBytes = morphFloats * sizeof( float );
		morphPayload = gpuOk ? (float *)vk_alloc_storage( morphBytes, &morphOff ) : NULL;
		if ( gpuOk && !morphPayload ) {
			gpuOk = qfalse;
		}
		if ( gpuOk ) {
			morphPayload[0] = (float)surf->numVertices;
			morphPayload[1] = (float)morphActive;
			for ( mk = 0; mk < IQM_MORPH_TOP_K; mk++ ) {
				morphPayload[2 + mk] = ( mk < morphActive ) ? topMorphW[mk] : 0.0f;
				morphPayload[2 + IQM_MORPH_TOP_K + mk] = ( mk < morphActive ) ? topMorphWPrev[mk] : 0.0f;
			}
			for ( i = 0; i < surf->numVertices; i++ ) {
				const size_t morphBase = (size_t)( 2 + 2u * (size_t)IQM_MORPH_TOP_K );
				for ( mk = 0; mk < morphActive; mk++ ) {
					int tgt = topMorphIdx[mk];
					size_t dst = (size_t)i * (size_t)IQM_MORPH_TOP_K * 6u + (size_t)mk * 6u;
					const gltfMorphTarget_t *mt = &surf->morphTargets[tgt];
					const float *dp = mt->deltaPosition ? ( mt->deltaPosition + i * 3 ) : NULL;
					const float *dn = mt->deltaNormal ? ( mt->deltaNormal + i * 3 ) : NULL;
					morphPayload[morphBase + dst + 0] = dp ? dp[0] : 0.0f;
					morphPayload[morphBase + dst + 1] = dp ? dp[1] : 0.0f;
					morphPayload[morphBase + dst + 2] = dp ? dp[2] : 0.0f;
					morphPayload[morphBase + dst + 3] = dn ? dn[0] : 0.0f;
					morphPayload[morphBase + dst + 4] = dn ? dn[1] : 0.0f;
					morphPayload[morphBase + dst + 5] = dn ? dn[2] : 0.0f;
				}
				for ( mk = morphActive; mk < IQM_MORPH_TOP_K; mk++ ) {
					size_t dst = (size_t)i * (size_t)IQM_MORPH_TOP_K * 6u + (size_t)mk * 6u;
					Com_Memset( morphPayload + morphBase + dst, 0, 6u * sizeof( float ) );
				}
			}
		}

		idxOff = vk_tess_index( surf->numIndices, surf->indices );
		if ( gpuOk && idxOff == ~0U ) {
			gpuOk = qfalse;
		}

		if ( gpuOk && r_gltfGpuTangentFix && r_gltfGpuTangentFix->integer >= 2 &&
			surf->gltfTopoData && surf->gltfTopoNumUints > 0 ) {
			const int blobInts = R_GLTFTopoDrawBlobUints( surf->numIndices, surf->numVertices );
			uint32_t *blob = (uint32_t *)vk_alloc_storage( (size_t)blobInts * sizeof( uint32_t ), &topoOff );
			int topoBaseIgnored;

			if ( !blob ) {
				gpuOk = qfalse;
			} else {
				R_GLTFTopoPackDrawBlob( surf->indices, surf->numIndices, surf->numVertices,
					surf->gltfTopoData, surf->vertices, blob, &topoBaseIgnored );
				(void)topoBaseIgnored;
			}
		}

		if ( !gpuOk ) {
			vk_reset_iqm_storage_offsets();
		} else {
			vk_set_iqm_storage_offsets( skinOff, morphOff, topoOff );
			tess.gltfGpuMorphActive = ( morphActive > 0 ) ? qtrue : qfalse;
			tess.gltfGpuMorphCount = morphActive;
			for ( mk = 0; mk < IQM_MORPH_TOP_K; mk++ ) {
				tess.gltfGpuMorphWeights[mk] = ( mk < morphActive ) ? topMorphW[mk] : 0.0f;
			}
			tess.gltfUseGpuPipeline = qtrue;
			tess.gltfDrawSurface = surf;
			tess.numVertexes = surf->numVertices;
			tess.numIndexes = surf->numIndices;
			vk_bind_index_buffer( vk.cmd->vertex_buffer, idxOff );

			for ( i = 0; i < surf->numVertices; i++ ) {
				v = &surf->vertices[i];
				tess.xyz[i][0] = v->position[0];
				tess.xyz[i][1] = v->position[1];
				tess.xyz[i][2] = v->position[2];
				tess.xyz[i][3] = R_GLTFPackGpuVertexMeta( i );
				tess.normal[i][0] = v->normal[0];
				tess.normal[i][1] = v->normal[1];
				tess.normal[i][2] = v->normal[2];
				tess.normal[i][3] = 0.0f;
				tess.texCoords[0][i][0] = v->texCoord0[0];
				tess.texCoords[0][i][1] = v->texCoord0[1];
				tess.vertexColors[i].rgba[0] = (byte)( v->color[0] * 255 );
				tess.vertexColors[i].rgba[1] = (byte)( v->color[1] * 255 );
				tess.vertexColors[i].rgba[2] = (byte)( v->color[2] * 255 );
				tess.vertexColors[i].rgba[3] = (byte)( v->color[3] * 255 );
				tess.qtangent[i][0] = v->tangent[0];
				tess.qtangent[i][1] = v->tangent[1];
				tess.qtangent[i][2] = v->tangent[2];
				tess.qtangent[i][3] = v->tangent[3];
				Vector4Set( tess.lightdir[i], 0.0f, 0.0f, 0.0f, 0.0f );
			}
			return;
		}
	}
	tess.gltfUseGpuPipeline = qfalse;
	tess.gltfGpuMorphActive = qfalse;
	tess.gltfGpuMorphCount = 0; /* USE_VK_PBR */

	if ( surf->vbo_vertex != TR_GLTF_VBO_HANDLE_INVALID && surf->vbo_index != TR_GLTF_VBO_HANDLE_INVALID ) {
		/* VBO path: set gltfDrawSurface for vk_bind_geometry to use */
		tess.gltfDrawSurface = surf;
		tess.numVertexes = surf->numVertices;
		tess.numIndexes = surf->numIndices;
		return;
	}

	/* Tess path (skinning, morph targets, or VBO creation failed) */
	tess.gltfDrawSurface = NULL;
	RB_CHECKOVERFLOW( surf->numVertices, surf->numIndices );

	base = tess.numVertexes;

	for ( i = 0; i < surf->numVertices; i++ ) {
		vec3_t pos, nrm;
		v = &surf->vertices[i];

		VectorCopy( v->position, pos );
		VectorCopy( v->normal, nrm );
		if ( useMorph ) {
			int ti;
			for ( ti = 0; ti < surf->numMorphTargets; ti++ ) {
				float w = morphW[ti];
				const gltfMorphTarget_t *mt;
				if ( w == 0.0f ) {
					continue;
				}
				mt = &surf->morphTargets[ti];
				if ( mt->deltaPosition ) {
					pos[0] += w * mt->deltaPosition[i * 3 + 0];
					pos[1] += w * mt->deltaPosition[i * 3 + 1];
					pos[2] += w * mt->deltaPosition[i * 3 + 2];
				}
				if ( mt->deltaNormal ) {
					nrm[0] += w * mt->deltaNormal[i * 3 + 0];
					nrm[1] += w * mt->deltaNormal[i * 3 + 1];
					nrm[2] += w * mt->deltaNormal[i * 3 + 2];
				}
			}
			VectorNormalize( nrm );
		}

		if ( haveJoints ) {
			vec3_t spos, snrm;
			float w0 = v->weights[0], w1 = v->weights[1], w2 = v->weights[2], w3 = v->weights[3];
			int j0 = v->joints[0], j1 = v->joints[1], j2 = v->joints[2], j3 = v->joints[3];
			float *m0, *m1, *m2, *m3;

			VectorClear( spos );
			VectorClear( snrm );
			if ( w0 > 0 && j0 < model->skeleton.numJoints ) {
				m0 = &jointMatrix[j0 * 12];
				spos[0] += w0 * ( m0[0] * pos[0] + m0[1] * pos[1] + m0[2] * pos[2] + m0[3] );
				spos[1] += w0 * ( m0[4] * pos[0] + m0[5] * pos[1] + m0[6] * pos[2] + m0[7] );
				spos[2] += w0 * ( m0[8] * pos[0] + m0[9] * pos[1] + m0[10] * pos[2] + m0[11] );
				snrm[0] += w0 * ( m0[0] * nrm[0] + m0[1] * nrm[1] + m0[2] * nrm[2] );
				snrm[1] += w0 * ( m0[4] * nrm[0] + m0[5] * nrm[1] + m0[6] * nrm[2] );
				snrm[2] += w0 * ( m0[8] * nrm[0] + m0[9] * nrm[1] + m0[10] * nrm[2] );
			}
			if ( w1 > 0 && j1 < model->skeleton.numJoints ) {
				m1 = &jointMatrix[j1 * 12];
				spos[0] += w1 * ( m1[0] * pos[0] + m1[1] * pos[1] + m1[2] * pos[2] + m1[3] );
				spos[1] += w1 * ( m1[4] * pos[0] + m1[5] * pos[1] + m1[6] * pos[2] + m1[7] );
				spos[2] += w1 * ( m1[8] * pos[0] + m1[9] * pos[1] + m1[10] * pos[2] + m1[11] );
				snrm[0] += w1 * ( m1[0] * nrm[0] + m1[1] * nrm[1] + m1[2] * nrm[2] );
				snrm[1] += w1 * ( m1[4] * nrm[0] + m1[5] * nrm[1] + m1[6] * nrm[2] );
				snrm[2] += w1 * ( m1[8] * nrm[0] + m1[9] * nrm[1] + m1[10] * nrm[2] );
			}
			if ( w2 > 0 && j2 < model->skeleton.numJoints ) {
				m2 = &jointMatrix[j2 * 12];
				spos[0] += w2 * ( m2[0] * pos[0] + m2[1] * pos[1] + m2[2] * pos[2] + m2[3] );
				spos[1] += w2 * ( m2[4] * pos[0] + m2[5] * pos[1] + m2[6] * pos[2] + m2[7] );
				spos[2] += w2 * ( m2[8] * pos[0] + m2[9] * pos[1] + m2[10] * pos[2] + m2[11] );
				snrm[0] += w2 * ( m2[0] * nrm[0] + m2[1] * nrm[1] + m2[2] * nrm[2] );
				snrm[1] += w2 * ( m2[4] * nrm[0] + m2[5] * nrm[1] + m2[6] * nrm[2] );
				snrm[2] += w2 * ( m2[8] * nrm[0] + m2[9] * nrm[1] + m2[10] * nrm[2] );
			}
			if ( w3 > 0 && j3 < model->skeleton.numJoints ) {
				m3 = &jointMatrix[j3 * 12];
				spos[0] += w3 * ( m3[0] * pos[0] + m3[1] * pos[1] + m3[2] * pos[2] + m3[3] );
				spos[1] += w3 * ( m3[4] * pos[0] + m3[5] * pos[1] + m3[6] * pos[2] + m3[7] );
				spos[2] += w3 * ( m3[8] * pos[0] + m3[9] * pos[1] + m3[10] * pos[2] + m3[11] );
				snrm[0] += w3 * ( m3[0] * nrm[0] + m3[1] * nrm[1] + m3[2] * nrm[2] );
				snrm[1] += w3 * ( m3[4] * nrm[0] + m3[5] * nrm[1] + m3[6] * nrm[2] );
				snrm[2] += w3 * ( m3[8] * nrm[0] + m3[9] * nrm[1] + m3[10] * nrm[2] );
			}
			VectorNormalize( snrm );
			VectorCopy( spos, pos );
			VectorCopy( snrm, nrm );
		}

		tess.xyz[base + i][0] = pos[0];
		tess.xyz[base + i][1] = pos[1];
		tess.xyz[base + i][2] = pos[2];
		tess.normal[base + i][0] = nrm[0];
		tess.normal[base + i][1] = nrm[1];
		tess.normal[base + i][2] = nrm[2];
		tess.texCoords[0][base + i][0] = v->texCoord0[0];
		tess.texCoords[0][base + i][1] = v->texCoord0[1];
		tess.vertexColors[base + i].rgba[0] = (byte)( v->color[0] * 255 );
		tess.vertexColors[base + i].rgba[1] = (byte)( v->color[1] * 255 );
		tess.vertexColors[base + i].rgba[2] = (byte)( v->color[2] * 255 );
		tess.vertexColors[base + i].rgba[3] = (byte)( v->color[3] * 255 );
	}

	tess.numVertexes += surf->numVertices;

	{
		int idxBase = tess.numIndexes;
		for ( j = 0; j < surf->numIndices; j++ ) {
			tess.indexes[tess.numIndexes + j] = (glIndex_t)( base + surf->indices[j] );
		}
		tess.numIndexes += surf->numIndices;
		if ( vk.pbrActive && tess.shader && tess.shader->hasPBR ) {
			RB_GLTFRecomputeQtangentsForTessRange( base, surf->numVertices, idxBase, surf->numIndices );
		}
	}
}

void (*rb_surfaceTable[SF_NUM_SURFACE_TYPES])( void *) = {
	(void(*)(void*))RB_SurfaceBad,			// SF_BAD, 
	(void(*)(void*))RB_SurfaceSkip,			// SF_SKIP, 
	(void(*)(void*))RB_SurfaceFace,			// SF_FACE,
	(void(*)(void*))RB_SurfaceGrid,			// SF_GRID,
	(void(*)(void*))RB_SurfaceTriangles,	// SF_TRIANGLES,
	(void(*)(void*))RB_SurfacePolychain,	// SF_POLY,
	(void(*)(void*))RB_SurfaceMesh,			// SF_MD3,
	(void(*)(void*))RB_MDRSurfaceAnim,		// SF_MDR,
	(void(*)(void*))RB_IQMSurfaceAnim,		// SF_IQM,
	(void(*)(void*))RB_GLTFSurface,			// SF_GLTF,
	(void(*)(void*))RB_SurfaceFlare,		// SF_FLARE,
	(void(*)(void*))RB_SurfaceEntity		// SF_ENTITY
};
