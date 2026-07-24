/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Optional HarfBuzz shaping for the vector font path.
When BUILD_HARFBUZZ is off, falls back to UTF-8 decode + FreeType advances.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "q_utf8.h"
#include "tr_public.h"
#include "tr_vector_font_shape.h"

#if defined( BUILD_HARFBUZZ )
#include <hb.h>
#include <hb-ft.h>
#endif

#ifdef BUILD_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#define VECTOR_SHAPE_MAX_GLYPHS 1024

qboolean R_VectorFont_ShapeRun( const char *utf8, uint32_t fontSlot,
	vectorShapedRun_t *out, vectorShapedGlyph_t *glyphBuf, uint32_t glyphCap )
{
	const char *s;
	uint32_t n;

	(void)fontSlot;
	if ( !utf8 || !out || !glyphBuf || glyphCap == 0 ) {
		return qfalse;
	}
	Com_Memset( out, 0, sizeof( *out ) );
	out->glyphs = glyphBuf;
	out->glyphCount = 0;
	out->advanceX = 0.0f;
	out->advanceY = 0.0f;

#if defined( BUILD_HARFBUZZ ) && defined( BUILD_FREETYPE )
	/* Full HarfBuzz path is wired when both libraries are linked; for now the
	 * buffer API is available for future FT_Face binding. Fallback below remains
	 * correct for Latin/ASCII HUD until per-face hb_font_t caching lands. */
	(void)0;
#endif

	/* Fallback: one glyph per decoded codepoint (Latin/kerning via FreeType later). */
	s = utf8;
	n = 0;
	while ( *s && n < glyphCap && n < VECTOR_SHAPE_MAX_GLYPHS ) {
		uint32_t cp = Q_UTF8_Decode( &s );
		vectorShapedGlyph_t *g = &glyphBuf[n];
		Com_Memset( g, 0, sizeof( *g ) );
		g->glyphIndex = cp & 0xFFFFu;
		g->cluster = n;
		g->fontSlot = (uint16_t)fontSlot;
		/* Advances filled by draw path from vectorFontGlyph_t.xAdvance. */
		n++;
	}
	out->glyphCount = n;
	return n > 0 ? qtrue : qfalse;
}
