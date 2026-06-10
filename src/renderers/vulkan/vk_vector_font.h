/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan Loop & Blinn glyphlet draw path (r_vectorFontMode 2).
===========================================================================
*/

#ifndef VK_VECTOR_FONT_H
#define VK_VECTOR_FONT_H

#include "../../qcommon/q_shared.h"
#include "../common/tr_vector_font.h"

#include "../common/tr_vector_font_glyphlet.h"

void            VK_VectorFont_Init( void );
void            VK_VectorFont_Shutdown( void );
void            VK_VectorFont_ClearGpu( void );
qboolean        VK_VectorFont_UploadAtlas( vectorGlyphletAtlas_t *atlas,
	vectorFontGlyph_t *glyphs );
qboolean        VK_VectorFont_MeshReady( void );
qboolean        VK_VectorFont_DrawString( float x, float y, float scale, const char *text,
	const float *color, float shadowOff );

#endif /* VK_VECTOR_FONT_H */
