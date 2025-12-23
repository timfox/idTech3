/*
===========================================================================
tr_font_enhanced.h - Modern High-Quality Font Rendering
===========================================================================
*/

#ifndef __TR_FONT_ENHANCED_H__
#define __TR_FONT_ENHANCED_H__

#include "tr_types.h"

// Enhanced font rendering functions
float RE_Font_Height(fontInfo_t *font, float scale);
float RE_Font_Width(const char *text, float scale, fontInfo_t *font);
void RE_Font_DrawString(float x, float y, const char *text, const vec4_t color, float scale, fontInfo_t *font, int style);

#endif // __TR_FONT_ENHANCED_H__

