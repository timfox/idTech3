/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vector font stubs for non-FreeType builds (BUILD_FREETYPE off).
GPU vector text requires FreeType outline extraction at load time.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "tr_public.h"
#include "tr_vector_font.h"
#include "tr_vector_font_glyphlet.h"

#if !defined(BUILD_FREETYPE)

static cvar_t *r_vectorFontMode;
static vectorGlyphletAtlas_t s_glyphletAtlasStub;

void R_VectorFont_Init( void )
{
	static qboolean s_logged;

	r_vectorFontMode = ri.Cvar_Get( "r_vectorFontMode", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_vectorFontMode,
		"Vector font algorithm (requires BUILD_FREETYPE=ON). 0=Lengyel, 2=Loop&Blinn glyphlets." );
	R_VectorGlyphletAtlas_Init( &s_glyphletAtlasStub );
	if ( !s_logged ) {
		ri.Printf( PRINT_ALL,
			"[VectorFont] stub (build with -DBUILD_FREETYPE=ON; r_vectorFont disabled)\n" );
		s_logged = qtrue;
	}
}

int R_VectorFont_Mode( void )
{
	return r_vectorFontMode ? r_vectorFontMode->integer : 0;
}

void R_VectorFont_Shutdown( void )
{
	R_VectorFont_Clear();
}

qboolean R_VectorFont_IsEnabled( void )
{
	return qfalse;
}

void R_VectorFont_Clear( void ) {}

qboolean R_VectorFont_Load( const char *ttfPath )
{
	(void)ttfPath;
	return qfalse;
}

const vectorFontGlyph_t *R_VectorFont_GetGlyph( int ch )
{
	(void)ch;
	return NULL;
}

qhandle_t R_VectorFont_Shader( void )
{
	return 0;
}

void R_VectorFont_SetShader( qhandle_t shader )
{
	(void)shader;
}

int R_VectorFont_CurveTexWidth( void )
{
	return VECTOR_CURVE_TEX_WIDTH;
}

struct image_s *R_VectorFont_GetCurveImage( void )
{
	return NULL;
}

qboolean R_VectorFont_DrawString( float x, float y, float scale, const char *text,
	const float *color, float shadowOff )
{
	(void)x;
	(void)y;
	(void)scale;
	(void)text;
	(void)color;
	(void)shadowOff;
	return qfalse;
}

qboolean RE_LoadVectorFont( const char *path )
{
	(void)path;
	return qfalse;
}

qboolean RE_VectorFontActive( void )
{
	return qfalse;
}

qboolean RE_DrawVectorString( float x, float y, float scale, const char *text,
	const float *color, float shadowOff )
{
	(void)x;
	(void)y;
	(void)scale;
	(void)text;
	(void)color;
	(void)shadowOff;
	return qfalse;
}

#endif /* !BUILD_FREETYPE */
