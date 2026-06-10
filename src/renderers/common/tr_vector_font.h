/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

GPU vector font rendering from TrueType glyph outlines.

Mode 0 (default, r_vectorFontMode): Lengyel, JCGT 2017 — curve texture +
winding-number coverage (frag_ui_vector_text).

Mode 2 (planned): Loop & Blinn 2005 + mesh-shader glyphlets per AMD GPUOpen
https://gpuopen.com/learn/mesh_shaders/mesh_shaders-font-rendering/
See docs/VECTOR_FONT.md and docs/research/amd-gpuopen-loop-blinn-mesh-fonts.md
===========================================================================
*/

#ifndef TR_VECTOR_FONT_H
#define TR_VECTOR_FONT_H

#include "../../qcommon/q_shared.h"

#define VECTOR_CURVE_TEX_WIDTH 4096
#define VECTOR_TEXELS_PER_CURVE 2

/* Loop & Blinn / AMD glyphlet (mesh-shader path, not built yet). */
#define VECTOR_GLYPHLET_TRI_SOLID   0u
#define VECTOR_GLYPHLET_TRI_CONVEX  1u
#define VECTOR_GLYPHLET_TRI_CONCAVE 2u
#define VECTOR_MAX_GLYPHLET_TRIS    128
#define VECTOR_MAX_GLYPHLET_VERTS   64

typedef struct {
	unsigned int vertexBaseIndex;
	unsigned int triangleBaseIndex;
	unsigned int primBaseIndex;
	unsigned int vertexCount;
	unsigned int primitiveCount;
} vectorGlyphletInfo_t;

typedef struct {
	int     curveStart;
	int     curveCount;
	float   emLeft;
	float   emBottom;
	float   emRight;
	float   emTop;
	float   xAdvance;
	qboolean valid;
	vectorGlyphletInfo_t glyphlet;
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
int             R_VectorFont_Mode( void );
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
