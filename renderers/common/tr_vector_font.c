/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CPU-side TrueType outline extraction and curve texture packing for the
Lengyel (JCGT 2017) GPU vector font path.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "q_utf8.h"
#include "tr_public.h"
#include "tr_vector_font.h"
#include "tr_vector_font_glyphlet.h"

#if defined(RENDERER_VULKAN)
#include "../vulkan/tr_common.h"
#include "../vulkan/tr_local.h"
#include "../vulkan/vk_texture_image.h"
#include "../vulkan/vk_vector_font.h"
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
static vectorGlyphletAtlas_t vectorGlyphletAtlas;
static qhandle_t vectorFontShaderHandle = 0;
static cvar_t *r_vectorFontMode;
static cvar_t *r_vectorFontCoverage;
static cvar_t *r_vectorFontHinting;
static cvar_t *r_vectorFontStemDarkening;
static cvar_t *r_vectorFontPixelSnap;
static cvar_t *r_vectorFontDebug;

#define VECTOR_FONT_MODE_LENGYEL      0
#define VECTOR_FONT_MODE_LOOP_BLINN   2

static int R_VectorFont_EffectiveMode( void ) {
	int mode;

	if ( !r_vectorFontMode ) {
		return VECTOR_FONT_MODE_LENGYEL;
	}
	mode = r_vectorFontMode->integer;
	if ( mode == VECTOR_FONT_MODE_LOOP_BLINN ) {
		return VECTOR_FONT_MODE_LOOP_BLINN;
	}
	return VECTOR_FONT_MODE_LENGYEL;
}

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
#define VECTOR_INITIAL_TEXELS 4096
#define VECTOR_MAX_PACKED_TEXELS ( 16 * 1024 * 1024 )

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

static void VecOutline_SortCurvesByMaxX( vecOutlineBuilder_t *b ) {
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

static void VecOutline_SortCurvesByMaxY( vecOutlineBuilder_t *b ) {
	int i;
	int j;
	for ( i = 0; i < b->numCurves - 1; i++ ) {
		for ( j = i + 1; j < b->numCurves; j++ ) {
			if ( VecCurve_MaxY( &b->curves[j] ) > VecCurve_MaxY( &b->curves[i] ) ) {
				vecCurveSeg_t tmp = b->curves[i];
				b->curves[i] = b->curves[j];
				b->curves[j] = tmp;
			}
		}
	}
}

static void VecOutline_PackCurves( float *texels, int *texelCount, const vecCurveSeg_t *curves, int numCurves ) {
	int c;
	for ( c = 0; c < numCurves; c++ ) {
		const vecCurveSeg_t *cv = &curves[c];
		texels[*texelCount * 4 + 0] = cv->p1.x;
		texels[*texelCount * 4 + 1] = cv->p1.y;
		texels[*texelCount * 4 + 2] = cv->p2.x;
		texels[*texelCount * 4 + 3] = cv->p2.y;
		( *texelCount )++;
		texels[*texelCount * 4 + 0] = cv->p3.x;
		texels[*texelCount * 4 + 1] = cv->p3.y;
		texels[*texelCount * 4 + 2] = 0.0f;
		texels[*texelCount * 4 + 3] = 0.0f;
		( *texelCount )++;
	}
}

static qboolean VecOutline_EnsureTexelCapacity( float **texels, int *capacity, int required ) {
	float *grown;
	int newCapacity;

	if ( required <= *capacity ) {
		return qtrue;
	}
	if ( required < 0 || required > VECTOR_MAX_PACKED_TEXELS ) {
		return qfalse;
	}

	newCapacity = *capacity;
	while ( newCapacity < required ) {
		if ( newCapacity > VECTOR_MAX_PACKED_TEXELS / 2 ) {
			newCapacity = VECTOR_MAX_PACKED_TEXELS;
			break;
		}
		newCapacity *= 2;
	}

	grown = ri.Malloc( (size_t)newCapacity * 4u * sizeof( *grown ) );
	if ( !grown ) {
		return qfalse;
	}
	Com_Memset( grown, 0, (size_t)newCapacity * 4u * sizeof( *grown ) );
	if ( *texels ) {
		Com_Memcpy( grown, *texels, (size_t)( *capacity ) * 4u * sizeof( *grown ) );
		ri.Free( *texels );
	}
	*texels = grown;
	*capacity = newCapacity;
	return qtrue;
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

	/*
	 * Grow with actual outline complexity.  Reserving the per-glyph worst
	 * case for all 256 legacy slots consumed 1 GiB after adding the duplicate
	 * Y-sorted list, even for an ordinary Latin HUD font.
	 */
	texelCapacity = VECTOR_INITIAL_TEXELS;
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

		/* Dual sorted lists (Lengyel): X-sorted for horizontal rays, Y-sorted for vertical.
		 * Each list is packed contiguously; shader Y list starts at curveStart + curveCount*2. */
		if ( !VecOutline_EnsureTexelCapacity( &texels, &texelCapacity,
			texelCount + builder.numCurves * VECTOR_TEXELS_PER_CURVE * 2 ) ) {
			ri.Printf( PRINT_WARNING,
				"R_VectorFont_BuildFromFace: packed curve storage limit reached for '%c'\n", ch );
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
		}

		VecOutline_SortCurvesByMaxX( &builder );
		VecOutline_PackCurves( texels, &texelCount, builder.curves, builder.numCurves );
		VecOutline_SortCurvesByMaxY( &builder );
		VecOutline_PackCurves( texels, &texelCount, builder.curves, builder.numCurves );

		g->emLeft = emLeft;
		g->emBottom = emBottom;
		g->emRight = emRight;
		g->emTop = emTop;
		g->valid = qtrue;

		if ( R_VectorFont_EffectiveMode() == VECTOR_FONT_MODE_LOOP_BLINN ) {
			(void)R_VectorGlyphlet_BuildFromSlot( slot, &vectorGlyphletAtlas, &g->glyphlet );
		}
	}

	width = VECTOR_CURVE_TEX_WIDTH;
	height = ( texelCount + width - 1 ) / width;
	if ( height < 1 ) {
		height = 1;
	}
	if ( !VecOutline_EnsureTexelCapacity( &texels, &texelCapacity, width * height ) ) {
		ri.Printf( PRINT_WARNING, "R_VectorFont_BuildFromFace: unable to pad curve texture\n" );
		ri.Free( texels );
		return qfalse;
	}

	image = R_CreateImageRGBA32F( imageName, texels, width, height, IMGFLAG_CLAMPTOEDGE | IMGFLAG_NOSCALE );
	vectorFont.curveImage = image;
	vectorFont.curveTexWidth = width;
	vectorFont.curveTexHeight = height;
	vectorFont.totalCurves = texelCount / ( VECTOR_TEXELS_PER_CURVE * 2 ); /* unique curves (X+Y lists) */
	vectorFont.loaded = qtrue;

	if ( R_VectorFont_EffectiveMode() == VECTOR_FONT_MODE_LOOP_BLINN ) {
		if ( !VK_VectorFont_UploadAtlas( &vectorGlyphletAtlas, vectorFont.glyphs ) ) {
			ri.Printf( PRINT_WARNING,
				"Vector font: mode 2 glyphlet upload failed; set r_vectorFontMode 0 for Lengyel fallback\n" );
		} else {
			ri.Printf( PRINT_ALL,
				"Vector font: loaded %s (%u glyphlet tris, %ux%u curve texture, mode 2 Loop&Blinn%s)\n",
				ttfPath, vectorGlyphletAtlas.primCount, width, height,
				VK_VectorFont_MeshReady() ? " + NV mesh dispatch" : " vertex fallback" );
		}
	} else {
		ri.Printf( PRINT_ALL, "Vector font: loaded %s (%d curves, %dx%d curve texture, mode %d Lengyel)\n",
			ttfPath, vectorFont.totalCurves, width, height, VECTOR_FONT_MODE_LENGYEL );
	}

	ri.Free( texels );
	return qtrue;
}

#endif /* BUILD_FREETYPE */

static void VectorFont_Status_f( void ) {
	ri.Printf( PRINT_ALL,
		"=== vector_font_status ===\n"
		"  loaded=%d path='%s' curves=%d tex=%dx%d mode=%d\n"
		"  coverage=%d hinting=%d stemDarken=%.2f pixelSnap=%d debug=%d\n"
		"  atlas-free: curve control points + dual sorted lists (no SDF/MSDF glyph atlas)\n"
		"  shaping: UTF-8 legacy fallback (HarfBuzz face cache not yet connected)\n"
		"  cubic policy: fixed midpoint quadratic conversion (not certified)\n"
		"  temporal: HUD draws into ui_overlay (post TAA/tonemap)\n",
		vectorFont.loaded ? 1 : 0,
		vectorFont.ttfPath,
		vectorFont.totalCurves,
		vectorFont.curveTexWidth,
		vectorFont.curveTexHeight,
		R_VectorFont_EffectiveMode(),
		r_vectorFontCoverage ? r_vectorFontCoverage->integer : 2,
		r_vectorFontHinting ? r_vectorFontHinting->integer : 0,
		r_vectorFontStemDarkening ? r_vectorFontStemDarkening->value : 0.0f,
		r_vectorFontPixelSnap ? r_vectorFontPixelSnap->integer : 0,
		r_vectorFontDebug ? r_vectorFontDebug->integer : 0 );
}

static void VectorFont_Validate_f( void ) {
	ri.Printf( PRINT_ALL,
		"=== vector_font_validate ===\n"
		"  Lengyel winding: dual-axis X/Y sorted curve lists\n"
		"  Blend: premultiplied (ONE, ONE_MINUS_SRC_ALPHA)\n"
		"  Coverage: r_vectorFontCoverage 0=center 1=dual-axis 2=adaptive-SS(default) 3=ultra\n"
		"  Hinting: r_vectorFontHinting 0=unhinted(world) 1=light 2=native UI ppem\n"
		"  Commands: vector_font_status | vector_font_validate | vector_font_memory_status\n" );
}

static void VectorFont_Certify_f( void ) {
	ri.Printf( PRINT_ALL,
		"=== vector_font_certify ===\n"
		"  state=INCOMPLETE\n"
		"  PASS atlas-free outline extraction, dual-axis curve lists, premultiplied blend\n"
		"  PASS bounded curve storage (grows with actual outline complexity)\n"
		"  BLOCKED HarfBuzz face/font cache and glyph-index outline cache\n"
		"  BLOCKED adaptive-error cubic conversion and numerical reference metrics\n"
		"  BLOCKED hinted per-ppem vector cache, COLR layers, world-space certification\n"
		"  The legacy bitmap/SDF renderers remain the production fallback.\n" );
}

static void VectorFont_Memory_f( void ) {
	size_t curveBytes = (size_t)vectorFont.curveTexWidth * (size_t)vectorFont.curveTexHeight * 16u;
	ri.Printf( PRINT_ALL,
		"=== vector_font_memory_status ===\n"
		"  curveTexture RGBA32F: %dx%d (~%zu bytes)\n"
		"  uniqueCurves=%d (stored twice: X-sorted + Y-sorted)\n"
		"  Note: atlas-free eliminates raster glyph pages; vector+band storage remains.\n",
		vectorFont.curveTexWidth, vectorFont.curveTexHeight, curveBytes,
		vectorFont.totalCurves );
}

void R_VectorFont_Init( void ) {
	r_vectorFontMode = ri.Cvar_Get( "r_vectorFontMode", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_vectorFontMode,
		"Vector font algorithm when r_vectorFont 1: 0 = Lengyel JCGT 2017 (curve texture + winding), "
		"2 = Loop & Blinn glyphlets (AMD GPUOpen; NV mesh dispatch when r_vk_meshShaderNV 1). reloadTtf after change." );
	r_vectorFontCoverage = ri.Cvar_Get( "r_vectorFontCoverage", "2", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_vectorFontCoverage, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_vectorFontCoverage,
		"Analytical coverage: 0=center diagnostic, 1=dual-axis, 2=adaptive boundary SS (production), 3=ultra SS." );
	r_vectorFontHinting = ri.Cvar_Get( "r_vectorFontHinting", "0", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_vectorFontHinting, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_vectorFontHinting,
		"0=unhinted outlines (world-space), 1=light FreeType hinting, 2=native TrueType hinting for screen UI." );
	r_vectorFontStemDarkening = ri.Cvar_Get( "r_vectorFontStemDarkening", "0", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_vectorFontStemDarkening, "0", "1", CV_FLOAT );
	r_vectorFontPixelSnap = ri.Cvar_Get( "r_vectorFontPixelSnap", "0", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_vectorFontPixelSnap, "0", "2", CV_INTEGER );
	r_vectorFontDebug = ri.Cvar_Get( "r_vectorFontDebug", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_vectorFontDebug, "0", "15", CV_INTEGER );

	ri.Cmd_AddCommand( "vector_font_status", VectorFont_Status_f );
	ri.Cmd_AddCommand( "vector_font_validate", VectorFont_Validate_f );
	ri.Cmd_AddCommand( "vector_font_memory_status", VectorFont_Memory_f );
	ri.Cmd_AddCommand( "vector_font_gpu_status", VectorFont_Status_f );
	ri.Cmd_AddCommand( "vector_font_certify", VectorFont_Certify_f );

	R_VectorGlyphletAtlas_Init( &vectorGlyphletAtlas );
	R_VectorFont_Clear();
	VK_VectorFont_Init();
}

int R_VectorFont_Mode( void ) {
	return R_VectorFont_EffectiveMode();
}

void R_VectorFont_Shutdown( void ) {
	ri.Cmd_RemoveCommand( "vector_font_status" );
	ri.Cmd_RemoveCommand( "vector_font_validate" );
	ri.Cmd_RemoveCommand( "vector_font_memory_status" );
	ri.Cmd_RemoveCommand( "vector_font_gpu_status" );
	ri.Cmd_RemoveCommand( "vector_font_certify" );
	VK_VectorFont_Shutdown();
	R_VectorGlyphletAtlas_Shutdown( &vectorGlyphletAtlas );
	R_VectorFont_Clear();
}

qboolean R_VectorFont_IsEnabled( void ) {
	return vectorFont.loaded;
}

void R_VectorFont_Clear( void ) {
	VK_VectorFont_ClearGpu();
	R_VectorGlyphletAtlas_Clear( &vectorGlyphletAtlas );
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

	if ( r_vectorFontPixelSnap && r_vectorFontPixelSnap->integer >= 1 ) {
		y = (float)floor( (double)y + 0.5 );
		if ( r_vectorFontPixelSnap->integer >= 2 ) {
			x = (float)floor( (double)x + 0.5 );
		}
	}

	if ( R_VectorFont_EffectiveMode() == VECTOR_FONT_MODE_LOOP_BLINN ) {
		return RE_QueueVectorFontString( x, y, scale, text, color, shadowOff );
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
