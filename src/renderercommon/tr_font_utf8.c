/*
===========================================================================
Copyright (C) 2024 id Tech 3

UTF-8 and modern font rendering utilities
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"
#include "../renderer/tr_common.h"

/*
=================
RE_UTF8_CharLength
=================
Get the byte length of a UTF-8 character
Returns 0 for invalid sequences
=================
*/
int RE_UTF8_CharLength(const unsigned char *str)
{
	if (!str)
		return 0;
	
	unsigned char c = str[0];
	
	// ASCII character
	if ((c & 0x80) == 0)
		return 1;
	
	// Invalid start byte
	if ((c & 0xC0) == 0x80 || (c & 0xFE) == 0xFE)
		return 0;
	
	// 2-byte sequence
	if ((c & 0xE0) == 0xC0) {
		if ((str[1] & 0xC0) != 0x80)
			return 0;
		return 2;
	}
	
	// 3-byte sequence
	if ((c & 0xF0) == 0xE0) {
		if ((str[1] & 0xC0) != 0x80 || (str[2] & 0xC0) != 0x80)
			return 0;
		return 3;
	}
	
	// 4-byte sequence
	if ((c & 0xF8) == 0xF0) {
		if ((str[1] & 0xC0) != 0x80 || (str[2] & 0xC0) != 0x80 || (str[3] & 0xC0) != 0x80)
			return 0;
		return 4;
	}
	
	return 0;
}

/*
=================
RE_UTF8_DecodeChar
=================
Decode a UTF-8 character to Unicode code point
Returns the Unicode code point, or 0 on error
Advances *str to the next character
=================
*/
unsigned int RE_UTF8_DecodeChar(const unsigned char **str)
{
	if (!str || !*str)
		return 0;
	
	const unsigned char *s = *str;
	unsigned int ch = 0;
	int len = RE_UTF8_CharLength(s);
	
	if (len == 0)
		return 0;
	
	if (len == 1) {
		ch = s[0];
		*str = s + 1;
		return ch;
	}
	
	// Multi-byte sequence
	ch = s[0] & (0xFF >> (len + 1));
	for (int i = 1; i < len; i++) {
		if ((s[i] & 0xC0) != 0x80)
			return 0; // Invalid continuation byte
		ch = (ch << 6) | (s[i] & 0x3F);
	}
	
	*str = s + len;
	return ch;
}

/*
=================
RE_Text_Width_Improved
=================
Calculate text width with kerning support
=================
*/
float RE_Text_Width_Improved(const char *text, float scale, fontInfo_t *font, int limit)
{
	if (!text || !font)
		return 0.0f;
	
	float width = 0.0f;
	float useScale = scale * font->glyphScale;
	const unsigned char *s = (const unsigned char *)text;
	int count = 0;
	int len = strlen(text);
	unsigned char prevChar = 0;
	
	if (limit > 0 && len > limit)
		len = limit;
	
	while (s && *s && count < len) {
		// Skip color codes
		if (Q_IsColorString((const char *)s)) {
			s += 2;
			continue;
		}
		
		unsigned char c = *s;
		
		// Handle UTF-8 (basic support - for full Unicode, need glyph mapping)
		if ((c & 0x80) != 0) {
			// UTF-8 character - for now, skip or use fallback
			// Full Unicode support would require glyph mapping
			int utf8Len = RE_UTF8_CharLength(s);
			if (utf8Len > 0) {
				s += utf8Len;
				count++;
				prevChar = 0; // Reset kerning
				continue;
			}
		}
		
		glyphInfo_t *glyph = &font->glyphs[c & 255];
		width += glyph->xSkip * useScale;
		
		// Apply kerning if available and enabled
		extern cvar_t *r_fontKerning;
		if (font->hasKerning && prevChar != 0 && c >= 32 && c <= 127) {
			if (!r_fontKerning || r_fontKerning->integer != 0) {
				int kerningOffset = font->glyphs[prevChar].kerning[c & 255];
				if (kerningOffset != 0) {
					width += kerningOffset * useScale;
				}
			}
		}
		
		prevChar = c;
		s++;
		count++;
	}
	
	return width;
}

/*
=================
RE_Text_Height_Improved
=================
Calculate text height
=================
*/
float RE_Text_Height_Improved(const char *text, float scale, fontInfo_t *font, int limit)
{
	if (!text || !font)
		return 0.0f;
	
	float maxHeight = 0.0f;
	float useScale = scale * font->glyphScale;
	const unsigned char *s = (const unsigned char *)text;
	int count = 0;
	int len = strlen(text);
	
	if (limit > 0 && len > limit)
		len = limit;
	
	while (s && *s && count < len) {
		// Skip color codes
		if (Q_IsColorString((const char *)s)) {
			s += 2;
			continue;
		}
		
		unsigned char c = *s;
		
		// Handle UTF-8
		if ((c & 0x80) != 0) {
			int utf8Len = RE_UTF8_CharLength(s);
			if (utf8Len > 0) {
				s += utf8Len;
				count++;
				continue;
			}
		}
		
		glyphInfo_t *glyph = &font->glyphs[c & 255];
		if (glyph->height > maxHeight) {
			maxHeight = glyph->height;
		}
		
		s++;
		count++;
	}
	
	return maxHeight * useScale;
}

/*
=================
RE_Text_Bounds_Improved
=================
Calculate text bounding box with kerning
Returns width and height in outWidth and outHeight
=================
*/
void RE_Text_Bounds_Improved(const char *text, float scale, fontInfo_t *font, int limit, float *outWidth, float *outHeight)
{
	if (outWidth)
		*outWidth = RE_Text_Width_Improved(text, scale, font, limit);
	if (outHeight)
		*outHeight = RE_Text_Height_Improved(text, scale, font, limit);
}

