/*
extern refimport_t ri;
===========================================================================
stb_truetype runtime font atlas builder (optional)

Builds a glyph atlas at runtime using stb_truetype. Enabled when
USE_STB_TRUETYPE is defined and stb_truetype.h is available at
libs/stb/stb_truetype.h. Falls back to a stub otherwise.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../opengl/tr_common.h"

// Font CVars - defined in each renderer
extern cvar_t *r_fontSDF;
extern cvar_t *r_fontSDFSpread;
extern cvar_t *r_fontSDFSmooth;
extern cvar_t *r_fontLCDFilter;
extern cvar_t *r_fontSDFOutline;
extern cvar_t *r_fontGPUSDF;
extern cvar_t *r_fontGPUEffects;
extern cvar_t *r_fontGPULayout;

// Renderer import interface - defined in renderer main file
extern refimport_t ri;
// Renderer import interface - defined in renderer main file

#include "../opengl/tr_local.h"

// Renderer import interface - defined in renderer main file


#if defined(USE_STB_TRUETYPE) && (__has_include("../../libs/stb/stb_truetype.h") || __has_include("../../libs/cimgui/imgui/imstb_truetype.h"))
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
static void *R_StbMalloc(size_t size, void *user) { (void)user; return ri.Malloc((int)size); }
/* stb_truetype follows the standard free() contract where freeing NULL is a no-op.
 * Our allocator raises a fatal error on NULL, so guard here to match stb's expectations. */
static void R_StbFree(void *ptr, void *user) { (void)user; if (!ptr) return; ri.Free(ptr); }
#define STBTT_malloc(x,u) R_StbMalloc((x),(u))
#define STBTT_free(x,u)   R_StbFree((x),(u))
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#if __has_include("../../libs/stb/stb_truetype.h")
#include "../../libs/stb/stb_truetype.h"
#elif __has_include("../../libs/cimgui/imgui/imstb_truetype.h")
#include "../../libs/cimgui/imgui/imstb_truetype.h"
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#else
#undef USE_STB_TRUETYPE
#endif

#ifdef USE_STB_TRUETYPE
// Select a sane atlas size based on CVar hints (mirrors FreeType path)
static int R_SelectAtlasSize(void) {
	int atlasSize = 512;
	if (r_fontAtlasSize) {
		atlasSize = r_fontAtlasSize->integer;
		if (atlasSize < 256) atlasSize = 256;
		else if (atlasSize > 512 && atlasSize < 1024) atlasSize = 512;
		else if (atlasSize > 1024) atlasSize = 1024;
		if (atlasSize != 256 && atlasSize != 512 && atlasSize != 1024) {
			atlasSize = 512;
		}
	}
	return atlasSize;
}
#endif

qboolean RE_RegisterFont_Stb(const char *fontName, int pointSize, fontInfo_t *font) {
#ifndef USE_STB_TRUETYPE
	(void)fontName;
	(void)pointSize;
	(void)font;
	ri.Printf(PRINT_WARNING, "RE_RegisterFont_Stb: built without USE_STB_TRUETYPE\n");
	return qfalse;
#else
	if (!fontName || !font) {
		return qfalse;
	}

	if (pointSize <= 0) {
		pointSize = 16;
	}

	void *fileData = NULL;
	int fileLen = ri.FS_ReadFile(fontName, &fileData);
	if (fileLen <= 0) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont_Stb: unable to read '%s'\n", fontName);
		return qfalse;
	}

	stbtt_fontinfo info;
	if (!stbtt_InitFont(&info, (const unsigned char *)fileData, 0)) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont_Stb: InitFont failed for '%s'\n", fontName);
		ri.FS_FreeFile(fileData);
		return qfalse;
	}

	const int atlasSize = R_SelectAtlasSize();
	unsigned char *atlas = ri.Malloc(atlasSize * atlasSize);
	if (!atlas) {
		ri.FS_FreeFile(fileData);
		return qfalse;
	}
	Com_Memset(atlas, 0, atlasSize * atlasSize);

	// Decide path: SDF or grayscale
	qboolean useSDF = (r_fontSDF && r_fontSDF->integer != 0);
	int sdfSpread = (r_fontSDFSpread) ? r_fontSDFSpread->integer : 6;
	if (sdfSpread < 4) sdfSpread = 4;
	if (sdfSpread > 32) sdfSpread = 32;

	if (useSDF) {
		// Manual packing with simple row packing
		float scale = stbtt_ScaleForPixelHeight(&info, (float)pointSize);
		int xOut = 0, yOut = 0, maxHeight = 0;

		Com_Memset(font, 0, sizeof(*font));

		for (int ch = 32; ch <= 127; ch++) {
			int glyphIndex = stbtt_FindGlyphIndex(&info, ch);
			int gw, gh, xoff, yoff;
			unsigned char *bmp = stbtt_GetGlyphSDF(&info, scale, glyphIndex,
				sdfSpread, 180, 0.0f, &gw, &gh, &xoff, &yoff);
			if (!bmp) {
				continue;
			}

			if (xOut + gw + 1 >= atlasSize) {
				xOut = 0;
				yOut += maxHeight + 1;
				maxHeight = 0;
			}
			if (yOut + gh + 1 >= atlasSize) {
				ri.Free(bmp);
				ri.Printf(PRINT_WARNING, "RE_RegisterFont_Stb: SDF atlas overflow for '%s'\n", fontName);
				ri.Free(atlas);
				ri.FS_FreeFile(fileData);
				return qfalse;
			}

			for (int row = 0; row < gh; row++) {
				unsigned char *src = bmp + row * gw;
				unsigned char *dst = atlas + (yOut + row) * atlasSize + xOut;
				Com_Memcpy(dst, src, gw);
			}

			glyphInfo_t *g = &font->glyphs[ch];
			int advanceWidth;
			int leftBearing;
			stbtt_GetGlyphHMetrics(&info, glyphIndex, &advanceWidth, &leftBearing);

			g->imageWidth = gw;
			g->imageHeight = gh;
			g->height = gh;
			g->pitch = gw;
			g->xSkip = (int)floorf(scale * advanceWidth + 0.5f);
			g->top = -yoff;
			g->bottom = g->top - gh;

			g->s  = (float)xOut / atlasSize;
			g->t  = (float)yOut / atlasSize;
			g->s2 = (float)(xOut + gw) / atlasSize;
			g->t2 = (float)(yOut + gh) / atlasSize;

			xOut += gw + 1;
			if (gh > maxHeight) {
				maxHeight = gh;
			}

			ri.Free(bmp);
		}

		// Convert atlas to RGBA for upload
		const int scaledSize = atlasSize * atlasSize;
		byte *imageBuff = ri.Malloc(scaledSize * 4);
		if (!imageBuff) {
			ri.Free(atlas);
			ri.FS_FreeFile(fileData);
			return qfalse;
		}

		int left = 0;
		float max = 0.0f;
		for (int i = 0; i < scaledSize; i++) {
			if (atlas[i] > max) {
				max = (float)atlas[i];
			}
		}
		if (max > 0.0f) {
			max = 255.0f / max;
		} else {
			max = 1.0f;
		}

		for (int i = 0; i < scaledSize; i++) {
			imageBuff[left++] = 255;
			imageBuff[left++] = 255;
			imageBuff[left++] = 255;
			imageBuff[left++] = (byte)((float)atlas[i] * max);
		}

		char shaderName[MAX_QPATH];
		Com_sprintf(shaderName, sizeof(shaderName), "fonts/fontImage_stb_sdf_%i", pointSize);
		image_t *image = R_CreateImage(shaderName, NULL, imageBuff, atlasSize, atlasSize, IMGFLAG_CLAMPTOEDGE);
		if (image) {
			image->isFont = qtrue;
			image->isSDF = qtrue;
			image->sdfSpread = (float)sdfSpread;
		}
		qhandle_t h = RE_RegisterShaderFromImage(shaderName, LIGHTMAP_2D, image, qfalse);

		// Fill glyph handles
		for (int i = 0; i < GLYPHS_PER_FONT; i++) {
			if (font->glyphs[i].glyph == 0) {
				font->glyphs[i].glyph = h;
				Q_strncpyz(font->glyphs[i].shaderName, shaderName, sizeof(font->glyphs[i].shaderName));
			}
		}

		// Scale: match the FreeType convention (48pt reference at 72 dpi)
		float dpi = (r_fontDPI && r_fontDPI->value > 0.0f) ? r_fontDPI->value : 72.0f;
		if (dpi < 72.0f) dpi = 72.0f;
		if (dpi > 300.0f) dpi = 300.0f;
		float glyphScale = 72.0f / dpi;
		glyphScale *= 48.0f / (float)pointSize;

		font->glyphScale = glyphScale;
		Q_strncpyz(font->name, fontName, sizeof(font->name));
		font->pointSize = pointSize;
		font->dpi = dpi;
		font->isSDF = qtrue;
		font->sdfSpread = (float)sdfSpread;

		ri.Free(atlas);
		ri.Free(imageBuff);
		ri.FS_FreeFile(fileData);

		return qtrue;
	}

	// Legacy grayscale path (non-SDF)
	stbtt_pack_context pc;
	if (!stbtt_PackBegin(&pc, atlas, atlasSize, atlasSize, 0, 1, NULL)) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont_Stb: PackBegin failed\n");
		ri.Free(atlas);
		ri.FS_FreeFile(fileData);
		return qfalse;
	}

	// Oversample a bit for smoother edges
	stbtt_PackSetOversampling(&pc, 2, 2);

	stbtt_packedchar chars[96]; // ASCII 32-127
	if (!stbtt_PackFontRange(&pc, (const unsigned char *)fileData, 0,
	                         (float)pointSize, 32, 96, chars)) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont_Stb: PackFontRange failed\n");
		stbtt_PackEnd(&pc);
		ri.Free(atlas);
		ri.FS_FreeFile(fileData);
		return qfalse;
	}

	stbtt_PackEnd(&pc);

	// Convert atlas to RGBA for upload
	const int scaledSize = atlasSize * atlasSize;
	byte *imageBuff = ri.Malloc(scaledSize * 4);
	if (!imageBuff) {
		ri.Free(atlas);
		ri.FS_FreeFile(fileData);
		return qfalse;
	}

	int left = 0;
	float max = 0.0f;
	for (int i = 0; i < scaledSize; i++) {
		if (atlas[i] > max) {
			max = (float)atlas[i];
		}
	}
	if (max > 0.0f) {
		max = 255.0f / max;
	} else {
		max = 1.0f;
	}

	for (int i = 0; i < scaledSize; i++) {
		imageBuff[left++] = 255;
		imageBuff[left++] = 255;
		imageBuff[left++] = 255;
		imageBuff[left++] = (byte)((float)atlas[i] * max);
	}

	// Upload image
	char shaderName[MAX_QPATH];
	Com_sprintf(shaderName, sizeof(shaderName), "fonts/fontImage_stb_%i", pointSize);
	image_t *image = R_CreateImage(shaderName, NULL, imageBuff, atlasSize, atlasSize, IMGFLAG_CLAMPTOEDGE);
	if (image) {
		image->isFont = qtrue;
		image->isSDF = qfalse;
		image->sdfSpread = 0.0f;
	}
	qhandle_t h = RE_RegisterShaderFromImage(shaderName, LIGHTMAP_2D, image, qfalse);

	// Fill glyphs
	Com_Memset(font, 0, sizeof(*font));
	for (int i = 0; i < GLYPHS_PER_FONT; i++) {
		font->glyphs[i].glyph = h;
		Q_strncpyz(font->glyphs[i].shaderName, shaderName, sizeof(font->glyphs[i].shaderName));
	}

	for (int i = 0; i < 96; i++) {
		int ch = 32 + i;
		glyphInfo_t *g = &font->glyphs[ch];
		stbtt_packedchar *pcData = &chars[i];

		int gw = pcData->x1 - pcData->x0;
		int gh = pcData->y1 - pcData->y0;

		g->imageWidth = gw;
		g->imageHeight = gh;
		g->height = gh;
		g->pitch = gw;
		g->xSkip = (int)floorf(pcData->xadvance + 0.5f);
		g->top = (int)floorf(pcData->yoff + 0.5f);
		g->bottom = g->top - gh;

		g->s  = (float)pcData->x0 / atlasSize;
		g->t  = (float)pcData->y0 / atlasSize;
		g->s2 = (float)pcData->x1 / atlasSize;
		g->t2 = (float)pcData->y1 / atlasSize;
	}

	// Scale: match the FreeType convention (48pt reference at 72 dpi)
	float dpi = (r_fontDPI && r_fontDPI->value > 0.0f) ? r_fontDPI->value : 72.0f;
	if (dpi < 72.0f) dpi = 72.0f;
	if (dpi > 300.0f) dpi = 300.0f;
	float glyphScale = 72.0f / dpi;
	glyphScale *= 48.0f / (float)pointSize;

	font->glyphScale = glyphScale;
	Q_strncpyz(font->name, fontName, sizeof(font->name));
	font->pointSize = pointSize;
	font->dpi = dpi;
	font->isSDF = qfalse;
	font->sdfSpread = 0.0f;

	ri.Free(atlas);
	ri.Free(imageBuff);
	ri.FS_FreeFile(fileData);

	return qtrue;
#endif
}

