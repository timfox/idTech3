/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CPU Loop & Blinn glyphlet builder: ear-clipped solid fill + curve triangles.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "tr_vector_font_glyphlet.h"

#ifdef BUILD_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

typedef struct {
	float x;
	float y;
} glPoint2_t;

typedef struct {
	glPoint2_t  points[VECTOR_GLYPHLET_MAX_CONTOUR_PTS];
	int         count;
} glContour_t;

typedef struct {
	glContour_t             contours[VECTOR_GLYPHLET_MAX_CONTOURS];
	int                     contourCount;
	float                   emScale;
	vectorGlyphletAtlas_t  *atlas;
	vectorGlyphletInfo_t   *info;
} glOutline_t;

static float GL_Cross2( glPoint2_t a, glPoint2_t b, glPoint2_t c ) {
	return ( b.x - a.x ) * ( c.y - a.y ) - ( b.y - a.y ) * ( c.x - a.x );
}

static float GL_Area2( const glContour_t *c ) {
	float area = 0.0f;
	int i;

	for ( i = 0; i < c->count; i++ ) {
		const glPoint2_t *p0 = &c->points[i];
		const glPoint2_t *p1 = &c->points[( i + 1 ) % c->count];
		area += p0->x * p1->y - p1->x * p0->y;
	}
	return area * 0.5f;
}

static qboolean GL_ContourIsEar( const glContour_t *poly, const int *idx, int n, int u, int v, int w ) {
	glPoint2_t a = poly->points[idx[u]];
	glPoint2_t b = poly->points[idx[v]];
	glPoint2_t c = poly->points[idx[w]];
	int i;

	if ( GL_Cross2( a, b, c ) <= 0.0f ) {
		return qfalse;
	}
	for ( i = 0; i < n; i++ ) {
		if ( i == u || i == v || i == w ) {
			continue;
		}
		glPoint2_t p = poly->points[idx[i]];
		float c0 = GL_Cross2( a, b, p );
		float c1 = GL_Cross2( b, c, p );
		float c2 = GL_Cross2( c, a, p );
		if ( c0 >= 0.0f && c1 >= 0.0f && c2 >= 0.0f ) {
			return qfalse;
		}
	}
	return qtrue;
}

static int GL_TriangulateContour( const glContour_t *poly, uint32_t vertBase,
	vectorGlyphletAtlas_t *atlas, vectorGlyphletInfo_t *info ) {
	int idx[VECTOR_GLYPHLET_MAX_CONTOUR_PTS];
	int n = poly->count;
	int remaining = n;
	int guard = n * n;
	int v = 0;
	int trisAdded = 0;

	if ( n < 3 ) {
		return 0;
	}

	for ( v = 0; v < n; v++ ) {
		idx[v] = v;
	}

	while ( remaining > 2 && guard-- > 0 ) {
		int u = ( v + remaining - 1 ) % remaining;
		int w = ( v + 1 ) % remaining;

		if ( !GL_ContourIsEar( poly, idx, remaining, u, v, w ) ) {
			v = ( v + 1 ) % remaining;
			continue;
		}

		if ( info->primitiveCount + 1u > VECTOR_MAX_GLYPHLET_TRIS ||
			atlas->indexCount + 3u > atlas->indexCapacity ||
			atlas->primCount + 1u > atlas->primCapacity ) {
			return trisAdded;
		}

		atlas->indices[atlas->indexCount + 0] = vertBase + (uint32_t)idx[u];
		atlas->indices[atlas->indexCount + 1] = vertBase + (uint32_t)idx[v];
		atlas->indices[atlas->indexCount + 2] = vertBase + (uint32_t)idx[w];
		atlas->indexCount += 3;

		atlas->primTypes[atlas->primCount] = VECTOR_GLYPHLET_TRI_SOLID;
		atlas->primCount++;
		info->primitiveCount++;
		trisAdded++;

		for ( u = v; u < remaining - 1; u++ ) {
			idx[u] = idx[u + 1];
		}
		remaining--;
		v = 0;
	}

	return trisAdded;
}

static qboolean GL_AtlasGrowVert( vectorGlyphletAtlas_t *atlas, uint32_t need ) {
	vectorGlyphletVert_t *nv;

	if ( need <= atlas->vertCapacity ) {
		return qtrue;
	}
	if ( need > VECTOR_GLYPHLET_ATLAS_MAX_VERTS ) {
		return qfalse;
	}
	nv = (vectorGlyphletVert_t *)Z_Malloc( (size_t)need * sizeof( *nv ) );
	if ( !nv ) {
		return qfalse;
	}
	if ( atlas->verts ) {
		Com_Memcpy( nv, atlas->verts, (size_t)atlas->vertCount * sizeof( *nv ) );
		Z_Free( atlas->verts );
	}
	atlas->verts = nv;
	atlas->vertCapacity = need;
	return qtrue;
}

static qboolean GL_AtlasGrowIndex( vectorGlyphletAtlas_t *atlas, uint32_t need ) {
	uint32_t *ni;

	if ( need <= atlas->indexCapacity ) {
		return qtrue;
	}
	if ( need > VECTOR_GLYPHLET_ATLAS_MAX_INDICES ) {
		return qfalse;
	}
	ni = (uint32_t *)Z_Malloc( (size_t)need * sizeof( *ni ) );
	if ( !ni ) {
		return qfalse;
	}
	if ( atlas->indices ) {
		Com_Memcpy( ni, atlas->indices, (size_t)atlas->indexCount * sizeof( *ni ) );
		Z_Free( atlas->indices );
	}
	atlas->indices = ni;
	atlas->indexCapacity = need;
	return qtrue;
}

static qboolean GL_AtlasGrowPrim( vectorGlyphletAtlas_t *atlas, uint32_t need ) {
	uint8_t *np;

	if ( need <= atlas->primCapacity ) {
		return qtrue;
	}
	if ( need > VECTOR_GLYPHLET_ATLAS_MAX_TRIS ) {
		return qfalse;
	}
	np = (uint8_t *)Z_Malloc( (size_t)need );
	if ( !np ) {
		return qfalse;
	}
	if ( atlas->primTypes ) {
		Com_Memcpy( np, atlas->primTypes, (size_t)atlas->primCount );
		Z_Free( atlas->primTypes );
	}
	atlas->primTypes = np;
	atlas->primCapacity = need;
	return qtrue;
}

static qboolean GL_AtlasAddVertex( vectorGlyphletAtlas_t *atlas, float x, float y,
	float u, float v ) {
	vectorGlyphletVert_t *vert;

	if ( !GL_AtlasGrowVert( atlas, atlas->vertCount + 1u ) ) {
		return qfalse;
	}
	vert = &atlas->verts[atlas->vertCount];
	vert->x = x;
	vert->y = y;
	vert->canonU = u;
	vert->canonV = v;
	atlas->vertCount++;
	return qtrue;
}

static qboolean GL_AtlasAddCurveTri( vectorGlyphletAtlas_t *atlas, vectorGlyphletInfo_t *info,
	glPoint2_t p1, glPoint2_t p2, glPoint2_t p3 ) {
	uint32_t base;
	uint8_t triType;
	float cross;

	if ( info->primitiveCount + 1u > VECTOR_MAX_GLYPHLET_TRIS ) {
		return qfalse;
	}
	if ( atlas->vertCount + 3u > atlas->vertCapacity && !GL_AtlasGrowVert( atlas, atlas->vertCount + 3u ) ) {
		return qfalse;
	}
	if ( !GL_AtlasGrowIndex( atlas, atlas->indexCount + 3u ) ||
		!GL_AtlasGrowPrim( atlas, atlas->primCount + 1u ) ) {
		return qfalse;
	}

	base = atlas->vertCount;
	if ( !GL_AtlasAddVertex( atlas, p1.x, p1.y, 0.0f, 0.0f ) ||
		!GL_AtlasAddVertex( atlas, p2.x, p2.y, 0.5f, 0.0f ) ||
		!GL_AtlasAddVertex( atlas, p3.x, p3.y, 1.0f, 0.0f ) ) {
		return qfalse;
	}

	cross = GL_Cross2( p1, p2, p3 );
	triType = ( cross >= 0.0f ) ? VECTOR_GLYPHLET_TRI_CONVEX : VECTOR_GLYPHLET_TRI_CONCAVE;

	atlas->indices[atlas->indexCount + 0] = base + 0u;
	atlas->indices[atlas->indexCount + 1] = base + 1u;
	atlas->indices[atlas->indexCount + 2] = base + 2u;
	atlas->indexCount += 3;

	atlas->primTypes[atlas->primCount] = triType;
	atlas->primCount++;
	info->primitiveCount++;
	return qtrue;
}

static glPoint2_t GL_FontPoint( const FT_Vector *v, float emScale ) {
	glPoint2_t p;
	p.x = (float)v->x * emScale;
	p.y = (float)v->y * emScale;
	return p;
}

static int GL_OutlineMoveTo( const FT_Vector *to, void *user ) {
	glOutline_t *o = (glOutline_t *)user;

	if ( o->contourCount >= VECTOR_GLYPHLET_MAX_CONTOURS ) {
		return 0;
	}
	if ( o->contourCount > 0 && o->contours[o->contourCount - 1].count == 0 ) {
		o->contours[o->contourCount - 1].points[0] = GL_FontPoint( to, o->emScale );
		o->contours[o->contourCount - 1].count = 1;
		return 0;
	}
	o->contours[o->contourCount].points[0] = GL_FontPoint( to, o->emScale );
	o->contours[o->contourCount].count = 1;
	o->contourCount++;
	return 0;
}

static int GL_OutlineAddPoint( glOutline_t *o, glPoint2_t p ) {
	glContour_t *c;

	if ( o->contourCount <= 0 ) {
		return 0;
	}
	c = &o->contours[o->contourCount - 1];
	if ( c->count >= VECTOR_GLYPHLET_MAX_CONTOUR_PTS ) {
		return 0;
	}
	c->points[c->count++] = p;
	return 0;
}

static int GL_OutlineLineTo( const FT_Vector *to, void *user ) {
	glOutline_t *o = (glOutline_t *)user;
	glPoint2_t end;
	glPoint2_t mid;
	glPoint2_t last;

	if ( o->contourCount <= 0 || o->contours[o->contourCount - 1].count <= 0 ) {
		return 0;
	}
	last = o->contours[o->contourCount - 1].points[o->contours[o->contourCount - 1].count - 1];
	end = GL_FontPoint( to, o->emScale );
	mid.x = ( last.x + end.x ) * 0.5f;
	mid.y = ( last.y + end.y ) * 0.5f;
	if ( !GL_AtlasAddCurveTri( o->atlas, o->info, last, mid, end ) ) {
		return 0;
	}
	GL_OutlineAddPoint( o, end );
	return 0;
}

static int GL_OutlineConicTo( const FT_Vector *control, const FT_Vector *to, void *user ) {
	glOutline_t *o = (glOutline_t *)user;
	glPoint2_t p2;
	glPoint2_t p3;
	glPoint2_t last;

	if ( o->contourCount <= 0 || o->contours[o->contourCount - 1].count <= 0 ) {
		return 0;
	}
	last = o->contours[o->contourCount - 1].points[o->contours[o->contourCount - 1].count - 1];
	p2 = GL_FontPoint( control, o->emScale );
	p3 = GL_FontPoint( to, o->emScale );
	if ( !GL_AtlasAddCurveTri( o->atlas, o->info, last, p2, p3 ) ) {
		return 0;
	}
	GL_OutlineAddPoint( o, p3 );
	return 0;
}

static int GL_OutlineCubicTo( const FT_Vector *control1, const FT_Vector *control2,
	const FT_Vector *to, void *user ) {
	glOutline_t *o = (glOutline_t *)user;
	glPoint2_t p0;
	glPoint2_t c1;
	glPoint2_t c2;
	glPoint2_t p3;
	glPoint2_t q1;
	glPoint2_t q2;
	glPoint2_t q3;
	glPoint2_t q4;
	glPoint2_t q5;
	glPoint2_t q6;

	if ( o->contourCount <= 0 || o->contours[o->contourCount - 1].count <= 0 ) {
		return 0;
	}
	p0 = o->contours[o->contourCount - 1].points[o->contours[o->contourCount - 1].count - 1];
	c1 = GL_FontPoint( control1, o->emScale );
	c2 = GL_FontPoint( control2, o->emScale );
	p3 = GL_FontPoint( to, o->emScale );

	q1.x = ( p0.x + c1.x * 3.0f ) * 0.25f;
	q1.y = ( p0.y + c1.y * 3.0f ) * 0.25f;
	q2.x = ( c1.x * 3.0f + c2.x * 3.0f ) * 0.25f;
	q2.y = ( c1.y * 3.0f + c2.y * 3.0f ) * 0.25f;
	q3.x = ( c2.x * 3.0f + p3.x ) * 0.25f;
	q3.y = ( c2.y * 3.0f + p3.y ) * 0.25f;
	q4.x = ( q1.x + q2.x ) * 0.5f;
	q4.y = ( q1.y + q2.y ) * 0.5f;
	q5.x = ( q2.x + q3.x ) * 0.5f;
	q5.y = ( q2.y + q3.y ) * 0.5f;
	q6.x = ( q4.x + q5.x ) * 0.5f;
	q6.y = ( q4.y + q5.y ) * 0.5f;

	if ( !GL_AtlasAddCurveTri( o->atlas, o->info, p0, q1, q4 ) ||
		!GL_AtlasAddCurveTri( o->atlas, o->info, q4, q2, q6 ) ||
		!GL_AtlasAddCurveTri( o->atlas, o->info, q6, q3, p3 ) ) {
		return 0;
	}
	GL_OutlineAddPoint( o, p3 );
	return 0;
}

void R_VectorGlyphletAtlas_Init( vectorGlyphletAtlas_t *atlas ) {
	if ( !atlas ) {
		return;
	}
	Com_Memset( atlas, 0, sizeof( *atlas ) );
}

void R_VectorGlyphletAtlas_Shutdown( vectorGlyphletAtlas_t *atlas ) {
	R_VectorGlyphletAtlas_Clear( atlas );
}

void R_VectorGlyphletAtlas_Clear( vectorGlyphletAtlas_t *atlas ) {
	if ( !atlas ) {
		return;
	}
	if ( atlas->verts ) {
		Z_Free( atlas->verts );
	}
	if ( atlas->indices ) {
		Z_Free( atlas->indices );
	}
	if ( atlas->primTypes ) {
		Z_Free( atlas->primTypes );
	}
	Com_Memset( atlas, 0, sizeof( *atlas ) );
}

qboolean R_VectorGlyphlet_BuildFromSlot( FT_GlyphSlot slot, vectorGlyphletAtlas_t *atlas,
	vectorGlyphletInfo_t *info ) {
	FT_Outline_Funcs funcs;
	glOutline_t outline;
	glContour_t poly;
	int c;
	uint32_t solidVertBase;
	int tris;

	if ( !slot || !atlas || !info ) {
		return qfalse;
	}
	if ( slot->format != ft_glyph_format_outline ) {
		return qfalse;
	}

	Com_Memset( info, 0, sizeof( *info ) );
	info->vertexBaseIndex = atlas->vertCount;
	info->triangleBaseIndex = atlas->indexCount;
	info->primBaseIndex = atlas->primCount;

	Com_Memset( &outline, 0, sizeof( outline ) );
	outline.emScale = 1.0f / (float)( slot->face ? slot->face->units_per_EM : 2048 );
	outline.atlas = atlas;
	outline.info = info;

	Com_Memset( &funcs, 0, sizeof( funcs ) );
	funcs.move_to = GL_OutlineMoveTo;
	funcs.line_to = GL_OutlineLineTo;
	funcs.conic_to = GL_OutlineConicTo;
	funcs.cubic_to = GL_OutlineCubicTo;

	if ( FT_Outline_Decompose( &slot->outline, &funcs, &outline ) != 0 ) {
		return qfalse;
	}

	for ( c = 0; c < outline.contourCount; c++ ) {
		poly = outline.contours[c];
		if ( poly.count < 3 ) {
			continue;
		}

		solidVertBase = atlas->vertCount;
		for ( tris = 0; tris < poly.count; tris++ ) {
			if ( !GL_AtlasAddVertex( atlas, poly.points[tris].x, poly.points[tris].y, 0.0f, 0.0f ) ) {
				return qfalse;
			}
		}

		if ( GL_Area2( &poly ) < 0.0f ) {
			/* Clockwise hole contour: reverse winding for ear clip. */
			int i;
			glPoint2_t tmp;
			for ( i = 0; i < poly.count / 2; i++ ) {
				tmp = poly.points[i];
				poly.points[i] = poly.points[poly.count - 1 - i];
				poly.points[poly.count - 1 - i] = tmp;
			}
		}

		GL_TriangulateContour( &poly, solidVertBase, atlas, info );
	}

	info->vertexCount = atlas->vertCount - info->vertexBaseIndex;
	return ( info->primitiveCount > 0u ) ? qtrue : qfalse;
}

#endif /* BUILD_FREETYPE */
