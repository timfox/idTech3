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


#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#include "tr_public.h"

#if defined(RENDERER_VULKAN)
#include "../vulkan/tr_common.h"
#elif defined(RENDERER_OPENGL)
#include "../opengl/tr_common.h"
#else
#error "tr_font.c must be compiled with either RENDERER_VULKAN or RENDERER_OPENGL defined"
#endif

extern void R_IssuePendingRenderCommands( void );
extern qhandle_t RE_RegisterShaderNoMip( const char *name );

#ifdef BUILD_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ERRORS_H
#include FT_SYSTEM_H
#include FT_IMAGE_H
#include FT_OUTLINE_H

#define _FLOOR(x)  ((x) & -64)
#define _CEIL(x)   (((x)+63) & -64)
#define _TRUNC(x)  ((x) >> 6)

FT_Library ftLibrary = NULL;  
#endif

#ifdef BUILD_FREETYPE
#define MAX_FONTS 16
static int registeredFontCount = 0;
static fontInfo_t registeredFont[MAX_FONTS];

static int R_FontDeviceDpi( void ) {
	int d = ri.Cvar_VariableIntegerValue( "r_fontDpi" );
	if ( d < 72 ) {
		d = 72;
	}
	if ( d > 144 ) {
		d = 144;
	}
	return d;
}

static FT_Int32 R_FontLoadFlags( void ) {
	int mode = ri.Cvar_VariableIntegerValue( "r_fontHint" );
	FT_Int32 flags = FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP;
	if ( mode == 0 ) {
		return flags;
	}
	if ( mode >= 2 ) {
		flags |= (FT_Int32)FT_LOAD_TARGET_NORMAL;
	} else {
		flags |= (FT_Int32)FT_LOAD_TARGET_LIGHT;
	}
	return flags;
}

static imgFlags_t R_FontAtlasFlags( void ) {
	imgFlags_t f = IMGFLAG_CLAMPTOEDGE;
	if ( ri.Cvar_VariableIntegerValue( "r_fontMipmap" ) > 0 ) {
		f |= IMGFLAG_MIPMAP;
	}
	return f;
}

static void R_GetGlyphInfo(FT_GlyphSlot glyph, int *left, int *right, int *width, int *top, int *bottom, int *height, int *pitch) {
	*left  = _FLOOR( glyph->metrics.horiBearingX );
	*right = _CEIL( glyph->metrics.horiBearingX + glyph->metrics.width );
	*width = _TRUNC(*right - *left);

	*top    = _CEIL( glyph->metrics.horiBearingY );
	*bottom = _FLOOR( glyph->metrics.horiBearingY - glyph->metrics.height );
	*height = _TRUNC( *top - *bottom );
	*pitch  = ( qtrue ? (*width+3) & -4 : (*width+7) >> 3 );
}


static FT_Bitmap *R_RenderGlyph(FT_GlyphSlot glyph, glyphInfo_t* glyphOut) {
	FT_Bitmap  *bit2;
	int left, right, width, top, bottom, height, pitch, size;

	R_GetGlyphInfo(glyph, &left, &right, &width, &top, &bottom, &height, &pitch);

	if ( glyph->format == ft_glyph_format_outline ) {
		size   = pitch*height; 

		bit2 = ri.Malloc(sizeof(FT_Bitmap));

		bit2->width      = width;
		bit2->rows       = height;
		bit2->pitch      = pitch;
		bit2->pixel_mode = ft_pixel_mode_grays;
		//bit2->pixel_mode = ft_pixel_mode_mono;
		bit2->buffer     = ri.Malloc(pitch*height);
		bit2->num_grays = 256;

		Com_Memset( bit2->buffer, 0, size );

		FT_Outline_Translate( &glyph->outline, -left, -bottom );

		FT_Outline_Get_Bitmap( ftLibrary, &glyph->outline, bit2 );

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

static glyphInfo_t *RE_ConstructGlyphInfo(unsigned char *imageOut, int *xOut, int *yOut, int *maxHeight, FT_Face face, const unsigned char c, qboolean calcHeight) {
	int i;
	static glyphInfo_t glyph;
	unsigned char *src, *dst;
	float scaled_width, scaled_height;
	FT_Bitmap *bitmap = NULL;

	Com_Memset(&glyph, 0, sizeof(glyphInfo_t));
	// make sure everything is here
	if (face != NULL) {
		FT_Load_Glyph( face, FT_Get_Char_Index( face, c ), R_FontLoadFlags() );
		bitmap = R_RenderGlyph(face->glyph, &glyph);
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

		// we need to make sure we fit
		if (*xOut + scaled_width + 1 >= 255) {
			*xOut = 0;
			*yOut += *maxHeight + 1;
		}

		if (*yOut + *maxHeight + 1 >= 255) {
			*yOut = -1;
			*xOut = -1;
			ri.Free(bitmap->buffer);
			ri.Free(bitmap);
			return &glyph;
		}


		src = bitmap->buffer;
		dst = imageOut + (*yOut * 256) + *xOut;

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
				dst += 256;
			}
		} else {
			for (i = 0; i < glyph.height; i++) {
				Com_Memcpy(dst, src, glyph.pitch);
				src += glyph.pitch;
				dst += 256;
			}
		}

		// we now have an 8 bit per pixel grey scale bitmap 
		// that is width wide and pf->ftSize->metrics.y_ppem tall

		glyph.imageHeight = scaled_height;
		glyph.imageWidth = scaled_width;
		glyph.s = (float)*xOut / 256;
		glyph.t = (float)*yOut / 256;
		glyph.s2 = glyph.s + (float)scaled_width / 256;
		glyph.t2 = glyph.t + (float)scaled_height / 256;

		*xOut += scaled_width + 1;

		ri.Free(bitmap->buffer);
		ri.Free(bitmap);
	}

	return &glyph;
}
static int readInt( void );
static float readFloat( void );

static int fdOffset;
static byte	*fdFile;

/*
 * UTF-8 fix: Byte 0xC2 is the start byte for 2-byte sequences U+0080..U+00BF
 * (e.g. © = 0xC2 0xA9). UI draws byte-by-byte, so 0xC2 was shown as Â.
 * Make glyph 0xC2 zero-width so only the continuation byte's glyph is drawn.
 */
static void RE_ApplyUtf8GlyphFix( fontInfo_t *font ) {
	glyphInfo_t *g = &font->glyphs[0xC2];
	g->xSkip = 0;
	g->height = 0;
	g->pitch = 0;
	g->imageWidth = 0;
	g->imageHeight = 0;
	g->top = 0;
	g->bottom = 0;
	g->s2 = g->s;
	g->t2 = g->t;
}

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

void RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font) {
	FT_Face face;
	int j, k, xOut, yOut, lastStart, imageNumber;
	int scaledSize, newSize, maxHeight, left;
	unsigned char *out, *imageBuff;
	glyphInfo_t *glyph;
	image_t *image;
	qhandle_t h;
	float max;
	int dpi = R_FontDeviceDpi();
	float glyphScale;
	void *faceData;
	int i, len;
	char name[MAX_QPATH];
	const char *resolvedFontName;

	if (!fontName) {
		ri.Printf(PRINT_ALL, "RE_RegisterFont: called with empty name\n");
		return;
	}

	resolvedFontName = fontName;
	if ( !Q_stricmp( fontName, "fonts/impact.ttf" ) || !Q_stricmp( fontName, "impact.ttf" ) ) {
		resolvedFontName = "fonts/Inter-Bold.ttf";
		ri.Printf( PRINT_WARNING, "RE_RegisterFont: Falling back from '%s' to '%s'\n", fontName, resolvedFontName );
	}

	if (pointSize <= 0) {
		pointSize = 12;
	}

	if (registeredFontCount >= MAX_FONTS) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: Too many fonts registered already.\n");
		return;
	}

	/* --- Try name-based cache first: fonts/<fontbase>_<pointSize>.dat --- */
	{
		char namedDat[MAX_QPATH];
		const char *baseName = resolvedFontName;
		const char *slash = strrchr(resolvedFontName, '/');
		const char *dot;
		if (slash) baseName = slash + 1;
		dot = strrchr(baseName, '.');

		if (dot) {
			int nameLen = (int)(dot - baseName);
			char cleanName[64];
			if (nameLen >= (int)sizeof(cleanName)) nameLen = (int)sizeof(cleanName) - 1;
			Com_Memcpy(cleanName, baseName, nameLen);
			cleanName[nameLen] = '\0';
			Com_sprintf(namedDat, sizeof(namedDat), "fonts/%s_%i.dat", cleanName, pointSize);
		} else {
			Com_sprintf(namedDat, sizeof(namedDat), "fonts/%s_%i.dat", baseName, pointSize);
		}

		for (i = 0; i < registeredFontCount; i++) {
			if (Q_stricmp(namedDat, registeredFont[i].name) == 0) {
				Com_Memcpy(font, &registeredFont[i], sizeof(fontInfo_t));
				return;
			}
		}

		len = ri.FS_ReadFile(namedDat, NULL);
		if (len == sizeof(fontInfo_t)) {
			Q_strncpyz(name, namedDat, sizeof(name));
			goto load_cached_dat;
		}
	}

	/* --- Fall back to legacy point-size-only lookup --- */
	Com_sprintf(name, sizeof(name), "fonts/fontImage_%i.dat", pointSize);
	for (i = 0; i < registeredFontCount; i++) {
		if (Q_stricmp(name, registeredFont[i].name) == 0) {
			Com_Memcpy(font, &registeredFont[i], sizeof(fontInfo_t));
			return;
		}
	}

	len = ri.FS_ReadFile(name, NULL);
	if (len == sizeof(fontInfo_t)) {
		goto load_cached_dat;
	}

	goto try_freetype;

load_cached_dat:
	ri.FS_ReadFile(name, &faceData);
	fdOffset = 0;
	fdFile = faceData;
	for (i = 0; i < GLYPHS_PER_FONT; i++) {
		font->glyphs[i].height = readInt();
		font->glyphs[i].top = readInt();
		font->glyphs[i].bottom = readInt();
		font->glyphs[i].pitch = readInt();
		font->glyphs[i].xSkip = readInt();
		font->glyphs[i].imageWidth = readInt();
		font->glyphs[i].imageHeight = readInt();
		font->glyphs[i].s = readFloat();
		font->glyphs[i].t = readFloat();
		font->glyphs[i].s2 = readFloat();
		font->glyphs[i].t2 = readFloat();
		font->glyphs[i].glyph = readInt();
		Q_strncpyz(font->glyphs[i].shaderName, (const char *)&fdFile[fdOffset], sizeof(font->glyphs[i].shaderName));
		fdOffset += sizeof(font->glyphs[i].shaderName);
	}
	font->glyphScale = readFloat();
	Com_Memcpy(font->name, &fdFile[fdOffset], MAX_QPATH);

	Q_strncpyz(font->name, name, sizeof(font->name));
	for (i = GLYPH_START; i <= GLYPH_END; i++) {
		font->glyphs[i].glyph = RE_RegisterShaderNoMip(font->glyphs[i].shaderName);
	}
	RE_ApplyUtf8GlyphFix( font );
	Com_Memcpy(&registeredFont[registeredFontCount++], font, sizeof(fontInfo_t));
	ri.FS_FreeFile(faceData);
	return;

try_freetype:

	if (ftLibrary == NULL) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: FreeType not initialized.\n");
		return;
	}

	len = ri.FS_ReadFile(resolvedFontName, &faceData);
	if (len <= 0) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: Unable to read font file '%s'\n", resolvedFontName);
		return;
	}

	if (FT_New_Memory_Face(ftLibrary, faceData, len, 0, &face)) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: FreeType, unable to allocate new face.\n");
		return;
	}

	if ( FT_Set_Char_Size( face, pointSize << 6, pointSize << 6, (FT_UInt)dpi, (FT_UInt)dpi ) ) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: FreeType, unable to set face char size.\n");
		return;
	}

	out = ri.Malloc(256 * 256);
	if (out == NULL) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: ri.Malloc failure during output image creation.\n");
		return;
	}
	Com_Memset(out, 0, 256 * 256);

	maxHeight = 0;
	for (i = GLYPH_START; i <= GLYPH_END; i++) {
		RE_ConstructGlyphInfo(out, &xOut, &yOut, &maxHeight, face, (unsigned char)i, qtrue);
	}

	xOut = 0;
	yOut = 0;
	i = GLYPH_START;
	lastStart = i;
	imageNumber = 0;

	while (i <= GLYPH_END + 1) {
		if (i == GLYPH_END + 1) {
			xOut = yOut = -1;
		} else {
			glyph = RE_ConstructGlyphInfo(out, &xOut, &yOut, &maxHeight, face, (unsigned char)i, qfalse);
		}

		if (xOut == -1 || yOut == -1) {
			scaledSize = 256 * 256;
			newSize = scaledSize * 4;
			imageBuff = ri.Malloc(newSize);
			left = 0;
			max = 0;
			for (k = 0; k < scaledSize; k++) {
				if (max < out[k]) {
					max = out[k];
				}
			}

			if (max > 0) {
				max = 255 / max;
			}

			for (k = 0; k < scaledSize; k++) {
				imageBuff[left++] = 255;
				imageBuff[left++] = 255;
				imageBuff[left++] = 255;
				imageBuff[left++] = ((float)out[k] * max);
			}

			Com_sprintf(name, sizeof(name), "fonts/fontImage_%i_%i.tga", imageNumber++, pointSize);
			if (r_saveFontData->integer) {
				WriteTGA(name, imageBuff, 256, 256);
			}

	#if defined(RENDERER_VULKAN)
		image = R_CreateImage(name, NULL, imageBuff, 256, 256, R_FontAtlasFlags(), 0, 0);
	#else
		image = R_CreateImage(name, NULL, imageBuff, 256, 256, R_FontAtlasFlags());
	#endif
			h = RE_RegisterShaderFromImage(name, LIGHTMAP_2D, image, qfalse);
			for (j = lastStart; j < i; j++) {
				font->glyphs[j].glyph = h;
				Q_strncpyz(font->glyphs[j].shaderName, name, sizeof(font->glyphs[j].shaderName));
			}
			lastStart = i;
			Com_Memset(out, 0, 256 * 256);
			xOut = 0;
			yOut = 0;
			ri.Free(imageBuff);
			if (i == GLYPH_END + 1) {
				i++;
			}
		} else {
			Com_Memcpy(&font->glyphs[i], glyph, sizeof(glyphInfo_t));
			i++;
		}
	}

	glyphScale = 72.0f / (float)dpi;
	glyphScale *= 48.0f / (float)pointSize;

	registeredFont[registeredFontCount].glyphScale = glyphScale;
	font->glyphScale = glyphScale;
	RE_ApplyUtf8GlyphFix( font );
	Com_Memcpy(&registeredFont[registeredFontCount++], font, sizeof(fontInfo_t));

	if (r_saveFontData->integer) {
		ri.FS_WriteFile(va("fonts/fontImage_%i.dat", pointSize), font, sizeof(fontInfo_t));

		/* Also save a name-based .dat for direct lookup */
		{
			const char *baseName = fontName;
			const char *slash = strrchr(fontName, '/');
			const char *dot;
			char cleanName[64];
			if (slash) baseName = slash + 1;
			dot = strrchr(baseName, '.');
			if (dot) {
				int nl = (int)(dot - baseName);
				if (nl >= (int)sizeof(cleanName)) nl = (int)sizeof(cleanName) - 1;
				Com_Memcpy(cleanName, baseName, nl);
				cleanName[nl] = '\0';
			} else {
				Q_strncpyz(cleanName, baseName, sizeof(cleanName));
			}
			ri.FS_WriteFile(va("fonts/%s_%i.dat", cleanName, pointSize), font, sizeof(fontInfo_t));
		}
	}

	ri.Free(out);
	ri.FS_FreeFile(faceData);
}
void R_InitFreeType(void) {
	if ( FT_Init_FreeType( &ftLibrary ) ) {
		ri.Printf( PRINT_WARNING, "R_InitFreeType: Unable to initialize FreeType.\n" );
	} else {
		ri.Printf( PRINT_ALL, "FreeType: TrueType raster dpi=%i (r_fontDpi), hint=%i (r_fontHint 0=default 1=light 2=normal), atlas mipmaps=%s (r_fontMipmap; restart after change)\n",
			R_FontDeviceDpi(), ri.Cvar_VariableIntegerValue( "r_fontHint" ),
			ri.Cvar_VariableIntegerValue( "r_fontMipmap" ) > 0 ? "on" : "off" );
	}
	registeredFontCount = 0;
}


void R_DoneFreeType(void) {
	if (ftLibrary) {
		FT_Done_FreeType( ftLibrary );
		ftLibrary = NULL;
	}
	registeredFontCount = 0;
}

#endif

