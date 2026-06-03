/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

GPU vector font rendering from TrueType glyph outlines (Lengyel, JCGT 2017).
Quadratic Bezier curves are uploaded to a float curve texture; the Vulkan
uiVectorText shader evaluates robust winding-number coverage per pixel.
===========================================================================
*/

#ifndef TR_VECTOR_FONT_H
#define TR_VECTOR_FONT_H

#include "../../qcommon/q_shared.h"

#define VECTOR_CURVE_TEX_WIDTH 4096
#define VECTOR_TEXELS_PER_CURVE 2

typedef struct {
	int     curveStart;
	int     curveCount;
	float   emLeft;
	float   emBottom;
	float   emRight;
	float   emTop;
	float   xAdvance;
	qboolean valid;
} vectorFontGlyph_t;

typedef struct {
	qboolean            loaded;
	char                ttfPath[MAX_QPATH];
	struct image_s     *curveImage;
	int                 curveTexWidth;
	int                 curveTexHeight;
	int                 totalCurves;
	vectorFontGlyph_t   glyphs[GLYPHS_PER_FONT];
} vectorFont_t;

void            R_VectorFont_Init( void );
void            R_VectorFont_Shutdown( void );
qboolean        R_VectorFont_IsEnabled( void );
qboolean        R_VectorFont_Load( const char *ttfPath );
void            R_VectorFont_Clear( void );
const vectorFontGlyph_t *R_VectorFont_GetGlyph( int ch );
int             R_VectorFont_CurveTexWidth( void );
struct image_s *R_VectorFont_GetCurveImage( void );
qhandle_t       R_VectorFont_Shader( void );
void            R_VectorFont_SetShader( qhandle_t shader );

qboolean        R_VectorFont_DrawString( float x, float y, float scale, const char *text,
	const float *color, float shadowOff );

#endif /* TR_VECTOR_FONT_H */
