/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Glyphlet atlas stubs when BUILD_FREETYPE is off.
===========================================================================
*/

#include "tr_vector_font_glyphlet.h"

#if !defined(BUILD_FREETYPE)

void R_VectorGlyphletAtlas_Init( vectorGlyphletAtlas_t *atlas )
{
	(void)atlas;
}

void R_VectorGlyphletAtlas_Shutdown( vectorGlyphletAtlas_t *atlas )
{
	(void)atlas;
}

void R_VectorGlyphletAtlas_Clear( vectorGlyphletAtlas_t *atlas )
{
	(void)atlas;
}

#endif /* !BUILD_FREETYPE */
