/*
===========================================================================
Unicode glyph mapping support for modern font rendering
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"
#include "../renderer/tr_common.h"

// Font CVars are accessed via ri.Cvar_Get

// Renderer import interface - defined in renderer main file
extern refimport_t ri;
// Renderer import interface - defined in renderer main file

#ifdef USE_FREETYPE
// FreeType types are declared in qcommon.h

// Unicode code point to glyph index mapping cache
#define MAX_UNICODE_CACHE 1024
typedef struct {
	unsigned int codePoint;
	unsigned int glyphIndex;
	qboolean valid;
} unicodeGlyphCache_t;

static unicodeGlyphCache_t unicodeCache[MAX_UNICODE_CACHE];
static int unicodeCacheCount = 0;

/*
=================
RE_GetGlyphIndexForUnicode
=================
Get glyph index for a Unicode code point
Returns glyph index or 0 if not found
=================
*/
unsigned int RE_GetGlyphIndexForUnicode(fontInfo_t *font, unsigned int codePoint)
{
	if (!font || codePoint == 0)
		return 0;
	
	// Check cache first
	int i;
	for (i = 0; i < unicodeCacheCount; i++) {
		if (unicodeCache[i].valid && 
		    unicodeCache[i].codePoint == codePoint) {
			return unicodeCache[i].glyphIndex;
		}
	}
	
	// For ASCII range, use direct mapping
	if (codePoint < 256) {
		return codePoint;
	}
	
	// For Unicode, we need FreeType to get the glyph index
	// This requires access to the FT_Face, which is stored during font registration
	// For now, return 0 (missing glyph) - full implementation would need FT_Face storage
	
	// TODO: Store FT_Face in fontInfo_t or maintain a mapping
	// For now, return 0 to indicate missing glyph
	return 0;
}

/*
=================
RE_FindUnicodeGlyphInFont
=================
Find a Unicode glyph in a font, checking fallback chain if needed
Returns glyphInfo_t* or NULL if not found
=================
*/
glyphInfo_t *RE_FindUnicodeGlyphInFont(fontInfo_t *font, unsigned int codePoint)
{
	if (!font || codePoint == 0)
		return NULL;
	
	// For ASCII range, use direct lookup
	if (codePoint < 256) {
		glyphInfo_t *glyph = &font->glyphs[codePoint];
		if (glyph->glyph != 0) {
			return glyph;
		}
		
		// Check fallback chain
		if (font->fallbackFont) {
			return RE_FindUnicodeGlyphInFont(font->fallbackFont, codePoint);
		}
		
		return NULL;
	}
	
	// For Unicode beyond ASCII, we need glyph mapping
	// This requires FreeType integration
	// For now, check fallback font if available
	if (font->fallbackFont) {
		return RE_FindUnicodeGlyphInFont(font->fallbackFont, codePoint);
	}
	
	return NULL;
}

/*
=================
RE_CacheUnicodeGlyph
=================
Cache a Unicode code point to glyph index mapping
=================
*/
void RE_CacheUnicodeGlyph(unsigned int codePoint, unsigned int glyphIndex)
{
	if (unicodeCacheCount >= MAX_UNICODE_CACHE) {
		// Cache full - could implement LRU eviction here
		return;
	}
	
	unicodeCache[unicodeCacheCount].codePoint = codePoint;
	unicodeCache[unicodeCacheCount].glyphIndex = glyphIndex;
	unicodeCache[unicodeCacheCount].valid = qtrue;
	unicodeCacheCount++;
}

/*
=================
RE_ClearUnicodeCache
=================
Clear the Unicode glyph cache
=================
*/
void RE_ClearUnicodeCache(void)
{
	unicodeCacheCount = 0;
	int i;
	for (i = 0; i < MAX_UNICODE_CACHE; i++) {
		unicodeCache[i].valid = qfalse;
	}
}

#else // !USE_FREETYPE

// Stub implementations when FreeType is not available
unsigned int RE_GetGlyphIndexForUnicode(fontInfo_t *font, unsigned int codePoint)
{
	(void)font;
	(void)codePoint;
	return 0;
}

glyphInfo_t *RE_FindUnicodeGlyphInFont(fontInfo_t *font, unsigned int codePoint)
{
	(void)font;
	(void)codePoint;
	return NULL;
}

void RE_CacheUnicodeGlyph(unsigned int codePoint, unsigned int glyphIndex)
{
	(void)codePoint;
	(void)glyphIndex;
}

void RE_ClearUnicodeCache(void)
{
}

#endif // USE_FREETYPE

