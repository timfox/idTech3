
#ifndef __TR_FONT_H__
#define __TR_FONT_H__

#include "tr_public.h"

// Font system initialization
void R_InitFonts(void);
void R_ShutdownFonts(void);

// Forward declarations for renderer-specific font registration
qboolean RE_RegisterFont_Vulkan(const char *fontName, int pointSize, fontInfo_t *font);

#endif // __TR_FONT_H__
