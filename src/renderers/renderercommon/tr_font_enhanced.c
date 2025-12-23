/*
===========================================================================
tr_font_enhanced.c - Modern High-Quality Font Rendering
===========================================================================
*/

#include "../../common/q_shared.h"
#include "../renderercommon/tr_public.h"
#include "../../common/qcommon.h"
#include "tr_font_enhanced.h"

// Renderer import interface - defined in renderer main file
extern refimport_t ri;

// Forward declaration for glyph access function from tr_font.c
glyphInfo_t *R_GetGlyphFromFont(fontInfo_t *font, int charCode);
// Forward declaration for Unicode glyph mapping
glyphInfo_t *RE_FindUnicodeGlyphInFont(fontInfo_t *font, unsigned int codePoint);

/*
=================
R_DecodeUTF8
=================
*/
static int R_DecodeUTF8(const unsigned char **textPtr) {
	const unsigned char *s = *textPtr;
	if (!s || !*s) {
		return -1;
	}

	unsigned char c = *s;
	if (c < 0x80) {
		(*textPtr)++;
		return c;
	}

	if ((c & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
		int code = ((c & 0x1F) << 6) | (s[1] & 0x3F);
		(*textPtr) += 2;
		return code;
	}

	if ((c & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
		int code = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
		(*textPtr) += 3;
		return code;
	}

	(*textPtr)++;
	return -1;
}

/*
=================
R_FindGlyph
=================
*/
static glyphInfo_t *R_FindGlyph(fontInfo_t **font, int ch) {
	if (!font || !*font) {
		return NULL;
	}

	// Try Unicode mapping first for non-ASCII
	if (ch >= 256) {
		glyphInfo_t *glyph = RE_FindUnicodeGlyphInFont(*font, ch);
		if (glyph && glyph->glyph) {
			return glyph;
		}
	}

	// Standard ASCII range or fallback
	fontInfo_t *cur = *font;
	while (cur) {
		if (ch >= 0 && ch < GLYPHS_PER_FONT) {
			glyphInfo_t *glyph = &cur->glyphs[ch];
			if (glyph && glyph->glyph) {
				*font = cur;
				return glyph;
			}
		}
		cur = cur->fallbackFont;
	}
	
	// Final fallback to the primary font's index (even if not loaded)
	if (ch >= 0 && ch < GLYPHS_PER_FONT) {
		return &(*font)->glyphs[ch];
	}
	
	return NULL;
}

/*
=================
RE_Font_Height
=================
*/
float RE_Font_Height(fontInfo_t *font, float scale) {
	if (!font) {
		return 0;
	}
	// Use 'A' as a reference for capital height
	glyphInfo_t *glyph = R_GetGlyphFromFont(font, 'A');
	if (!glyph) {
		glyph = &font->glyphs['A'];
	}
	return glyph->imageHeight * scale * font->glyphScale;
}

/*
=================
RE_Font_Width
=================
*/
float RE_Font_Width(const char *text, float scale, fontInfo_t *font) {
	if (!text || !*text || !font) {
		return 0.0f;
	}

	float width = 0.0f;
	int prev = -1;
	const unsigned char *s = (const unsigned char *)text;

	while (*s) {
		if (Q_IsColorString((const char *)s)) {
			s += 2;
			continue;
		}

		int code = R_DecodeUTF8(&s);
		if (code < 0) {
			continue;
		}

		fontInfo_t *useFont = font;
		glyphInfo_t *glyph = R_FindGlyph(&useFont, code);
		if (!glyph) {
			continue;
		}

		float useScale = scale * useFont->glyphScale;
		if (useFont->hasKerning && prev >= 0 && prev < 256) {
			width += glyph->kerning[prev] * useScale;
		}

		width += glyph->xSkip * useScale;
		prev = code;
	}

	return width;
}

/*
=================
RE_Font_DrawString
=================
*/
void RE_Font_DrawString(float x, float y, const char *text, const vec4_t color, float scale, fontInfo_t *font, int style) {
	if (!text || !*text || !font) {
		return;
	}

	extern void RE_StretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);
	extern void RE_SetColor(const float *rgba);

	float sizeScale = scale > 0.0f ? scale : 1.0f;
	float width = RE_Font_Width(text, sizeScale, font);
	
	if (style & UI_CENTER) {
		x -= width * 0.5f;
	} else if (style & UI_RIGHT) {
		x -= width;
	}

	vec4_t drawColor;
	Vector4Copy(color, drawColor);

	const unsigned char *ptr = (const unsigned char *)text;
	int prev = -1;

	qboolean drawShadow = (style & UI_DROPSHADOW) ? qtrue : qfalse;
	float shadowOfs = 2.0f * sizeScale;

	while (*ptr) {
		if (Q_IsColorString((const char *)ptr)) {
			int colorIndex = ColorIndex(ptr[1]);
			VectorCopy(g_color_table[colorIndex], drawColor);
			drawColor[3] = color[3];
			ptr += 2;
			continue;
		}

		int code = R_DecodeUTF8(&ptr);
		if (code < 0) {
			continue;
		}

		fontInfo_t *useFont = font;
		glyphInfo_t *glyph = R_FindGlyph(&useFont, code);
		if (!glyph) {
			continue;
		}

		float useScale = sizeScale * useFont->glyphScale;

		if (drawShadow) {
			vec4_t shadowColor = { 0, 0, 0, drawColor[3] * 0.7f };
            float adjX = x + shadowOfs;
            float adjY = y + shadowOfs - glyph->top * useScale;
            float w = glyph->imageWidth * useScale;
            float h = glyph->imageHeight * useScale;
            RE_SetColor(shadowColor);
            RE_StretchPic(adjX, adjY, w, h, glyph->s, glyph->t, glyph->s2, glyph->t2, glyph->glyph);
		}

        float adjX = x;
        float adjY = y - glyph->top * useScale;
        float w = glyph->imageWidth * useScale;
        float h = glyph->imageHeight * useScale;
        RE_SetColor(drawColor);
        RE_StretchPic(adjX, adjY, w, h, glyph->s, glyph->t, glyph->s2, glyph->t2, glyph->glyph);

		if (useFont->hasKerning && prev >= 0 && prev < 256) {
			x += glyph->kerning[prev] * useScale;
		}

		x += glyph->xSkip * useScale;
		prev = code;
	}

	RE_SetColor(NULL);
}

