/*
===========================================================================
Font fallback chain support for modern font rendering
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"
#include "../renderer/tr_common.h"

#define MAX_FONT_FALLBACKS 4
#define MAX_FONTS 6  // Match definition in tr_font.c

typedef struct {
	fontInfo_t *fonts[MAX_FONT_FALLBACKS];
	int count;
} fontFallbackChain_t;

static fontFallbackChain_t fallbackChains[MAX_FONTS];
static int fallbackChainCount = 0;

/*
=================
RE_RegisterFontFallback
=================
Register a font fallback chain
Returns qtrue on success
=================
*/
qboolean RE_RegisterFontFallback(const char *primaryFontName, int pointSize, const char **fallbackNames, int fallbackCount)
{
	if (!primaryFontName || fallbackCount < 0 || fallbackCount >= MAX_FONT_FALLBACKS)
		return qfalse;
	
	if (fallbackChainCount >= MAX_FONTS)
		return qfalse;
	
	fontFallbackChain_t *chain = &fallbackChains[fallbackChainCount++];
	chain->count = 0;
	
	// Register primary font
	fontInfo_t *primaryFont = ri.Malloc(sizeof(fontInfo_t));
	if (!primaryFont)
		return qfalse;
	
	RE_RegisterFont(primaryFontName, pointSize, primaryFont);
	
	// Link fallback fonts in chain
	if (fallbackCount > 0 && fallbackNames[0]) {
		fontInfo_t *firstFallback = ri.Malloc(sizeof(fontInfo_t));
		if (firstFallback) {
			RE_RegisterFont(fallbackNames[0], pointSize, firstFallback);
			primaryFont->fallbackFont = firstFallback; // Link first fallback
		}
	}
	
	chain->fonts[chain->count++] = primaryFont;
	
	// Register fallback fonts
	int i;
	for (i = 0; i < fallbackCount && chain->count < MAX_FONT_FALLBACKS; i++) {
		if (!fallbackNames[i])
			continue;
		
		fontInfo_t *fallbackFont = ri.Malloc(sizeof(fontInfo_t));
		if (!fallbackFont)
			break;
		
		RE_RegisterFont(fallbackNames[i], pointSize, fallbackFont);
		chain->fonts[chain->count++] = fallbackFont;
	}
	
	return qtrue;
}

/*
=================
RE_FindGlyphInFallback
=================
Find a glyph in a font fallback chain
Returns glyphInfo_t* or NULL if not found
=================
*/
static glyphInfo_t *RE_FindGlyphInFallback(fontFallbackChain_t *chain, unsigned char c)
{
	if (!chain)
		return NULL;
	
	int i;
	for (i = 0; i < chain->count; i++) {
		if (!chain->fonts[i])
			continue;
		
		glyphInfo_t *glyph = &chain->fonts[i]->glyphs[c & 255];
		if (glyph->glyph != 0) {
			return glyph;
		}
	}
	
	return NULL;
}

/*
=================
RE_GetFontFallbackChain
=================
Get font fallback chain by primary font name and point size
Returns NULL if not found
=================
*/
static fontFallbackChain_t *RE_GetFontFallbackChain(const char *fontName, int pointSize)
{
	// Simple lookup - in a full implementation, would use hash table
	// pointSize parameter reserved for future use
	(void)pointSize;  // Suppress unused parameter warning
	
	int i;
	for (i = 0; i < fallbackChainCount; i++) {
		if (fallbackChains[i].fonts[0] && 
		    !Q_stricmp(fallbackChains[i].fonts[0]->name, fontName)) {
			return &fallbackChains[i];
		}
	}
	
	return NULL;
}

/*
=================
RE_GetFontFallback
=================
Get font fallback chain (alias for RE_GetFontFallbackChain)
Returns NULL if not found
=================
*/
void *RE_GetFontFallback(const char *fontName, int pointSize)
{
	return (void *)RE_GetFontFallbackChain(fontName, pointSize);
}

