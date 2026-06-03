/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CPU-side TrueType outline extraction and curve texture packing for the
Lengyel (JCGT 2017) GPU vector font path.
===========================================================================
*/

#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#include "../../qcommon/q_utf8.h"
#include "tr_public.h"
#include "tr_vector_font.h"

#if defined(RENDERER_VULKAN)
#include "../vulkan/tr_common.h"
#include "../vulkan/tr_local.h"
#include "../vulkan/vk_texture_image.h"
#else
#error "tr_vector_font.c requires RENDERER_VULKAN"
#endif

#ifdef BUILD_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

extern FT_Library ftLibrary;
#endif

static vectorFont_t vectorFont;
static qhandle_t vectorFontShaderHandle = 0;

void RE_SetColor( const float *rgba );
void RE_DrawVectorGlyph( float x, float y, float w, float h,
	float emS1, float emT1, float emS2, float emT2,
	int curveStart, int curveCount );

typedef struct {
	float x;
	float y;
} vec2f_t;

typedef struct {
	vec2f_t p1;
	vec2f_t p2;
	vec2f_t p3;
} vecCurveSeg_t;

#define VECTOR_MAX_CURVE_SEGS 8192

typedef struct {
	vecCurveSeg_t   curves[VECTOR_MAX_CURVE_SEGS];
	int             numCurves;
	vec2f_t         moveTo;
	vec2f_t         lastPoint;
	qboolean        hasMove;
	float           emScale;
} vecOutlineBuilder_t;

static float VecCurve_MaxX( const vecCurveSeg_t *c ) {
	float m = c->p1.x;
	if ( c->p2.x > m ) m = c->p2.x;
	if ( c->p3.x > m ) m = c->p3.x;
	return m;
}

static float VecCurve_MaxY( const vecCurveSeg_t *c ) {
	float m = c->p1.y;
	if ( c->p2.y > m ) m = c->p2.y;
	if ( c->p3.y > m ) m = c->p3.y;
	return m;
}

static void VecOutline_SortCurves( vecOutlineBuilder_t *b ) {
	int i;
	int j;
	for ( i = 0; i < b->numCurves - 1; i++ ) {
		for ( j = i + 1; j < b->numCurves; j++ ) {
			if ( VecCurve_MaxX( &b->curves[j] ) > VecCurve_MaxX( &b->curves[i] ) ) {
				vecCurveSeg_t tmp = b->curves[i];
				b->curves[i] = b->curves[j];
				b->curves[j] = tmp;
			}
		}
	}
}

#ifdef BUILD_FREETYPE

static void VecOutline_Reset( vecOutlineBuilder_t *b, float emScale ) {
	Com_Memset( b, 0, sizeof( *b ) );
	b->emScale = emScale;
}

static vec2f_t VecOutline_FontPoint( const FT_Vector *v, float emScale ) {
	vec2f_t p;
	p.x = (float)v->x * emScale;
	p.y = (float)v->y * emScale;
	return p;
}

static void VecOutline_AddCurve( vecOutlineBuilder_t *b, vec2f_t p1, vec2f_t p2, vec2f_t p3 ) {
	vecCurveSeg_t *c;

	if ( b->numCurves >= VECTOR_MAX_CURVE_SEGS ) {
		return;
	}
	c = &b->curves[b->numCurves++];
	c->p1 = p1;
	c->p2 = p2;
	c->p3 = p3;
}

static int VecOutline_MoveTo( const FT_Vector *to, void *user ) {
	vecOutlineBuilder_t *b = (vecOutlineBuilder_t *)user;
	b->moveTo = VecOutline_FontPoint( to, b->emScale );
	b->lastPoint = b->moveTo;
	b->hasMove = qtrue;
	return 0;
}

static int VecOutline_LineTo( const FT_Vector *to, void *user ) {
	vecOutlineBuilder_t *b = (vecOutlineBuilder_t *)user;
	vec2f_t end;
	vec2f_t mid;

	end = VecOutline_FontPoint( to, b->emScale );
	mid.x = ( b->lastPoint.x + end.x ) * 0.5f;
	mid.y = ( b->lastPoint.y + end.y ) * 0.5f;
	VecOutline_AddCurve( b, b->lastPoint, mid, end );
	b->lastPoint = end;
	return 0;
}

static int VecOutline_ConicTo( const FT_Vector *control, const FT_Vector *to, void *user ) {
	vecOutlineBuilder_t *b = (vecOutlineBuilder_t *)user;
	vec2f_t p2;
	vec2f_t p3;

	p2 = VecOutline_FontPoint( control, b->emScale );
	p3 = VecOutline_FontPoint( to, b->emScale );
	VecOutline_AddCurve( b, b->lastPoint, p2, p3 );
	b->lastPoint = p3;
	return 0;
}

static int VecOutline_CubicTo( const FT_Vector *control1, const FT_Vector *control2,
	const FT_Vector *to, void *user ) {
	vecOutlineBuilder_t *b = (vecOutlineBuilder_t *)user;
	vec2f_t p0;
	vec2f_t c1;
	vec2f_t c2;
	vec2f_t p3;
	vec2f_t q1;
	vec2f_t q2;
	vec2f_t q3;
	vec2f_t q4;
	vec2f_t q5;
	vec2f_t q6;

	p0 = b->lastPoint;
	c1 = VecOutline_FontPoint( control1, b->emScale );
	c2 = VecOutline_FontPoint( control2, b->emScale );
	p3 = VecOutline_FontPoint( to, b->emScale );

	/* Approximate cubic with two quadratics (midpoint split). */
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

	VecOutline_AddCurve( b, p0, q1, q4 );
	VecOutline_AddCurve( b, q4, q2, q6 );
	VecOutline_AddCurve( b, q6, q3, p3 );
	b->lastPoint = p3;
	return 0;
}

static qboolean VecOutline_FromGlyph( FT_GlyphSlot slot, vecOutlineBuilder_t *b ) {
	FT_Outline_Funcs funcs;
	FT_Outline *outline;

	VecOutline_Reset( b, b->emScale );
	if ( slot->format != ft_glyph_format_outline ) {
		return qfalse;
	}

	outline = &slot->outline;
	Com_Memset( &funcs, 0, sizeof( funcs ) );
	funcs.move_to = VecOutline_MoveTo;
	funcs.line_to = VecOutline_LineTo;
	funcs.conic_to = VecOutline_ConicTo;
	funcs.cubic_to = VecOutline_CubicTo;

	if ( FT_Outline_Decompose( outline, &funcs, b ) != 0 ) {
		return qfalse;
	}
	return ( b->numCurves > 0 ) ? qtrue : qfalse;
}


static qboolean R_VectorFont_BuildFromFace( FT_Face face, const char *ttfPath ) {
	float *texels;
	int texelCapacity;
	int texelCount;
	int width;
	int height;
	int ch;
	char imageName[MAX_QPATH];
	unsigned long h;
	image_t *image;
	FT_Int32 loadFlags = FT_LOAD_NO_BITMAP | FT_LOAD_NO_SCALE;

	h = Com_GenerateHashValue( ttfPath, 256 );
	Com_sprintf( imageName, sizeof( imageName ), "fonts/_vcur_%lu", h );

	texelCapacity = VECTOR_MAX_CURVE_SEGS * GLYPHS_PER_FONT * VECTOR_TEXELS_PER_CURVE;
	texels = ri.Malloc( (size_t)texelCapacity * 4 * sizeof( *texels ) );
	if ( !texels ) {
		return qfalse;
	}
	Com_Memset( texels, 0, (size_t)texelCapacity * 4 * sizeof( *texels ) );

	R_VectorFont_Clear();
	Q_strncpyz( vectorFont.ttfPath, ttfPath, sizeof( vectorFont.ttfPath ) );
	texelCount = 0;

	for ( ch = GLYPH_START; ch <= GLYPH_END; ch++ ) {
		vecOutlineBuilder_t builder;
		vectorFontGlyph_t *g;
		FT_UInt glyphIndex;
		FT_GlyphSlot slot;
		float emLeft;
		float emRight;
		float emTop;
		float emBottom;
		int c;

		g = &vectorFont.glyphs[ch];
		g->valid = qfalse;

		glyphIndex = FT_Get_Char_Index( face, (FT_ULong)ch );
		if ( glyphIndex == 0 && ch != '?' ) {
			continue;
		}
		if ( FT_Load_Glyph( face, glyphIndex, loadFlags ) != 0 ) {
			continue;
		}

		slot = face->glyph;
		builder.emScale = 1.0f / (float)face->units_per_EM;
		if ( !VecOutline_FromGlyph( slot, &builder ) ) {
			continue;
		}
		VecOutline_SortCurves( &builder );

		if ( texelCount + builder.numCurves * VECTOR_TEXELS_PER_CURVE > texelCapacity ) {
			ri.Printf( PRINT_WARNING, "R_VectorFont_BuildFromFace: curve texture overflow for '%c'\n", ch );
			break;
		}

		g->curveStart = texelCount;
		g->curveCount = builder.numCurves;
		g->xAdvance = (float)slot->advance.x * builder.emScale;

		emLeft = emRight = builder.curves[0].p1.x;
		emTop = emBottom = builder.curves[0].p1.y;
		for ( c = 0; c < builder.numCurves; c++ ) {
			const vecCurveSeg_t *cv = &builder.curves[c];
			const vec2f_t *pts[3] = { &cv->p1, &cv->p2, &cv->p3 };
			int p;

			for ( p = 0; p < 3; p++ ) {
				if ( pts[p]->x < emLeft ) emLeft = pts[p]->x;
				if ( pts[p]->x > emRight ) emRight = pts[p]->x;
				if ( pts[p]->y < emBottom ) emBottom = pts[p]->y;
				if ( pts[p]->y > emTop ) emTop = pts[p]->y;
			}

			texels[texelCount * 4 + 0] = cv->p1.x;
			texels[texelCount * 4 + 1] = cv->p1.y;
			texels[texelCount * 4 + 2] = cv->p2.x;
			texels[texelCount * 4 + 3] = cv->p2.y;
			texelCount++;

			texels[texelCount * 4 + 0] = cv->p3.x;
			texels[texelCount * 4 + 1] = cv->p3.y;
			texels[texelCount * 4 + 2] = 0.0f;
			texels[texelCount * 4 + 3] = 0.0f;
			texelCount++;
		}

		g->emLeft = emLeft;
		g->emBottom = emBottom;
		g->emRight = emRight;
		g->emTop = emTop;
		g->valid = qtrue;
	}

	width = VECTOR_CURVE_TEX_WIDTH;
	height = ( texelCount + width - 1 ) / width;
	if ( height < 1 ) {
		height = 1;
	}

	image = R_CreateImageRGBA32F( imageName, texels, width, height, IMGFLAG_CLAMPTOEDGE | IMGFLAG_NOSCALE );
	vectorFont.curveImage = image;
	vectorFont.curveTexWidth = width;
	vectorFont.curveTexHeight = height;
	vectorFont.totalCurves = texelCount / VECTOR_TEXELS_PER_CURVE;
	vectorFont.loaded = qtrue;

	ri.Printf( PRINT_ALL, "Vector font: loaded %s (%d curves, %dx%d curve texture)\n",
		ttfPath, vectorFont.totalCurves, width, height );

	ri.Free( texels );
	return qtrue;
}

#endif /* BUILD_FREETYPE */

void R_VectorFont_Init( void ) {
	R_VectorFont_Clear();
}

void R_VectorFont_Shutdown( void ) {
	R_VectorFont_Clear();
}

qboolean R_VectorFont_IsEnabled( void ) {
	return vectorFont.loaded;
}

void R_VectorFont_Clear( void ) {
	Com_Memset( &vectorFont, 0, sizeof( vectorFont ) );
}

qboolean R_VectorFont_Load( const char *ttfPath ) {
#ifdef BUILD_FREETYPE
	FT_Face face;
	FT_Error err;
	void *faceData;
	int len;
	qboolean ok;

	if ( !ttfPath || !ttfPath[0] ) {
		return qfalse;
	}
	if ( vectorFont.loaded && !Q_stricmp( vectorFont.ttfPath, ttfPath ) ) {
		return qtrue;
	}

	if ( ftLibrary == NULL ) {
		err = FT_Init_FreeType( &ftLibrary );
		if ( err != 0 ) {
			ri.Printf( PRINT_WARNING, "R_VectorFont_Load: FT_Init_FreeType failed (%d)\n", (int)err );
			return qfalse;
		}
	}

	len = ri.FS_ReadFile( ttfPath, &faceData );
	if ( len <= 0 ) {
		ri.Printf( PRINT_WARNING, "R_VectorFont_Load: could not read '%s'\n", ttfPath );
		return qfalse;
	}

	err = FT_New_Memory_Face( ftLibrary, (const FT_Byte *)faceData, (FT_Long)len, 0, &face );
	if ( err != 0 ) {
		ri.Printf( PRINT_WARNING, "R_VectorFont_Load: FT_New_Memory_Face failed for '%s' (%d)\n", ttfPath, (int)err );
		ri.FS_FreeFile( faceData );
		return qfalse;
	}

	ok = R_VectorFont_BuildFromFace( face, ttfPath );
	FT_Done_Face( face );
	ri.FS_FreeFile( faceData );
	return ok;
#else
	(void)ttfPath;
	return qfalse;
#endif
}

const vectorFontGlyph_t *R_VectorFont_GetGlyph( int ch ) {
	ch &= 255;
	if ( !vectorFont.loaded || !vectorFont.glyphs[ch].valid ) {
		return vectorFont.glyphs['?'].valid ? &vectorFont.glyphs['?'] : NULL;
	}
	return &vectorFont.glyphs[ch];
}

qhandle_t R_VectorFont_Shader( void ) {
	return vectorFontShaderHandle;
}

void R_VectorFont_SetShader( qhandle_t shader ) {
	vectorFontShaderHandle = shader;
}

int R_VectorFont_CurveTexWidth( void ) {
	return vectorFont.curveTexWidth;
}

image_t *R_VectorFont_GetCurveImage( void ) {
	return vectorFont.curveImage;
}

static void R_VectorFont_DrawGlyphInternal( float x, float y, float scale, const vectorFontGlyph_t *g,
	const float *color, float shadowOff, qboolean shadowPass ) {
	float pad;
	float emW;
	float emH;
	float ax;
	float ay;
	float aw;
	float ah;

	if ( !g || !g->valid || scale <= 0.0f ) {
		return;
	}

	pad = 0.5f / scale;
	emW = ( g->emRight - g->emLeft ) + pad * 2.0f;
	emH = ( g->emTop - g->emBottom ) + pad * 2.0f;
	aw = emW * scale;
	ah = emH * scale;
	ax = x + ( g->emLeft - pad ) * scale;
	ay = y - g->emTop * scale - pad * scale;

	if ( shadowPass && shadowOff > 0.0f ) {
		vec4_t shadow = { 0.0f, 0.0f, 0.0f, color ? color[3] : 1.0f };
		RE_SetColor( shadow );
		RE_DrawVectorGlyph( ax + shadowOff, ay + shadowOff, aw, ah,
			g->emLeft - pad, g->emTop + pad, g->emRight + pad, g->emBottom - pad,
			g->curveStart, g->curveCount );
		return;
	}

	if ( color ) {
		RE_SetColor( color );
	}
	RE_DrawVectorGlyph( ax, ay, aw, ah,
		g->emLeft - pad, g->emTop + pad, g->emRight + pad, g->emBottom - pad,
		g->curveStart, g->curveCount );
}

qboolean R_VectorFont_DrawString( float x, float y, float scale, const char *text,
	const float *color, float shadowOff ) {
	const char *s;
	float xx;
	float yy;
	const vectorFontGlyph_t *g;

	if ( !R_VectorFont_IsEnabled() || !text || !text[0] || scale <= 0.0f ) {
		return qfalse;
	}

	if ( shadowOff > 0.0f ) {
		s = text;
		xx = x;
		yy = y;
		while ( *s ) {
			uint32_t cp = Q_UTF8_Decode( &s );
			if ( cp == '\n' ) {
				xx = x;
				yy += scale * 48.0f;
				continue;
			}
			g = R_VectorFont_GetGlyph( (int)cp );
			if ( g ) {
				R_VectorFont_DrawGlyphInternal( xx, yy, scale, g, color, shadowOff, qtrue );
				xx += g->xAdvance * scale;
			}
		}
	}

	s = text;
	xx = x;
	yy = y;
	if ( color ) {
		RE_SetColor( color );
	}
	while ( *s ) {
		uint32_t cp = Q_UTF8_Decode( &s );
		if ( cp == '\n' ) {
			xx = x;
			yy += scale * 48.0f;
			continue;
		}
		g = R_VectorFont_GetGlyph( (int)cp );
		if ( !g ) {
			continue;
		}
		R_VectorFont_DrawGlyphInternal( xx, yy, scale, g, color, shadowOff, qfalse );
		xx += g->xAdvance * scale;
	}
	RE_SetColor( NULL );
	return qtrue;
}

qboolean RE_LoadVectorFont( const char *path ) {
	return R_VectorFont_Load( path );
}

qboolean RE_VectorFontActive( void ) {
	return R_VectorFont_IsEnabled();
}

qboolean RE_DrawVectorString( float x, float y, float scale, const char *text,
	const float *color, float shadowOff ) {
	return R_VectorFont_DrawString( x, y, scale, text, color, shadowOff );
}
