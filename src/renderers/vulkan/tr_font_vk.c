#include "tr_local.h"
#include "../renderercommon/tr_font.h"

// Vulkan-specific font implementation
// This replaces the OpenGL font system for Vulkan renderer

// Simple bitmap font fallback for Vulkan renderer
static const unsigned char vk_default_font_data[128 * 8] = {
	// Minimal 8x8 bitmap font data - placeholder
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // space
	// ... more font data would go here for a complete implementation
};

qboolean RE_RegisterFont_Vulkan(const char *fontName, int pointSize, fontInfo_t *font) {
	// Initialize font structure with safe defaults
	Com_Memset(font, 0, sizeof(*font));

	// Basic font metrics
	font->pointSize = pointSize;
	font->height = pointSize; // Approximate height
	font->tall = pointSize;   // Approximate tall

	// Set up glyph metrics for ASCII range
	for (int i = 0; i < 256; i++) {
		font->glyphs[i].width = 8;      // Fixed width
		font->glyphs[i].height = 8;     // Fixed height
		font->glyphs[i].xSkip = 8;      // Fixed advance
		font->glyphs[i].xOffset = 0;    // No offset
		font->glyphs[i].yOffset = 0;    // No offset

		// Simple UV coordinates (placeholder)
		font->glyphs[i].s = 0.0f;
		font->glyphs[i].t = 0.0f;
		font->glyphs[i].s2 = 1.0f;
		font->glyphs[i].t2 = 1.0f;
	}

	// TODO: Implement proper Vulkan font texture creation
	// Fallback to STB font if available to prevent hard failures in Vulkan path
	font->shader = 0; // default placeholder
	extern qboolean RE_RegisterFont_Stb(const char *fontName, int pointSize, fontInfo_t *font);
	fontInfo_t stbFont;
	if (RE_RegisterFont_Stb(fontName, pointSize, &stbFont)) {
		Com_Memcpy(font, &stbFont, sizeof(fontInfo_t));
		ri.Printf(PRINT_ALL, "Vulkan: fallback to STB font for '%s' (%dpt)\n", fontName, pointSize);
		return qtrue;
	}

	ri.Printf(PRINT_ALL, "RE_RegisterFont_Vulkan: Registered placeholder font '%s' (%dpt)\n",
		fontName, pointSize);

	return qtrue;
}
