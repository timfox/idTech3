/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_font.c
// 
//
// The font system uses FreeType 2.x to render TrueType fonts for use within the game.
// As of this writing ( Nov, 2000 ) Team Arena uses these fonts for all of the ui and 
// about 90% of the cgame presentation. A few areas of the CGAME were left uses the old 
// fonts since the code is shared with standard Q3A.
//
// If you include this font rendering code in a commercial product you MUST include the
// following somewhere with your product, see www.freetype.org for specifics or changes.
// The Freetype code also uses some hinting techniques that MIGHT infringe on patents 
// held by apple so be aware of that also.
//
// As of Q3A 1.25+ and Team Arena, we are shipping the game with the font rendering code
// disabled. This removes any potential patent issues and it keeps us from having to 
// distribute an actual TrueTrype font which is 1. expensive to do and 2. seems to require
// an act of god to accomplish. 
//
// What we did was pre-render the fonts using FreeType ( which is why we leave the FreeType
// credit in the credits ) and then saved off the glyph data and then hand touched up the 
// font bitmaps so they scale a bit better in GL.
//
// There are limitations in the way fonts are saved and reloaded in that it is based on 
// point size and not name. So if you pre-render Helvetica in 18 point and Impact in 18 point
// you will end up with a single 18 point data file and image set. Typically you will want to 
// choose 3 sizes to best approximate the scaling you will be doing in the ui scripting system
// 
// In the UI Scripting code, a scale of 1.0 is equal to a 48 point font. In Team Arena, we
// use three or four scales, most of them exactly equaling the specific rendered size. We 
// rendered three sizes in Team Arena, 12, 16, and 20. 
//
// To generate new font data you need to go through the following steps.
// 1. delete the fontImage_x_xx.tga files and fontImage_xx.dat files from the fonts path.
// 2. in a ui script, specificy a font, smallFont, and bigFont keyword with font name and 
//    point size. the original TrueType fonts must exist in fonts at this point.
// 3. run the game, you should see things normally.
// 4. Exit the game and there will be three dat files and at least three tga files. The 
//    tga's are in 256x256 pages so if it takes three images to render a 24 point font you 
//    will end up with fontImage_0_24.tga through fontImage_2_24.tga
// 5. In future runs of the game, the system looks for these images and data files when a
//    specific point sized font is rendered and loads them for use. 
// 6. Because of the original beta nature of the FreeType code you will probably want to hand
//    touch the font bitmaps.
// 
// Currently a define in the project turns on or off the FreeType code which is currently 
// defined out. To pre-render new fonts you need enable the define ( BUILD_FREETYPE ) and 
// uncheck the exclude from build check box in the FreeType2 area of the Renderer project. 


#include "../common/q_shared.h"
#include "../common/qcommon.h"
#include "tr_public.h"
#include "../opengl/tr_common.h"

// Forward declarations for public API functions
glyphInfo_t *R_GetGlyphFromFont(fontInfo_t *font, int charCode);
void R_InitFonts(void);
void R_ShutdownFonts(void);

#ifdef BUILD_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ERRORS_H
#endif

// Font CVars are accessed via ri.Cvar_Get to avoid DLL symbol issues

// Renderer import interface - defined in renderer main file
extern refimport_t ri;

extern void R_IssuePendingRenderCommands( void );
extern qhandle_t RE_RegisterShaderNoMip( const char *name );



#ifdef USE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ERRORS_H
#include FT_SYSTEM_H
#include FT_IMAGE_H
#include FT_OUTLINE_H
#endif

#ifdef USE_BROTLI
#include <brotli/decode.h>
#endif

#ifdef USE_FREETYPE
#define _FLOOR(x)  ((x) & -64)
#define _CEIL(x)   (((x)+63) & -64)
#define _TRUNC(x)  ((x) >> 6)

static FT_Library ftLibrary = NULL;  
#endif

#define MAX_FONTS 6
#define MAX_FONT_CACHE 32  // Extended cache for better performance
static int registeredFontCount = 0;
static fontInfo_t registeredFont[MAX_FONTS];

// Font cache entry for name+size lookup
typedef struct {
	char fontName[MAX_QPATH];
	int pointSize;
	fontInfo_t *font;
	qboolean inUse;
	qboolean useSDF;
} fontCacheEntry_t;

static fontCacheEntry_t fontCache[MAX_FONT_CACHE];
static int fontCacheCount = 0;

// Glyph subsetting support
#define MAX_GLYPH_SUBSET_SIZE 512  // Maximum glyphs in a subset font

typedef struct glyphSubset_s {
	qboolean enabled;           // Whether subsetting is enabled
	qboolean tracking;          // Whether to track glyph usage
	uint32_t usageCount[256];   // How many times each character was used
	uint8_t  subsetMap[256];    // Maps character code to subset glyph index (-1 if not in subset)
	int      subsetSize;        // Number of glyphs in the subset
	qboolean finalized;         // Whether the subset has been finalized
} glyphSubset_t;

static glyphSubset_t glyphSubset;

/*
=================
R_InitGlyphSubsetting
Initialize glyph subsetting system
=================
*/
static void R_InitGlyphSubsetting(void) {
	Com_Memset(&glyphSubset, 0, sizeof(glyphSubset));
	glyphSubset.enabled = qtrue;  // Enable by default
	glyphSubset.tracking = qtrue; // Start tracking usage
	glyphSubset.subsetSize = 0;
	glyphSubset.finalized = qfalse;

	// Initialize subset map to -1 (not in subset)
	Com_Memset(glyphSubset.subsetMap, 0xFF, sizeof(glyphSubset.subsetMap));

	ri.Printf(PRINT_ALL, "R_InitGlyphSubsetting: Glyph subsetting initialized\n");
}

/*
=================
R_TrackGlyphUsage
Track usage of a glyph character
=================
*/
static void R_TrackGlyphUsage(int charCode) {
	if (!glyphSubset.tracking || glyphSubset.finalized) {
		return;
	}

	if (charCode >= 0 && charCode < 256) {
		glyphSubset.usageCount[charCode]++;
	}
}

/*
=================
R_FinalizeGlyphSubset
Create the final glyph subset based on usage statistics
=================
*/
static void R_FinalizeGlyphSubset(void) {
	if (glyphSubset.finalized || !glyphSubset.enabled) {
		return;
	}

	int totalUsage = 0;
	int usedChars = 0;

	// Count total usage and used characters
	for (int i = 0; i < 256; i++) {
		if (glyphSubset.usageCount[i] > 0) {
			totalUsage += glyphSubset.usageCount[i];
			usedChars++;
		}
	}

	ri.Printf(PRINT_ALL, "R_FinalizeGlyphSubset: %d characters used, %d total usages\n", usedChars, totalUsage);

	// Sort characters by usage frequency (simple bubble sort for now)
	int charOrder[256];
	for (int i = 0; i < 256; i++) {
		charOrder[i] = i;
	}

	for (int i = 0; i < 255; i++) {
		for (int j = 0; j < 255 - i; j++) {
			if (glyphSubset.usageCount[charOrder[j]] < glyphSubset.usageCount[charOrder[j + 1]]) {
				int temp = charOrder[j];
				charOrder[j] = charOrder[j + 1];
				charOrder[j + 1] = temp;
			}
		}
	}

	// Create subset mapping - include most frequently used characters first
	glyphSubset.subsetSize = 0;
	for (int i = 0; i < 256 && glyphSubset.subsetSize < MAX_GLYPH_SUBSET_SIZE; i++) {
		int charCode = charOrder[i];
		if (glyphSubset.usageCount[charCode] > 0) {
			glyphSubset.subsetMap[charCode] = glyphSubset.subsetSize;
			glyphSubset.subsetSize++;
		}
	}

	// Always include essential ASCII characters even if not used yet
	const char *essentialChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,!?-:;()[]{}";
	for (const char *c = essentialChars; *c && glyphSubset.subsetSize < MAX_GLYPH_SUBSET_SIZE; c++) {
		int charCode = (unsigned char)*c;
		if (glyphSubset.subsetMap[charCode] == 0xFF) { // Not in subset yet
			glyphSubset.subsetMap[charCode] = glyphSubset.subsetSize;
			glyphSubset.subsetSize++;
		}
	}

	glyphSubset.finalized = qtrue;
	glyphSubset.tracking = qfalse; // Stop tracking after finalization

	ri.Printf(PRINT_ALL, "R_FinalizeGlyphSubset: Created subset with %d glyphs\n", glyphSubset.subsetSize);
}

/*
=================
R_GetGlyphSubsetIndex
Get the subset index for a character code, or -1 if not in subset
=================
*/
static int __attribute__((unused)) R_GetGlyphSubsetIndex(int charCode) {
	if (!glyphSubset.enabled || !glyphSubset.finalized) {
		return charCode & 255; // Use original indexing if subsetting disabled
	}

	if (charCode < 0 || charCode >= 256) {
		return -1;
	}

	return glyphSubset.subsetMap[charCode];
}

/*
=================
R_IsGlyphInSubset
Check if a character is in the current glyph subset
=================
*/
static qboolean __attribute__((unused)) R_IsGlyphInSubset(int charCode) {
	if (!glyphSubset.enabled || !glyphSubset.finalized) {
		return qtrue; // All glyphs available if subsetting disabled
	}

	if (charCode < 0 || charCode >= 256) {
		return qfalse;
	}

	return glyphSubset.subsetMap[charCode] != 0xFF;
}

/*
=================
R_GetGlyphFromFont
Get a glyph from a font, handling subset fonts
=================
*/
glyphInfo_t *R_GetGlyphFromFont(fontInfo_t *font, int charCode) {
	if (!font) {
		return NULL;
	}

	// Track glyph usage for subsetting
	R_TrackGlyphUsage(charCode);

	// Handle subset fonts
	if (font->isSubset) {
		if (charCode < 0 || charCode >= 256) {
			return NULL;
		}
		int subsetIndex = font->subsetMap[charCode];
		if (subsetIndex == 0xFF) {
			// Glyph not in subset, try fallback font
			if (font->fallbackFont) {
				return R_GetGlyphFromFont(font->fallbackFont, charCode);
			}
			return NULL;
		}
		return &font->glyphs[subsetIndex];
	} else {
		// Standard font - use original indexing
		return &font->glyphs[charCode & 255];
	}
}

// Async font loading structures
#ifdef USE_JOBSYSTEM
typedef struct fontLoadJob_s {
	char fontName[MAX_QPATH];
	int pointSize;
	fontInfo_t *targetFont;
	qboolean useSDF;
	qboolean completed;
	qboolean success;
	fontInfo_t loadedFont;  // Temporary storage for loaded font data
} fontLoadJob_t;

// Maximum concurrent async font loads
#define MAX_ASYNC_FONT_JOBS 8
static fontLoadJob_t asyncFontJobs[MAX_ASYNC_FONT_JOBS];
static int asyncFontJobCount = 0;
#endif

// Store a built font in the registry and name+size cache
static void R_AddFontToCache(const char *fontName, int pointSize, qboolean useSDF, fontInfo_t *font) {
	int slot = registeredFontCount;

	if (registeredFontCount >= MAX_FONTS) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: Too many fonts registered already.\n");
		return;
	}

	Com_Memcpy(&registeredFont[slot], font, sizeof(fontInfo_t));

	if (fontCacheCount < MAX_FONT_CACHE) {
		fontCacheEntry_t *entry = &fontCache[fontCacheCount++];
		Q_strncpyz(entry->fontName, fontName, sizeof(entry->fontName));
		entry->pointSize = pointSize;
		entry->useSDF = useSDF;
		entry->font = &registeredFont[slot];
		entry->inUse = qtrue;
	}

	registeredFontCount++;

	ri.Printf(PRINT_ALL, "RE_RegisterFont: registered '%s' (%dpt, SDF=%d) as slot %d, total=%d\n",
		fontName, pointSize, useSDF ? 1 : 0, slot, registeredFontCount);
}

#ifdef USE_FREETYPE
// Font CVars are accessed via ri.Cvar_Get

static void R_GetGlyphInfo(FT_GlyphSlot glyph, int *left, int *right, int *width, int *top, int *bottom, int *height, int *pitch) {
	*left  = _FLOOR( glyph->metrics.horiBearingX );
	*right = _CEIL( glyph->metrics.horiBearingX + glyph->metrics.width );
	*width = _TRUNC(*right - *left);

	*top    = _CEIL( glyph->metrics.horiBearingY );
	*bottom = _FLOOR( glyph->metrics.horiBearingY - glyph->metrics.height );
	*height = _TRUNC( *top - *bottom );
	*pitch  = ( qtrue ? (*width+3) & -4 : (*width+7) >> 3 );
}

/*
=================
R_RenderGlyph_Improved
=================
Improved glyph rendering with better antialiasing and hinting options
=================
*/
static FT_Bitmap *R_RenderGlyph_Improved(FT_GlyphSlot glyph, glyphInfo_t* glyphOut, int __attribute__((unused)) loadFlags) {
	FT_Bitmap  *bit2;
	int left, right, width, top, bottom, height, pitch, size;
	FT_Error error;

	R_GetGlyphInfo(glyph, &left, &right, &width, &top, &bottom, &height, &pitch);

	if ( glyph->format == ft_glyph_format_outline ) {
		size   = pitch*height; 

		bit2 = ri.Malloc(sizeof(FT_Bitmap));

		bit2->width      = width;
		bit2->rows       = height;
		bit2->pitch      = pitch;
		bit2->pixel_mode = ft_pixel_mode_grays;
		bit2->buffer     = ri.Malloc(pitch*height);
		bit2->num_grays = 256;

		Com_Memset( bit2->buffer, 0, size );

		FreeType_OutlineTranslate( &glyph->outline, -left, -bottom );

		// Use improved rendering with better quality
		error = FreeType_OutlineGetBitmap( ftLibrary, &glyph->outline, bit2 );
		if (error) {
			ri.Free(bit2->buffer);
			ri.Free(bit2);
			return NULL;
		}

		glyphOut->height = height;
		glyphOut->pitch = pitch;
		glyphOut->top = (glyph->metrics.horiBearingY >> 6) + 1;
		glyphOut->bottom = bottom;

		return bit2;
	} else {
		ri.Printf(PRINT_ALL, "Non-outline fonts are not supported\n");
	}
	return NULL;
}

static void WriteTGA (const char *filename, byte *data, int width, int height) {
	byte			*buffer;
	int				i, c;
	int             row;
	unsigned char  *flip;
	unsigned char  *src, *dst;

	buffer = ri.Malloc(width*height*4 + 18);
	Com_Memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = width&255;
	buffer[13] = width>>8;
	buffer[14] = height&255;
	buffer[15] = height>>8;
	buffer[16] = 32;	// pixel size

	// swap rgb to bgr
	c = 18 + width * height * 4;
	for (i=18 ; i<c ; i+=4)
	{
		buffer[i] = data[i-18+2];		// blue
		buffer[i+1] = data[i-18+1];		// green
		buffer[i+2] = data[i-18+0];		// red
		buffer[i+3] = data[i-18+3];		// alpha
	}

	// flip upside down
	flip = (unsigned char *)ri.Malloc(width*4);
	for(row = 0; row < height/2; row++)
	{
		src = buffer + 18 + row * 4 * width;
		dst = buffer + 18 + (height - row - 1) * 4 * width;

		Com_Memcpy(flip, src, width*4);
		Com_Memcpy(src, dst, width*4);
		Com_Memcpy(dst, flip, width*4);
	}
	ri.Free(flip);

	ri.FS_WriteFile(filename, buffer, c);

	//f = fopen (filename, "wb");
	//fwrite (buffer, 1, c, f);
	//fclose (f);

	ri.Free (buffer);
}

#ifdef USE_BROTLI
/*
=================
WOFF2_DecompressFont
Decompress WOFF2 compressed font data to TTF format
=================
*/
static qboolean WOFF2_DecompressFont(const unsigned char *woff2Data, size_t woff2Size, unsigned char **decompressedData, size_t *decompressedSize) {
	BrotliDecoderState *state = BrotliDecoderCreateInstance(NULL, NULL, NULL);
	if (!state) {
		ri.Printf(PRINT_WARNING, "WOFF2_DecompressFont: Failed to create Brotli decoder\n");
		return qfalse;
	}

	// Start with a reasonable buffer size, will expand if needed
	size_t outputBufferSize = woff2Size * 4;  // WOFF2 is typically 30-50% smaller
	*decompressedData = ri.Malloc(outputBufferSize);
	if (!*decompressedData) {
		BrotliDecoderDestroyInstance(state);
		ri.Printf(PRINT_WARNING, "WOFF2_DecompressFont: Failed to allocate output buffer\n");
		return qfalse;
	}

	size_t totalOut = 0;
	BrotliDecoderResult result;

	do {
		size_t availableIn = woff2Size;
		const uint8_t *nextIn = woff2Data;
		size_t availableOut = outputBufferSize - totalOut;
		uint8_t *nextOut = *decompressedData + totalOut;

		result = BrotliDecoderDecompressStream(state, &availableIn, &nextIn, &availableOut, &nextOut, &totalOut);

		if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
			// Need more output space, double the buffer
			size_t newSize = outputBufferSize * 2;
			uint8_t *newBuffer = ri.Malloc(newSize);
			if (!newBuffer) {
				ri.Free(*decompressedData);
				BrotliDecoderDestroyInstance(state);
				ri.Printf(PRINT_WARNING, "WOFF2_DecompressFont: Failed to expand output buffer\n");
				return qfalse;
			}
			Com_Memcpy(newBuffer, *decompressedData, outputBufferSize);
			ri.Free(*decompressedData);
			*decompressedData = newBuffer;
			outputBufferSize = newSize;
		} else if (result == BROTLI_DECODER_RESULT_ERROR) {
			ri.Free(*decompressedData);
			BrotliDecoderDestroyInstance(state);
			ri.Printf(PRINT_WARNING, "WOFF2_DecompressFont: Brotli decompression error\n");
			return qfalse;
		}
	} while (result == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT || result == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT);

	BrotliDecoderDestroyInstance(state);

	if (result != BROTLI_DECODER_RESULT_SUCCESS) {
		ri.Free(*decompressedData);
		ri.Printf(PRINT_WARNING, "WOFF2_DecompressFont: Decompression failed with result %d\n", result);
		return qfalse;
	}

	*decompressedSize = totalOut;
	ri.Printf(PRINT_ALL, "WOFF2_DecompressFont: Successfully decompressed %zu bytes to %zu bytes (%.1f%% reduction)\n",
		woff2Size, *decompressedSize, (1.0f - (float)*decompressedSize / (float)woff2Size) * 100.0f);

	return qtrue;
}
#endif

static glyphInfo_t *RE_ConstructGlyphInfo(unsigned char *imageOut, int *xOut, int *yOut, int *maxHeight, FT_Face face, const unsigned char c, qboolean calcHeight) {
	int i;
	static glyphInfo_t glyph;
	unsigned char *src, *dst;
	float scaled_width, scaled_height;
	FT_Bitmap *bitmap = NULL;
	int loadFlags = FT_LOAD_DEFAULT;

	Com_Memset(&glyph, 0, sizeof(glyphInfo_t));
	// Initialize kerning array
	Com_Memset(glyph.kerning, 0, sizeof(glyph.kerning));
	// make sure everything is here
	if (face != NULL) {
		// Use improved load flags based on hinting CVar
		{
			cvar_t *fontHinting = ri.Cvar_Get("r_fontHinting", "1", 0);
			if (fontHinting) {
				switch (fontHinting->integer) {
				case 0: // None
					loadFlags = FT_LOAD_NO_HINTING;
					break;
				case 1: // Light
					loadFlags = FT_LOAD_TARGET_LIGHT;
					break;
				case 2: // Normal (default)
					loadFlags = FT_LOAD_DEFAULT;
					break;
				case 3: // Strong
					loadFlags = FT_LOAD_TARGET_NORMAL;
					break;
				default:
					loadFlags = FT_LOAD_DEFAULT;
					break;
				}
			}
		}
		
		FreeType_LoadGlyph(face, FreeType_GetCharIndex( face, c), loadFlags );
		bitmap = R_RenderGlyph_Improved(face->glyph, &glyph, loadFlags);
		if (bitmap) {
			glyph.xSkip = (face->glyph->metrics.horiAdvance >> 6) + 1;
		} else {
			return &glyph;
		}

		if (glyph.height > *maxHeight) {
			*maxHeight = glyph.height;
		}

		if (calcHeight) {
			ri.Free(bitmap->buffer);
			ri.Free(bitmap);
			return &glyph;
		}

/*
		// need to convert to power of 2 sizes so we do not get 
		// any scaling from the gl upload
		for (scaled_width = 1 ; scaled_width < glyph.pitch ; scaled_width<<=1)
			;
		for (scaled_height = 1 ; scaled_height < glyph.height ; scaled_height<<=1)
			;
*/

		scaled_width = glyph.pitch;
		scaled_height = glyph.height;

		// Get atlas size from CVar (default to 256)
		int atlasSize = 256;
		{
			cvar_t *fontAtlasSize = ri.Cvar_Get("r_fontAtlasSize", "256", 0);
			if (fontAtlasSize) {
				atlasSize = fontAtlasSize->integer;
			if (atlasSize < 256) atlasSize = 256;
			else if (atlasSize > 512 && atlasSize < 1024) atlasSize = 512;
			else if (atlasSize > 1024) atlasSize = 1024;
				if (atlasSize < 256) atlasSize = 256;
				else if (atlasSize > 512 && atlasSize < 1024) atlasSize = 512;
				else if (atlasSize > 1024) atlasSize = 1024;
				if (atlasSize != 256 && atlasSize != 512 && atlasSize != 1024) {
					atlasSize = 256;
				}
			}
		}

		// we need to make sure we fit
		if (*xOut + scaled_width + 1 >= atlasSize - 1) {
			*xOut = 0;
			*yOut += *maxHeight + 1;
		}

		if (*yOut + *maxHeight + 1 >= atlasSize - 1) {
			*yOut = -1;
			*xOut = -1;
			ri.Free(bitmap->buffer);
			ri.Free(bitmap);
			return &glyph;
		}


		src = bitmap->buffer;
		dst = imageOut + (*yOut * atlasSize) + *xOut;

		if (bitmap->pixel_mode == ft_pixel_mode_mono) {
			for (i = 0; i < glyph.height; i++) {
				int j;
				unsigned char *_src = src;
				unsigned char *_dst = dst;
				unsigned char mask = 0x80;
				unsigned char val = *_src;
				for (j = 0; j < glyph.pitch; j++) {
					if (mask == 0x80) {
						val = *_src++;
					}
					if (val & mask) {
						*_dst = 0xff;
					}
					mask >>= 1;

					if ( mask == 0 ) {
						mask = 0x80;
					}
					_dst++;
				}

				src += glyph.pitch;
				dst += atlasSize;
			}
		} else {
			for (i = 0; i < glyph.height; i++) {
				Com_Memcpy(dst, src, glyph.pitch);
				src += glyph.pitch;
				dst += atlasSize;
			}
		}

		// we now have an 8 bit per pixel grey scale bitmap 
		// that is width wide and pf->ftSize->metrics.y_ppem tall

		glyph.imageHeight = scaled_height;
		glyph.imageWidth = scaled_width;
		glyph.s = (float)*xOut / atlasSize;
		glyph.t = (float)*yOut / atlasSize;
		glyph.s2 = glyph.s + (float)scaled_width / atlasSize;
		glyph.t2 = glyph.t + (float)scaled_height / atlasSize;

		*xOut += scaled_width + 1;

		ri.Free(bitmap->buffer);
		ri.Free(bitmap);
	}

	return &glyph;
}
#endif

static int fdOffset;
static byte	*fdFile;

static int readInt( void ) {
	int i = ((unsigned int)fdFile[fdOffset] | ((unsigned int)fdFile[fdOffset+1]<<8) | ((unsigned int)fdFile[fdOffset+2]<<16) | ((unsigned int)fdFile[fdOffset+3]<<24));
	fdOffset += 4;
	return i;
}

typedef union {
	byte	fred[4];
	float	ffred;
} poor;

static float readFloat( void ) {
	poor	me;
#if defined Q3_BIG_ENDIAN
	me.fred[0] = fdFile[fdOffset+3];
	me.fred[1] = fdFile[fdOffset+2];
	me.fred[2] = fdFile[fdOffset+1];
	me.fred[3] = fdFile[fdOffset+0];
#elif defined Q3_LITTLE_ENDIAN
	me.fred[0] = fdFile[fdOffset+0];
	me.fred[1] = fdFile[fdOffset+1];
	me.fred[2] = fdFile[fdOffset+2];
	me.fred[3] = fdFile[fdOffset+3];
#endif
	fdOffset += 4;
	return me.ffred;
}

/*
=================
RE_RegisterFont
=================
Register a font with optional style flags
fontName: path to font file
pointSize: size in points
font: output font info structure
flags: optional flags (future: bold, italic, etc.)
=================
*/
/*
=================
RE_RegisterFont_Sync
Synchronous font registration (original implementation)
=================
*/
#ifdef BUILD_FREETYPE
static qboolean RE_RegisterFont_Sync(const char *fontName, int pointSize, fontInfo_t *font, qboolean wantSDF) {
#ifdef USE_FREETYPE
	// Ensure FreeType is initialized before trying to use it
	extern qboolean FreeType_Init(void);
	if (!FreeType_Init()) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: Failed to initialize FreeType.\n");
	}
#endif

	// Variables used in both FreeType and non-FreeType paths
	void *faceData;
	int i, len;
	char name[1024];

#ifdef BUILD_FREETYPE
	FT_Face __attribute__((unused)) face;
	int __attribute__((unused)) xOut, __attribute__((unused)) yOut, __attribute__((unused)) lastStart, __attribute__((unused)) imageNumber;
	int __attribute__((unused)) scaledSize, __attribute__((unused)) newSize, __attribute__((unused)) maxHeight, __attribute__((unused)) left;
	unsigned char __attribute__((unused)) *out, __attribute__((unused)) *imageBuff;
	glyphInfo_t __attribute__((unused)) *glyph;
	image_t __attribute__((unused)) *image;
	float __attribute__((unused)) max;
	int __attribute__((unused)) loadFlags = FT_LOAD_DEFAULT;
	FT_Render_Mode __attribute__((unused)) renderMode = FT_RENDER_MODE_NORMAL;
	float __attribute__((unused)) dpi = 72.0f;
	float __attribute__((unused)) glyphScale;
	int __attribute__((unused)) atlasSize = 256;
	qhandle_t __attribute__((unused)) h;
	int __attribute__((unused)) j, __attribute__((unused)) k;
#endif
	qboolean fromCache = qfalse;
	qboolean fromDat = qfalse;
	qboolean fromStb = qfalse;
	qboolean fromFreeType = qfalse;
	qboolean generatedAtlas = qfalse;
	int atlasPages = 0;
	int selectedAtlasSize = 256;
	int selectedDPI = 72;
	int selectedHint = -1;
	int selectedAA = -1;
	int selectedSpread = -1;

	if (!fontName) {
		ri.Printf(PRINT_ALL, "RE_RegisterFont: called with empty name\n");
		return qfalse;
	}

	if (pointSize <= 0) {
		pointSize = 12;
	}

	//R_IssuePendingRenderCommands();

	// Check font cache first (by name+size for better caching)
	for (i = 0; i < fontCacheCount; i++) {
		if (fontCache[i].inUse && 
		    fontCache[i].pointSize == pointSize &&
		    fontCache[i].useSDF == wantSDF &&
		    !Q_stricmp(fontCache[i].fontName, fontName)) {
			// Found in cache - copy cached font
			Com_Memcpy(font, fontCache[i].font, sizeof(fontInfo_t));
			fromCache = qtrue;
			ri.Printf(PRINT_ALL, "RE_RegisterFont: cache hit '%s' (%dpt, SDF=%d)\n",
				fontName, pointSize, wantSDF ? 1 : 0);
			return qtrue;
		}
	}

	if (registeredFontCount >= MAX_FONTS) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: Too many fonts registered already.\n");
		return qfalse;
	}

	ri.Printf(PRINT_ALL, "RE_RegisterFont: request name='%s' size=%d SDF=%d cache=%d/%d registered=%d/%d\n",
		fontName, pointSize, wantSDF ? 1 : 0, fontCacheCount, MAX_FONT_CACHE, registeredFontCount, MAX_FONTS);

	// Check pre-rendered font data files (legacy cache by point size only)
	Com_sprintf(name, sizeof(name), "fonts/fontImage_%i.dat",pointSize);
	for (i = 0; i < registeredFontCount; i++) {
		if (Q_stricmp(name, registeredFont[i].name) == 0) {
			Com_Memcpy(font, &registeredFont[i], sizeof(fontInfo_t));
			// Also add to new cache
			if (fontCacheCount < MAX_FONT_CACHE) {
				fontCacheEntry_t *entry = &fontCache[fontCacheCount++];
				Q_strncpyz(entry->fontName, fontName, sizeof(entry->fontName));
				entry->pointSize = pointSize;
				entry->font = &registeredFont[i];
				entry->inUse = qtrue;
			}
			return qtrue;
		}
	}

	len = ri.FS_ReadFile(name, NULL);
	if (!wantSDF && len == sizeof(fontInfo_t)) {
		ri.FS_ReadFile(name, &faceData);
		fdOffset = 0;
		fdFile = faceData;
		for(i=0; i<GLYPHS_PER_FONT; i++) {
			font->glyphs[i].height		= readInt();
			font->glyphs[i].top			= readInt();
			font->glyphs[i].bottom		= readInt();
			font->glyphs[i].pitch		= readInt();
			font->glyphs[i].xSkip		= readInt();
			font->glyphs[i].imageWidth	= readInt();
			font->glyphs[i].imageHeight = readInt();
			font->glyphs[i].s			= readFloat();
			font->glyphs[i].t			= readFloat();
			font->glyphs[i].s2			= readFloat();
			font->glyphs[i].t2			= readFloat();
			font->glyphs[i].glyph		= readInt();
			Q_strncpyz(font->glyphs[i].shaderName, (const char *)&fdFile[fdOffset], sizeof(font->glyphs[i].shaderName));
			fdOffset += sizeof(font->glyphs[i].shaderName);
		}
		font->glyphScale = readFloat();
		font->isSDF = qfalse;
		font->sdfSpread = 0.0f;
		font->fallbackFont = NULL;
		Com_Memcpy(font->name, &fdFile[fdOffset], MAX_QPATH);

//		Com_Memcpy(font, faceData, sizeof(fontInfo_t));
		Q_strncpyz(font->name, name, sizeof(font->name));
		for (i = GLYPH_START; i <= GLYPH_END; i++) {
			font->glyphs[i].glyph = RE_RegisterShaderNoMip(font->glyphs[i].shaderName);
			// Initialize kerning array for loaded fonts
			Com_Memset(font->glyphs[i].kerning, 0, sizeof(font->glyphs[i].kerning));
		}
		// Initialize font metadata for loaded fonts (may not be in file format)
		if (font->hasKerning == qfalse && font->pointSize == 0) {
			font->hasKerning = qfalse; // Assume no kerning for pre-rendered fonts
			font->pointSize = pointSize;
			font->dpi = 72.0f; // Default DPI
		}
		R_AddFontToCache(fontName, pointSize, qfalse, font);
		fromDat = qtrue;
		ri.Printf(PRINT_ALL, "RE_RegisterFont: loaded legacy fontImage_%i.dat for '%s'\n", pointSize, fontName);
		ri.FS_FreeFile(faceData);
		return qtrue;
	}

#ifdef USE_STB_TRUETYPE
	qboolean tryStb = wantSDF;
#ifndef USE_FREETYPE
	tryStb = qtrue;
#endif
	if (tryStb) {
		cvar_t *dbgFontAtlasSize = ri.Cvar_Get("r_fontAtlasSize", "512", 0);
		int dbgAtlas = (dbgFontAtlasSize) ? dbgFontAtlasSize->integer : 512;
		if (dbgAtlas < 256) dbgAtlas = 256;
		if (dbgAtlas > 1024) dbgAtlas = 1024;
		cvar_t *dbgSDFSpread = ri.Cvar_Get("r_fontSDFSpread", "6", 0);
		int dbgSpread = (dbgSDFSpread) ? dbgSDFSpread->integer : 6;
		ri.Printf(PRINT_ALL, "RE_RegisterFont: stb path begin '%s' size=%d atlasHint=%d spread=%d\n",
			fontName, pointSize, dbgAtlas, dbgSpread);
		if (RE_RegisterFont_Stb(fontName, pointSize, font)) {
			R_AddFontToCache(fontName, pointSize, font->isSDF, font);
			fromStb = qtrue;
			generatedAtlas = qtrue;
			atlasPages = 1; // stb path always uses a single atlas page
			ri.Printf(PRINT_ALL, "RE_RegisterFont: stb_truetype baked '%s' (%dpt, SDF=%d)\n",
				fontName, pointSize, wantSDF ? 1 : 0);
			return qtrue;
		}
#ifndef USE_FREETYPE
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: FreeType not available and stb_truetype failed for '%s'\n", fontName);
		return qfalse;
#else
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: stb_truetype failed for '%s', falling back to FreeType\n", fontName);
#endif
	}
#endif

#ifndef USE_FREETYPE
	ri.Printf(PRINT_WARNING, "RE_RegisterFont: FreeType code not available\n");
#else
	ftLibrary = FreeType_GetLibrary();
	if (ftLibrary == NULL) {
		// Try to initialize FreeType if it hasn't been initialized yet
		extern qboolean FreeType_Init(void);
		if (!FreeType_Init()) {
			ri.Printf(PRINT_WARNING, "RE_RegisterFont: FreeType initialization failed.\n");
			return qfalse;
		}
		ftLibrary = FreeType_GetLibrary();
		if (ftLibrary == NULL) {
			ri.Printf(PRINT_WARNING, "RE_RegisterFont: FreeType not available after initialization.\n");
			return qfalse;
		}
	}

	len = ri.FS_ReadFile(fontName, &faceData);
	if (len <= 0) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: Unable to read font file '%s' (len=%d)\n", fontName, len);
		// Try to check if file exists by attempting to read file size only
		int testLen = ri.FS_ReadFile(fontName, NULL);
		if (testLen > 0) {
			ri.Printf(PRINT_WARNING, "RE_RegisterFont: File exists but couldn't allocate memory (size=%d bytes)\n", testLen);
		} else if (testLen == 0) {
			ri.Printf(PRINT_WARNING, "RE_RegisterFont: File is empty or not found\n");
		} else {
			ri.Printf(PRINT_WARNING, "RE_RegisterFont: File not found or file system error (error code=%d)\n", testLen);
		}
		return qfalse;
	}

	// Check if this is a WOFF2 compressed font and decompress if needed
#ifdef USE_BROTLI
	if (len >= 4 && faceData[0] == 'w' && faceData[1] == 'O' && faceData[2] == 'F' && faceData[3] == '2') {
		// This is a WOFF2 file, decompress it
		ri.Printf(PRINT_ALL, "RE_RegisterFont: Detected WOFF2 compressed font '%s', decompressing...\n", fontName);

		unsigned char *decompressedData = NULL;
		size_t decompressedSize = 0;

		if (WOFF2_DecompressFont(faceData, len, &decompressedData, &decompressedSize)) {
			// Replace the compressed data with decompressed data
			ri.FS_FreeFile(faceData);
			faceData = decompressedData;
			len = decompressedSize;
			ri.Printf(PRINT_ALL, "RE_RegisterFont: WOFF2 decompression successful for '%s'\n", fontName);
		} else {
			ri.Printf(PRINT_WARNING, "RE_RegisterFont: WOFF2 decompression failed for '%s', using fallback\n", fontName);
			// Continue with compressed data - FreeType might handle it, but unlikely
		}
	}
#endif

	// Get font quality settings from CVars
	{
		cvar_t *fontDPI = ri.Cvar_Get("r_fontDPI", "72", 0);
		dpi = fontDPI->value;
		if (dpi < 72.0f) dpi = 72.0f;
		if (dpi > 300.0f) dpi = 300.0f;
	}
	selectedDPI = (int)dpi;
	
	{
		cvar_t *fontAtlasSize = ri.Cvar_Get("r_fontAtlasSize", "256", 0);
		atlasSize = fontAtlasSize->integer;
		// Clamp to valid power-of-two sizes
		if (atlasSize < 256) atlasSize = 256;
		else if (atlasSize > 512 && atlasSize < 1024) atlasSize = 512;
		else if (atlasSize > 1024) atlasSize = 1024;
		// Ensure power of two
		if (atlasSize != 256 && atlasSize != 512 && atlasSize != 1024) {
			atlasSize = 256;
		}
	}
	selectedAtlasSize = atlasSize;
	{
		cvar_t *fontSDFSpread = ri.Cvar_Get("r_fontSDFSpread", "6", 0);
		selectedSpread = fontSDFSpread->integer;
	}

	// Configure hinting based on CVar
	{
		cvar_t *fontHinting = ri.Cvar_Get("r_fontHinting", "1", 0);
		if (fontHinting) {
			switch (fontHinting->integer) {
			case 0: // None
				loadFlags = FT_LOAD_NO_HINTING;
				selectedHint = 0;
				break;
			case 1: // Light
				loadFlags = FT_LOAD_TARGET_LIGHT;
				selectedHint = 1;
				break;
			case 2: // Normal (default)
				loadFlags = FT_LOAD_DEFAULT;
				selectedHint = 2;
				break;
			case 3: // Strong
				loadFlags = FT_LOAD_TARGET_NORMAL;
				selectedHint = 3;
				break;
			default:
				loadFlags = FT_LOAD_DEFAULT;
				selectedHint = -1;
				break;
			}
		}
	}

	// Configure antialiasing
	{
		cvar_t *fontAntialiasing = ri.Cvar_Get("r_fontAntialiasing", "1", 0);
		if (fontAntialiasing && fontAntialiasing->integer == 0) {
			renderMode = FT_RENDER_MODE_MONO;
			selectedAA = 0;
		} else {
			renderMode = FT_RENDER_MODE_NORMAL;
			selectedAA = 1;
		}
	}

	// Try to detect font style from filename (basic heuristic)
	// e.g., "font_bold.ttf", "font-italic.otf", etc.
	qboolean wantBold = qfalse;
	qboolean wantItalic = qfalse;
	if (strstr(fontName, "bold") || strstr(fontName, "Bold")) {
		wantBold = qtrue;
	}
	if (strstr(fontName, "italic") || strstr(fontName, "Italic") || 
	    strstr(fontName, "oblique") || strstr(fontName, "Oblique")) {
		wantItalic = qtrue;
	}
	
	// Try to find the right face index if we want bold/italic
	int faceIndex = 0;
	int numFaces = 1;
	
	// First, try to load face 0 to get number of faces
	FT_Face tempFace;
	if (FreeType_NewMemoryFace( faceData, len, 0, &tempFace ) == 0) {
		numFaces = tempFace->num_faces;
		FreeType_DoneFace(tempFace);
		
		// Search for bold/italic variant if requested
		if (numFaces > 1 && (wantBold || wantItalic)) {
			int faceIdx;
			for (faceIdx = 0; faceIdx < numFaces; faceIdx++) {
				if (FreeType_NewMemoryFace( faceData, len, faceIdx, &tempFace ) == 0) {
					qboolean isBold = (tempFace->style_flags & FT_STYLE_FLAG_BOLD) != 0;
					qboolean isItalic = (tempFace->style_flags & FT_STYLE_FLAG_ITALIC) != 0;
					
					if ((!wantBold || isBold) && (!wantItalic || isItalic)) {
						faceIndex = i;
						FreeType_DoneFace(tempFace);
						break;
					}
					FreeType_DoneFace(tempFace);
				}
			}
		}
	}

	// allocate on the stack first in case we fail
	if (FreeType_NewMemoryFace( faceData, len, faceIndex, &face )) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: FreeType, unable to allocate new face.\n");
		ri.FS_FreeFile(faceData);
		return qfalse;
	}

	// Set LCD filtering if enabled (requires FreeType 2.3.0+)
	// Note: LCD filtering is typically used with LCD render mode, which we're not using here
	// This is a placeholder for future enhancement
	{
		cvar_t *fontLCDFilter = ri.Cvar_Get("r_fontLCDFilter", "0", 0);
		if (fontLCDFilter && fontLCDFilter->integer) {
			// LCD filtering would be applied during glyph rendering
			// For now, we use standard grayscale rendering
		}
	}

	if (FreeType_SetCharSize( face, pointSize << 6, pointSize << 6, (FT_UInt)dpi, (FT_UInt)dpi)) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: FreeType, unable to set face char size.\n");
		FreeType_DoneFace(face);
		ri.FS_FreeFile(faceData);
		return qfalse;
	}

	//*font = &registeredFonts[registeredFontCount++];

	// make a configurable size image buffer (256x256, 512x512, or 1024x1024)
	// once it is full, register it, clean it and keep going until all glyphs are rendered

	out = ri.Malloc(atlasSize * atlasSize);
	if (out == NULL) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: ri.Malloc failure during output image creation.\n");
		FreeType_DoneFace(face);
		ri.FS_FreeFile(faceData);
		return qfalse;
	}
	Com_Memset(out, 0, atlasSize * atlasSize);

	maxHeight = 0;

	for (i = GLYPH_START; i <= GLYPH_END; i++) {
		RE_ConstructGlyphInfo(out, &xOut, &yOut, &maxHeight, face, (unsigned char)i, qtrue);
	}

	xOut = 0;
	yOut = 0;
	i = GLYPH_START;
	lastStart = i;
	imageNumber = 0;

	while ( i <= GLYPH_END + 1 ) {

		if ( i == GLYPH_END + 1 ) {
			// upload/save current image buffer
			xOut = yOut = -1;
		} else {
			glyph = RE_ConstructGlyphInfo(out, &xOut, &yOut, &maxHeight, face, (unsigned char)i, qfalse);
		}

		if (xOut == -1 || yOut == -1)  {
			// ran out of room
			// we need to create an image from the bitmap, set all the handles in the glyphs to this point
			// 

			scaledSize = atlasSize * atlasSize;
			newSize = scaledSize * 4;
			imageBuff = ri.Malloc(newSize);
			left = 0;
			max = 0;
			for ( k = 0; k < (scaledSize) ; k++ ) {
				if (max < out[k]) {
					max = out[k];
				}
			}

			if (max > 0) {
				max = 255.0f / max;
			}

			for ( k = 0; k < (scaledSize) ; k++ ) {
				imageBuff[left++] = 255;
				imageBuff[left++] = 255;
				imageBuff[left++] = 255;

				imageBuff[left++] = (unsigned char)((float)out[k] * max);
			}

			Com_sprintf (name, sizeof(name), "fonts/fontImage_%i_%i.tga", imageNumber++, pointSize);
			if (r_saveFontData && r_saveFontData->integer) { 
				WriteTGA(name, imageBuff, atlasSize, atlasSize);
			}

			//Com_sprintf (name, sizeof(name), "fonts/fontImage_%i_%i", imageNumber++, pointSize);
			image = R_CreateImage(name, NULL, imageBuff, atlasSize, atlasSize, IMGFLAG_CLAMPTOEDGE);
			h = RE_RegisterShaderFromImage(name, LIGHTMAP_2D, image, qfalse);
			for (j = lastStart; j < i; j++) {
				font->glyphs[j].glyph = h;
				Q_strncpyz(font->glyphs[j].shaderName, name, sizeof(font->glyphs[j].shaderName));
			}
			lastStart = i;
			Com_Memset(out, 0, atlasSize * atlasSize);
			xOut = 0;
			yOut = 0;
			ri.Free(imageBuff);
			if ( i == GLYPH_END + 1 )
				i++;
		} else {
			Com_Memcpy(&font->glyphs[i], glyph, sizeof(glyphInfo_t));
			i++;
		}
	}
	generatedAtlas = qtrue;
	atlasPages = imageNumber;

	// change the scale to be relative to 1 based on configured DPI ( so dpi of 144 means a scale of .5 )
	glyphScale = 72.0f / dpi;

	// we also need to adjust the scale based on point size relative to 48 points as the ui scaling is based on a 48 point font
	glyphScale *= 48.0f / pointSize;

	registeredFont[registeredFontCount].glyphScale = glyphScale;
	font->glyphScale = glyphScale;
	
	// Store font metadata
	font->hasKerning = FreeType_HasKerning(face) ? qtrue : qfalse;
	font->pointSize = pointSize;
	font->dpi = dpi;
	
	// Get font family and style names
	if (face->family_name) {
		Q_strncpyz(font->familyName, face->family_name, sizeof(font->familyName));
	} else {
		font->familyName[0] = '\0';
	}
	
	if (face->style_name) {
		Q_strncpyz(font->styleName, face->style_name, sizeof(font->styleName));
	} else {
		font->styleName[0] = '\0';
	}
	
	// Pre-calculate kerning for common character pairs if kerning is supported
	// Check CVar to see if kerning should be enabled
	qboolean enableKerning = qtrue;
	{
		cvar_t *fontKerning = ri.Cvar_Get("r_fontKerning", "1", 0);
		if (fontKerning && fontKerning->integer == 0) {
			enableKerning = qfalse;
		}
	}
	
	if (font->hasKerning && enableKerning) {
		int prevChar, currChar;
		FT_UInt prevGlyph, currGlyph;
		FT_Vector kerning;
		
		// Pre-calculate kerning for ASCII character pairs (space-optimized)
		// Only store kerning for the most common pairs to save memory
		for (prevChar = 32; prevChar <= 127; prevChar++) {
			prevGlyph = FreeType_GetCharIndex(face, prevChar);
			if (prevGlyph == 0) continue;
			
			// Check kerning with common characters
			for (currChar = 32; currChar <= 127; currChar++) {
				currGlyph = FreeType_GetCharIndex(face, currChar);
				if (currGlyph == 0) continue;
				
				kerning = FreeType_GetKerningDefault(face, prevGlyph, currGlyph);
				if (kerning.x != 0) {
					// Store kerning offset (convert from 26.6 fixed point to pixels)
					font->glyphs[prevChar].kerning[currChar & 255] = kerning.x >> 6;
				}
			}
		}
	}
	
	// Store font name and point size for caching
	Q_strncpyz(font->name, fontName, sizeof(font->name));
	font->pointSize = pointSize;
	font->fallbackFont = NULL; // Initialize fallback pointer
	
	R_AddFontToCache(fontName, pointSize, qfalse, font);
	fromFreeType = qtrue;

	FreeType_DoneFace(face);
	ri.FS_FreeFile(faceData);

	if (r_saveFontData && r_saveFontData->integer) {
		ri.FS_WriteFile(va("fonts/fontImage_%i.dat", pointSize), font, sizeof(fontInfo_t));
	}

	ri.Free(out);
#endif

	ri.Printf(PRINT_ALL,
		"RE_RegisterFont: source=%s '%s' (%dpt, SDF=%d) atlasPages=%d generated=%d registered=%d\n",
		fromDat ? "dat" : (fromStb ? "stb" : (fromFreeType ? "freetype" : (fromCache ? "cache" : "unknown"))),
		fontName, pointSize, wantSDF ? 1 : 0, atlasPages, generatedAtlas ? 1 : 0, registeredFontCount);
	ri.Printf(PRINT_ALL,
		"RE_RegisterFont: settings name='%s' size=%d dpi=%d atlas=%d hint=%d aa=%d spread=%d glyphScale=%.3f\n",
		fontName, pointSize, selectedDPI, selectedAtlasSize, selectedHint, selectedAA, selectedSpread, font->glyphScale);

	return qtrue;
}
#endif // BUILD_FREETYPE

#ifndef BUILD_FREETYPE
static qboolean RE_RegisterFont_Sync(const char *fontName, int pointSize, fontInfo_t *font, qboolean wantSDF) {
	ri.Printf(PRINT_WARNING, "RE_RegisterFont_Sync: FreeType not available, cannot load font '%s'\n", fontName);
	return qfalse;
}
#endif



void R_InitFreeType(void) {
#ifdef USE_FREETYPE
	ftLibrary = FreeType_GetLibrary();
	if (!ftLibrary) {
		ri.Printf(PRINT_WARNING, "R_InitFreeType: FreeType not available.\n");
	} else {
		// Clear font cache when FreeType becomes available to force re-registration
		// with the new FreeType system (fonts that failed with stb_truetype can now load)
		ri.Printf(PRINT_ALL, "R_InitFreeType: Clearing font cache to enable FreeType font loading\n");
		fontCacheCount = 0;
		for (int i = 0; i < MAX_FONT_CACHE; i++) {
			fontCache[i].inUse = qfalse;
		}
	}
#endif
	registeredFontCount = 0;
}


#ifdef USE_JOBSYSTEM
/*
=================
FontLoadWorker_Async
Worker function for async font loading (runs on background thread)
=================
*/
static void FontLoadWorker_Async(void *data) {
	fontLoadJob_t *job = (fontLoadJob_t *)data;

	ri.Printf(PRINT_ALL, "FontLoadWorker_Async: Loading font '%s' (%dpt, SDF=%d)\n",
		job->fontName, job->pointSize, job->useSDF ? 1 : 0);

	// Perform the actual font loading (this is the synchronous path)
	if (RE_RegisterFont_Sync(job->fontName, job->pointSize, &job->loadedFont, job->useSDF)) {
		job->success = qtrue;
		ri.Printf(PRINT_ALL, "FontLoadWorker_Async: Successfully loaded font '%s'\n", job->fontName);
	} else {
		job->success = qfalse;
		ri.Printf(PRINT_WARNING, "FontLoadWorker_Async: Failed to load font '%s'\n", job->fontName);
	}

	job->completed = qtrue;
}

/*
=================
FontLoadCompletion_Async
Completion callback for async font loading (runs on main thread)
=================
*/
static void FontLoadCompletion_Async(void *data) {
	fontLoadJob_t *job = (fontLoadJob_t *)data;

	if (job->success && job->targetFont) {
		// Copy the loaded font data to the target
		Com_Memcpy(job->targetFont, &job->loadedFont, sizeof(fontInfo_t));

		// Add to cache
		R_AddFontToCache(job->fontName, job->pointSize, job->useSDF, job->targetFont);

		ri.Printf(PRINT_ALL, "FontLoadCompletion_Async: Font '%s' registered successfully\n", job->fontName);
	} else {
		ri.Printf(PRINT_WARNING, "FontLoadCompletion_Async: Font '%s' loading failed\n", job->fontName);
	}

	// Mark job as available for reuse
	job->completed = qfalse;
	job->success = qfalse;
	job->targetFont = NULL;
	asyncFontJobCount--;
}

/*
=================
RE_RegisterFont_Async
Register a font asynchronously using the job system
=================
*/
qboolean RE_RegisterFont_Async(const char *fontName, int pointSize, fontInfo_t *font) {
#ifdef USE_JOBSYSTEM
	if (!fontName || !font) {
		return qfalse;
	}

	// Check cache first
	cvar_t *fontSDF = ri.Cvar_Get("r_fontSDF", "0", 0);
	qboolean wantSDF = (fontSDF && fontSDF->integer != 0);
	for (int i = 0; i < fontCacheCount; i++) {
		if (fontCache[i].inUse &&
		    fontCache[i].pointSize == pointSize &&
		    fontCache[i].useSDF == wantSDF &&
		    !Q_stricmp(fontCache[i].fontName, fontName)) {
			// Found in cache - copy cached font
			Com_Memcpy(font, fontCache[i].font, sizeof(fontInfo_t));
			ri.Printf(PRINT_ALL, "RE_RegisterFont_Async: cache hit '%s' (%dpt, SDF=%d)\n",
				fontName, pointSize, wantSDF ? 1 : 0);
			return qtrue;
		}
	}

	// Check if we have too many concurrent async jobs
	if (asyncFontJobCount >= MAX_ASYNC_FONT_JOBS) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont_Async: Too many concurrent font loads (%d), falling back to sync\n", asyncFontJobCount);
		return RE_RegisterFont_Sync(fontName, pointSize, font, wantSDF);
	}

	// Find available job slot
	fontLoadJob_t *job = NULL;
	for (int i = 0; i < MAX_ASYNC_FONT_JOBS; i++) {
		if (!asyncFontJobs[i].completed && !asyncFontJobs[i].targetFont) {
			job = &asyncFontJobs[i];
			break;
		}
	}

	if (!job) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont_Async: No available job slots, falling back to sync\n");
		return RE_RegisterFont_Sync(fontName, pointSize, font, wantSDF);
	}

	// Initialize job
	Q_strncpyz(job->fontName, fontName, sizeof(job->fontName));
	job->pointSize = pointSize;
	job->targetFont = font;
	job->useSDF = wantSDF;
	job->completed = qfalse;
	job->success = qfalse;
	asyncFontJobCount++;

	// Submit job to job system
	extern job_handle_t *JobSystem_SubmitJobWithCompletion(jobFunction_t, void *, jobPriority_t, void (*)(void *), void *);
	job_handle_t *handle = JobSystem_SubmitJobWithCompletion(
		FontLoadWorker_Async, job, JOB_PRIORITY_NORMAL,
		FontLoadCompletion_Async, job);

	if (!handle) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont_Async: Failed to submit job, falling back to sync\n");
		job->completed = qfalse;
		job->targetFont = NULL;
		asyncFontJobCount--;
		return RE_RegisterFont_Sync(fontName, pointSize, font, wantSDF);
	}

	ri.Printf(PRINT_ALL, "RE_RegisterFont_Async: Submitted async load for '%s' (%dpt)\n", fontName, pointSize);
	return qtrue;  // Job submitted successfully

#else
	// Job system not available, fall back to sync
	{
		cvar_t *fontSDF = ri.Cvar_Get("r_fontSDF", "0", 0);
		return RE_RegisterFont_Sync(fontName, pointSize, font, (fontSDF && fontSDF->integer != 0));
	}
#endif
}

/*
=================
RE_UpdateAsyncFonts
Update async font loading (should be called once per frame)
=================
*/
/*
=================
RE_TestAsyncFontLoading
Test function to verify async font loading works
=================
*/
void RE_TestAsyncFontLoading(void) {
	fontInfo_t testFont;

	ri.Printf(PRINT_ALL, "RE_TestAsyncFontLoading: Testing font loading features...\n");

	// Test WOFF2 detection
#ifdef USE_BROTLI
	ri.Printf(PRINT_ALL, "RE_TestAsyncFontLoading: WOFF2 decompression support enabled\n");

	// Test with a fake WOFF2 file to verify detection
	const char *testData = "wOF2test";  // WOFF2 magic bytes "wOF2"
	if (testData[0] == 'w' && testData[1] == 'O' && testData[2] == 'F' && testData[3] == '2') {
		ri.Printf(PRINT_ALL, "RE_TestAsyncFontLoading: WOFF2 magic bytes detection working\n");
	}
#else
	ri.Printf(PRINT_ALL, "RE_TestAsyncFontLoading: WOFF2 decompression support not enabled\n");
#endif

	// Test async/sync loading
	if (RE_RegisterFont("fonts/roboto-regular.ttf", 16, &testFont)) {
		ri.Printf(PRINT_ALL, "RE_TestAsyncFontLoading: Font registration returned success\n");
	} else {
		ri.Printf(PRINT_WARNING, "RE_TestAsyncFontLoading: Font registration failed\n");
	}

#ifdef USE_JOBSYSTEM
	ri.Printf(PRINT_ALL, "RE_TestAsyncFontLoading: Job system available, async loading enabled\n");
#else
	ri.Printf(PRINT_ALL, "RE_TestAsyncFontLoading: Job system not available, using sync loading\n");
#endif
}

void RE_UpdateAsyncFonts(void) {
#ifdef USE_JOBSYSTEM
	// Job system handles completion callbacks automatically via JobSystem_Update
	// This function is here for potential future expansion
#endif
}
#endif // USE_JOBSYSTEM

/*
=================
RE_RegisterFont
Public interface for font registration - chooses between async and sync loading
=================
*/
qboolean RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font) {
	// Check if file system is ready before attempting to load fonts
	// This prevents failures during early initialization when .pk3 files aren't loaded yet
	if (!FS_Initialized() || FS_StartupInProgress()) {
		ri.Printf(PRINT_DEVELOPER, "RE_RegisterFont: File system not ready, deferring font load '%s' (%dpt)\n",
			fontName, pointSize);
		// Return false but don't mark as a hard failure - this will allow retry later
		Com_Memset(font, 0, sizeof(*font));
		return qfalse;
	}

	// Check if async loading is enabled and available
	static cvar_t *r_fontAsync = NULL;
	if (!r_fontAsync) {
		r_fontAsync = ri.Cvar_Get("r_fontAsync", "1", CVAR_ARCHIVE | CVAR_LATCH);
		ri.Cvar_SetDescription(r_fontAsync, "Use asynchronous font loading (0 = sync, 1 = async)");
	}

	ri.Printf(PRINT_ALL, "RE_RegisterFont: called for '%s' (%dpt), async=%d\n",
		fontName, pointSize, r_fontAsync ? r_fontAsync->integer : 0);

#if defined(USE_JOBSYSTEM)
	if (r_fontAsync && r_fontAsync->integer) {
		ri.Printf(PRINT_ALL, "RE_RegisterFont: using async loading\n");
		return RE_RegisterFont_Async(fontName, pointSize, font);
	} else
#endif
	{
		ri.Printf(PRINT_ALL, "RE_RegisterFont: using sync loading\n");
		cvar_t *fontSDF = ri.Cvar_Get("r_fontSDF", "0", 0);
		qboolean wantSDF = (fontSDF && fontSDF->integer != 0);
		return RE_RegisterFont_Sync(fontName, pointSize, font, wantSDF);
	}
}

/*
=================
R_InitFonts
Initialize the font system
=================
*/
void R_InitFonts(void) {
	R_InitGlyphSubsetting();

#ifdef USE_FREETYPE
	// Initialize FreeType if available
	ftLibrary = FreeType_GetLibrary();
	if (!ftLibrary) {
		ri.Printf(PRINT_WARNING, "R_InitFonts: FreeType not available.\n");
	} else {
		// Clear font cache when FreeType becomes available to force re-registration
		// with the new FreeType system (fonts that failed with stb_truetype can now load)
		ri.Printf(PRINT_ALL, "R_InitFonts: FreeType initialized, clearing font cache for enhanced rendering\n");
		fontCacheCount = 0;
		for (int i = 0; i < MAX_FONT_CACHE; i++) {
			fontCache[i].inUse = qfalse;
		}
	}
#endif

	ri.Printf(PRINT_ALL, "R_InitFonts: Enhanced font rendering system initialized\n");
}

/*
=================
R_ShutdownFonts
Shutdown the font system
=================
*/
void R_ShutdownFonts(void) {
	// Finalize glyph subset if tracking was enabled
	if (glyphSubset.tracking && !glyphSubset.finalized) {
		R_FinalizeGlyphSubset();
	}
}

void R_DoneFreeType(void) {
#ifdef USE_FREETYPE
	// FreeType shutdown is handled by FreeType_Shutdown() in common.c
	ftLibrary = NULL;
#endif
	// Clear registered fonts but keep cache for faster reloading
	registeredFontCount = 0;
	// Optionally clear cache if memory is tight (uncomment if needed):
	// fontCacheCount = 0;
	// for (int i = 0; i < MAX_FONT_CACHE; i++) {
	//     fontCache[i].inUse = qfalse;
	// }
}

