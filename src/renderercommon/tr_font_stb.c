/*
===========================================================================
stb_truetype runtime font atlas builder (optional)

Builds a glyph atlas at runtime using stb_truetype. Enabled when
USE_STB_TRUETYPE is defined and stb_truetype.h is available at
libs/stb/stb_truetype.h. Falls back to a stub otherwise.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../renderer/tr_common.h"

#ifdef USE_STB_TRUETYPE
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#define STBTT_malloc ri.Malloc
#define STBTT_free ri.Free
#include "../../libs/stb/stb_truetype.h"
#endif

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

qboolean RE_RegisterFont_Stb(const char *fontName, int pointSize, fontInfo_t *font) {
#ifndef USE_STB_TRUETYPE
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

	ri.Free(atlas);
	ri.Free(imageBuff);
	ri.FS_FreeFile(fileData);

	return qtrue;
#endif
}

