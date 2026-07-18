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


#include "q_shared.h"
#include "qcommon.h"
#include "tr_public.h"

#if defined(RENDERER_VULKAN)
#include "../vulkan/tr_common.h"
#else
#error "tr_font.c must be compiled with RENDERER_VULKAN defined"
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
#ifdef FT_LCD_FILTER_H
#include FT_LCD_FILTER_H
#endif

#define _FLOOR(x)  ((x) & -64)
#define _CEIL(x)   (((x)+63) & -64)
#define _TRUNC(x)  ((x) >> 6)

FT_Library ftLibrary = NULL;  
#endif

#ifdef BUILD_FREETYPE
#define MAX_FONTS 16
static int registeredFontCount = 0;
static fontInfo_t registeredFont[MAX_FONTS];
static char registeredTtfPath[MAX_FONTS][MAX_QPATH];
static int registeredPointSize[MAX_FONTS];
static FT_Face registeredFace[MAX_FONTS];
static void *registeredFaceData[MAX_FONTS];

static void R_FontSetupFaceSize( FT_Face face, int pointSize );

static void R_FontReleaseSlotFace( int slot ) {
	if ( slot < 0 || slot >= MAX_FONTS ) {
		return;
	}
	if ( registeredFace[slot] ) {
		FT_Done_Face( registeredFace[slot] );
		registeredFace[slot] = NULL;
	}
	if ( registeredFaceData[slot] ) {
		ri.FS_FreeFile( registeredFaceData[slot] );
		registeredFaceData[slot] = NULL;
	}
	registeredTtfPath[slot][0] = '\0';
	registeredPointSize[slot] = 0;
}

static void R_FontReleaseAllFaces( void ) {
	int i;

	for ( i = 0; i < MAX_FONTS; i++ ) {
		R_FontReleaseSlotFace( i );
	}
}

static void R_FontBindSource( int slot, const char *ttfPath, int pointSize ) {
	if ( slot < 0 || slot >= MAX_FONTS || !ttfPath ) {
		return;
	}
	Q_strncpyz( registeredTtfPath[slot], ttfPath, MAX_QPATH );
	registeredPointSize[slot] = pointSize;
}

static void R_FontBindSourceFromDat( int slot, const char *datName ) {
	const char *base;
	const char *slash;
	const char *underscore;
	const char *dot;
	char prefix[MAX_QPATH];
	char cleanName[64];
	int nameLen;

	if ( slot < 0 || slot >= MAX_FONTS || !datName || !datName[0] ) {
		return;
	}

	slash = strrchr( datName, '/' );
	base = slash ? slash + 1 : datName;
	if ( !Q_strncmp( base, "fontImage_", 10 ) ) {
		return;
	}

	underscore = strrchr( base, '_' );
	dot = strrchr( base, '.' );
	if ( !underscore || !dot || underscore >= dot ) {
		return;
	}

	if ( slash ) {
		nameLen = (int)( slash - datName + 1 );
		if ( nameLen >= (int)sizeof( prefix ) ) {
			nameLen = (int)sizeof( prefix ) - 1;
		}
		Com_Memcpy( prefix, datName, nameLen );
		prefix[nameLen] = '\0';
	} else {
		prefix[0] = '\0';
	}

	nameLen = (int)( underscore - base );
	if ( nameLen >= (int)sizeof( cleanName ) ) {
		nameLen = (int)sizeof( cleanName ) - 1;
	}
	Com_Memcpy( cleanName, base, nameLen );
	cleanName[nameLen] = '\0';

	Com_sprintf( registeredTtfPath[slot], MAX_QPATH, "%s%s.ttf", prefix, cleanName );
	registeredPointSize[slot] = atoi( underscore + 1 );
}

static int R_FontFindSlot( const fontInfo_t *font ) {
	int i;

	if ( !font ) {
		return -1;
	}
	for ( i = 0; i < registeredFontCount; i++ ) {
		if ( font->name[0] && !Q_stricmp( font->name, registeredFont[i].name ) ) {
			return i;
		}
	}
	return -1;
}

static qboolean R_FontEnsureFace( int slot ) {
	void *faceData;
	int len;

	if ( slot < 0 || slot >= registeredFontCount ) {
		return qfalse;
	}
	if ( registeredFace[slot] ) {
		return qtrue;
	}
	if ( !registeredTtfPath[slot][0] || !ftLibrary ) {
		return qfalse;
	}

	len = ri.FS_ReadFile( registeredTtfPath[slot], &faceData );
	if ( len <= 0 ) {
		return qfalse;
	}
	if ( FT_New_Memory_Face( ftLibrary, faceData, len, 0, &registeredFace[slot] ) ) {
		ri.FS_FreeFile( faceData );
		return qfalse;
	}
	registeredFaceData[slot] = faceData;
	R_FontSetupFaceSize( registeredFace[slot], registeredPointSize[slot] );
	return qtrue;
}

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

static qboolean R_FontVerticalHintEnabled( void ) {
	return ri.Cvar_VariableIntegerValue( "r_fontVerticalHint" ) > 0 ? qtrue : qfalse;
}

static qboolean R_FontLcdEnabled( void ) {
	return ri.Cvar_VariableIntegerValue( "r_fontLcd" ) > 0 ? qtrue : qfalse;
}

static int R_FontAtlasSize( void ) {
	int requested = ri.Cvar_VariableIntegerValue( "r_fontAtlasSize" );

	if ( requested <= 256 ) {
		return 256;
	}
	if ( requested <= 512 ) {
		return 512;
	}
	if ( requested <= 1024 ) {
		return 1024;
	}
	return 2048;
}

static void R_FontSetupFaceSize( FT_Face face, int pointSize ) {
	int dpi = R_FontDeviceDpi();

	if ( R_FontVerticalHintEnabled() ) {
		const float scale = 100.0f;
		FT_Matrix matrix;

		matrix.xx = (FT_Fixed)( ( 1.0 / scale ) * 0x10000L );
		matrix.xy = 0;
		matrix.yx = 0;
		matrix.yy = (FT_Fixed)( 1.0 * 0x10000L );
		FT_Set_Transform( face, &matrix, NULL );
		FT_Set_Char_Size( face, ( pointSize << 6 ), 0, (FT_UInt)( dpi * scale ), (FT_UInt)dpi );
	} else {
		FT_Set_Transform( face, NULL, NULL );
		FT_Set_Char_Size( face, pointSize << 6, pointSize << 6, (FT_UInt)dpi, (FT_UInt)dpi );
	}
}

static int R_FontUnhintedAdvance( FT_Face face, FT_UInt gindex ) {
	if ( FT_Load_Glyph( face, gindex, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING ) ) {
		return 0;
	}
	return face->glyph->metrics.horiAdvance >> 6;
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

static void R_FontRuntimeRegKey( char *dst, int dstSize, const char *ttfPath, int pointSize ) {
	unsigned long h = Com_GenerateHashValue( ttfPath, 256 );
	int dpi = R_FontDeviceDpi();
	int atlasSize = R_FontAtlasSize();
	int hint = ri.Cvar_VariableIntegerValue( "r_fontHint" );
	int mip = ri.Cvar_VariableIntegerValue( "r_fontMipmap" ) > 0 ? 1 : 0;
	int vhint = R_FontVerticalHintEnabled() ? 1 : 0;
	int lcd = R_FontLcdEnabled() ? 1 : 0;
	if ( hint < 0 ) {
		hint = 0;
	}
	if ( hint > 2 ) {
		hint = 2;
	}
	Com_sprintf( dst, dstSize, "fonts/_ftr_%lu_%i_d%i_a%i_h%i_m%i_v%i_l%i",
		h, pointSize, dpi, atlasSize, hint, mip, vhint, lcd );
}

static void R_FontAtlasImageName( char *dst, int dstSize, const char *ttfPath, int pageIndex, int pointSize ) {
	unsigned long h = Com_GenerateHashValue( ttfPath, 256 );
	int dpi = R_FontDeviceDpi();
	int atlasSize = R_FontAtlasSize();
	int hint = ri.Cvar_VariableIntegerValue( "r_fontHint" );
	int mip = ri.Cvar_VariableIntegerValue( "r_fontMipmap" ) > 0 ? 1 : 0;
	int vhint = R_FontVerticalHintEnabled() ? 1 : 0;
	int lcd = R_FontLcdEnabled() ? 1 : 0;
	if ( hint < 0 ) {
		hint = 0;
	}
	if ( hint > 2 ) {
		hint = 2;
	}
	Com_sprintf( dst, dstSize, "fonts/_ftg_%lu_%i_%i_d%i_a%i_h%i_m%i_v%i_l%i.tga",
		h, pointSize, pageIndex, dpi, atlasSize, hint, mip, vhint, lcd );
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


static FT_Bitmap *R_RenderGlyph( FT_GlyphSlot glyph, glyphInfo_t *glyphOut, qboolean *ownedBitmap ) {
	FT_Bitmap *bit2;
	int left, right, width, top, bottom, height, pitch, size;

	*ownedBitmap = qfalse;

	if ( R_FontLcdEnabled() ) {
		if ( FT_Render_Glyph( glyph, FT_RENDER_MODE_LCD ) ) {
			return NULL;
		}
		bit2 = &glyph->bitmap;
		glyphOut->height = bit2->rows;
		/* FT LCD bitmaps are 3× wide (RGB subpixels). Pack/UV use logical pixels. */
		glyphOut->pitch = bit2->width / 3;
		if ( glyphOut->pitch < 1 && bit2->width > 0 ) {
			glyphOut->pitch = 1;
		}
		glyphOut->top = ( glyph->metrics.horiBearingY >> 6 ) + 1;
		glyphOut->bottom = _TRUNC( _FLOOR( glyph->metrics.horiBearingY - glyph->metrics.height ) );
		return bit2;
	}

	R_GetGlyphInfo( glyph, &left, &right, &width, &top, &bottom, &height, &pitch );

	if ( glyph->format == ft_glyph_format_outline ) {
		size   = pitch * height;

		bit2 = ri.Malloc( sizeof( FT_Bitmap ) );

		bit2->width      = width;
		bit2->rows       = height;
		bit2->pitch      = pitch;
		bit2->pixel_mode = ft_pixel_mode_grays;
		bit2->buffer     = ri.Malloc( pitch * height );
		bit2->num_grays = 256;

		Com_Memset( bit2->buffer, 0, size );

		FT_Outline_Translate( &glyph->outline, -left, -bottom );

		FT_Outline_Get_Bitmap( ftLibrary, &glyph->outline, bit2 );

		glyphOut->height = height;
		glyphOut->pitch = pitch;
		glyphOut->top = ( glyph->metrics.horiBearingY >> 6 ) + 1;
		glyphOut->bottom = bottom;
		*ownedBitmap = qtrue;

		return bit2;
	} else {
		ri.Printf( PRINT_ALL, "Non-outline fonts are not supported\n" );
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

static glyphInfo_t *RE_ConstructGlyphInfo( unsigned char *imageOut, int *xOut, int *yOut, int *maxHeight, FT_Face face, const unsigned char c, qboolean calcHeight, qboolean lcdAtlas ) {
	int i, j;
	static glyphInfo_t glyph;
	const int atlasSize = R_FontAtlasSize();
	const int atlasEdge = atlasSize - 1;
	unsigned char *src, *dst;
	float scaled_width, scaled_height;
	FT_Bitmap *bitmap = NULL;
	qboolean ownedBitmap = qfalse;
	FT_UInt gindex;
	int unhintedAdvance = 0;

	Com_Memset( &glyph, 0, sizeof( glyphInfo_t ) );
	if ( face != NULL ) {
		gindex = FT_Get_Char_Index( face, c );
		if ( R_FontVerticalHintEnabled() ) {
			unhintedAdvance = R_FontUnhintedAdvance( face, gindex );
		}
		FT_Load_Glyph( face, gindex, R_FontLoadFlags() );
		bitmap = R_RenderGlyph( face->glyph, &glyph, &ownedBitmap );
		if ( bitmap ) {
			if ( unhintedAdvance > 0 ) {
				glyph.xSkip = unhintedAdvance + 1;
			} else {
				glyph.xSkip = ( face->glyph->metrics.horiAdvance >> 6 ) + 1;
			}
		} else {
			return &glyph;
		}

		if (glyph.height > *maxHeight) {
			*maxHeight = glyph.height;
		}

		if (calcHeight) {
			if ( ownedBitmap ) {
				ri.Free( bitmap->buffer );
				ri.Free( bitmap );
			}
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

		/* Glyph larger than the atlas cell cannot be packed — skip without
		 * signaling a page flush, or RegisterFont will create pages forever. */
		if ( scaled_width >= atlasEdge || *maxHeight >= atlasEdge ) {
			if ( ownedBitmap ) {
				ri.Free( bitmap->buffer );
				ri.Free( bitmap );
			}
			return &glyph;
		}

		// we need to make sure we fit
		if (*xOut + scaled_width + 1 >= atlasEdge) {
			*xOut = 0;
			*yOut += *maxHeight + 1;
		}

		if (*yOut + *maxHeight + 1 >= atlasEdge) {
			/* Empty page still too small: skip glyph (do not request another page). */
			if ( *xOut == 0 && *yOut == 0 ) {
				if ( ownedBitmap ) {
					ri.Free( bitmap->buffer );
					ri.Free( bitmap );
				}
				return &glyph;
			}
			*yOut = -1;
			*xOut = -1;
			if ( ownedBitmap ) {
				ri.Free( bitmap->buffer );
				ri.Free( bitmap );
			}
			return &glyph;
		}


		src = bitmap->buffer;
		if ( lcdAtlas && bitmap->pixel_mode == FT_PIXEL_MODE_LCD ) {
			dst = imageOut + ( ( *yOut * atlasSize ) + *xOut ) * 4;
			for ( i = 0; i < glyph.height; i++ ) {
				unsigned char *rowSrc = src + i * bitmap->pitch;
				unsigned char *rowDst = dst + i * ( atlasSize * 4 );
				/* glyph.pitch is logical pixels; FT LCD row is 3 bytes each. */
				for ( j = 0; j < glyph.pitch; j++ ) {
					unsigned char r = rowSrc[j * 3 + 0];
					unsigned char g = rowSrc[j * 3 + 1];
					unsigned char b = rowSrc[j * 3 + 2];
					unsigned char a = r;
					if ( g > a ) {
						a = g;
					}
					if ( b > a ) {
						a = b;
					}
					rowDst[j * 4 + 0] = r;
					rowDst[j * 4 + 1] = g;
					rowDst[j * 4 + 2] = b;
					rowDst[j * 4 + 3] = a;
				}
			}
		} else {
			dst = imageOut + ( *yOut * atlasSize ) + *xOut;

			if (bitmap->pixel_mode == ft_pixel_mode_mono) {
				for (i = 0; i < glyph.height; i++) {
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
		}

		// we now have an 8 bit per pixel grey scale bitmap 
		// that is width wide and pf->ftSize->metrics.y_ppem tall

		glyph.imageHeight = scaled_height;
		glyph.imageWidth = scaled_width;
		glyph.s = (float)*xOut / (float)atlasSize;
		glyph.t = (float)*yOut / (float)atlasSize;
		glyph.s2 = glyph.s + (float)scaled_width / (float)atlasSize;
		glyph.t2 = glyph.t + (float)scaled_height / (float)atlasSize;

		*xOut += scaled_width + 1;

		if ( ownedBitmap ) {
			ri.Free( bitmap->buffer );
			ri.Free( bitmap );
		}
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

void RE_ClearTrueTypeFontCache( void ) {
	R_FontReleaseAllFaces();
	registeredFontCount = 0;
	Com_Memset( registeredFont, 0, sizeof( registeredFont ) );
	ri.Printf( PRINT_DEVELOPER, "RE_ClearTrueTypeFontCache: TrueType registration cache cleared\n" );
}

float RE_GetFontKerning( const fontInfo_t *font, int prevIndex, int nextIndex ) {
	FT_UInt prevGindex;
	FT_UInt nextGindex;
	FT_Vector delta;
	int slot;

	if ( !font || prevIndex < 0 || nextIndex < 0 ) {
		return 0.0f;
	}
	prevIndex &= 255;
	nextIndex &= 255;

	slot = R_FontFindSlot( font );
	if ( slot < 0 || !R_FontEnsureFace( slot ) ) {
		return 0.0f;
	}

	prevGindex = FT_Get_Char_Index( registeredFace[slot], (unsigned char)prevIndex );
	nextGindex = FT_Get_Char_Index( registeredFace[slot], (unsigned char)nextIndex );
	if ( !prevGindex || !nextGindex ) {
		return 0.0f;
	}
	if ( FT_Get_Kerning( registeredFace[slot], prevGindex, nextGindex, FT_KERNING_DEFAULT, &delta ) ) {
		return 0.0f;
	}
	return (float)( delta.x >> 6 );
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
	qboolean preferRuntimeTtf = qfalse;

	if (!fontName) {
		ri.Printf(PRINT_ALL, "RE_RegisterFont: called with empty name\n");
		return;
	}

	resolvedFontName = fontName;
	if ( !Q_stricmp( fontName, "fonts/impact.ttf" ) || !Q_stricmp( fontName, "impact.ttf" ) ) {
		resolvedFontName = "fonts/Inter-Bold.ttf";
		ri.Printf( PRINT_WARNING, "RE_RegisterFont: Falling back from '%s' to '%s'\n", fontName, resolvedFontName );
	}
	/* Prefer modern Inter when callers ask for Source Sans (plan fallback). */
	if ( !Q_stricmpn( fontName, "fonts/SourceSans", 16 ) || !Q_stricmpn( fontName, "SourceSans", 10 ) ) {
		resolvedFontName = "fonts/Inter-Regular.ttf";
		ri.Printf( PRINT_WARNING, "RE_RegisterFont: Falling back from '%s' to '%s'\n", fontName, resolvedFontName );
	}

	if (pointSize <= 0) {
		pointSize = 12;
	}

	if (registeredFontCount >= MAX_FONTS) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: Too many fonts registered already.\n");
		return;
	}

	/* TrueType/OpenType paths always rasterize via FreeType so console/HUD
	   never get stuck on stale prebaked fonts/<name>_<pt>.dat atlases. */
	{
		const char *ext = strrchr( resolvedFontName, '.' );
		if ( ext && ( !Q_stricmp( ext, ".ttf" ) || !Q_stricmp( ext, ".otf" ) || !Q_stricmp( ext, ".ttc" ) ) ) {
			preferRuntimeTtf = qtrue;
		}
	}
	if ( preferRuntimeTtf ) {
		goto try_freetype;
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
	Com_Memcpy( &registeredFont[registeredFontCount], font, sizeof( fontInfo_t ) );
	R_FontBindSourceFromDat( registeredFontCount, name );
	registeredFontCount++;
	ri.FS_FreeFile( faceData );
	return;

try_freetype:
{
	char runtimeRegKey[MAX_QPATH];
	const int regSlot = registeredFontCount;

	R_FontRuntimeRegKey( runtimeRegKey, sizeof( runtimeRegKey ), resolvedFontName, pointSize );
	for ( i = 0; i < registeredFontCount; i++ ) {
		if ( !Q_stricmp( runtimeRegKey, registeredFont[i].name ) ) {
			Com_Memcpy( font, &registeredFont[i], sizeof( fontInfo_t ) );
			return;
		}
	}

	if (ftLibrary == NULL) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: FreeType not initialized.\n");
		return;
	}

	len = ri.FS_ReadFile(resolvedFontName, &faceData);
	if (len <= 0) {
		static const char *const kFontFallbacks[] = {
			"fonts/Inter-Regular.ttf",
			"fonts/Inter-Bold.ttf",
			NULL
		};
		int fb;
		qboolean loaded = qfalse;

		ri.Printf(PRINT_WARNING, "RE_RegisterFont: Unable to read font file '%s'\n", resolvedFontName);
		for ( fb = 0; kFontFallbacks[fb]; fb++ ) {
			if ( !Q_stricmp( resolvedFontName, kFontFallbacks[fb] ) ) {
				continue;
			}
			len = ri.FS_ReadFile( kFontFallbacks[fb], &faceData );
			if ( len > 0 ) {
				ri.Printf( PRINT_WARNING, "RE_RegisterFont: Falling back to '%s'\n", kFontFallbacks[fb] );
				resolvedFontName = kFontFallbacks[fb];
				loaded = qtrue;
				break;
			}
		}
		if ( !loaded ) {
			return;
		}
	}

	if ( FT_New_Memory_Face( ftLibrary, faceData, len, 0, &face ) ) {
		ri.Printf( PRINT_WARNING, "RE_RegisterFont: FreeType, unable to allocate new face.\n" );
		ri.FS_FreeFile( faceData );
		return;
	}

	R_FontSetupFaceSize( face, pointSize );
	R_FontBindSource( regSlot, resolvedFontName, pointSize );
	registeredFace[regSlot] = face;
	registeredFaceData[regSlot] = faceData;

	{
		const qboolean lcdAtlas = R_FontLcdEnabled();
		const int atlasSize = R_FontAtlasSize();
		const int atlasBytes = lcdAtlas ? ( atlasSize * atlasSize * 4 ) : ( atlasSize * atlasSize );

		out = ri.Malloc( atlasBytes );
		if ( out == NULL ) {
			ri.Printf( PRINT_WARNING, "RE_RegisterFont: ri.Malloc failure during output image creation.\n" );
			R_FontReleaseSlotFace( regSlot );
			return;
		}
		Com_Memset(out, 0, atlasBytes);

	maxHeight = 0;
	xOut = 0;
	yOut = 0;
	for (i = GLYPH_START; i <= GLYPH_END; i++) {
		RE_ConstructGlyphInfo(out, &xOut, &yOut, &maxHeight, face, (unsigned char)i, qtrue, lcdAtlas);
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
			glyph = RE_ConstructGlyphInfo(out, &xOut, &yOut, &maxHeight, face, (unsigned char)i, qfalse, lcdAtlas);
		}

		if (xOut == -1 || yOut == -1) {
			/* Safety: LCD/dpi packing bugs used to create pages forever. */
			if ( imageNumber >= 64 ) {
				ri.Printf( PRINT_WARNING, "RE_RegisterFont: aborting atlas for '%s' @ %dpt after %d pages\n",
					resolvedFontName, pointSize, imageNumber );
				ri.Free( out );
				R_FontReleaseSlotFace( regSlot );
				return;
			}
			scaledSize = atlasSize * atlasSize;
			newSize = scaledSize * 4;
			imageBuff = ri.Malloc(newSize);
			if ( lcdAtlas ) {
				Com_Memcpy( imageBuff, out, newSize );
			} else {
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
			}

			R_FontAtlasImageName( name, sizeof( name ), resolvedFontName, imageNumber, pointSize );
			imageNumber++;
			if (r_saveFontData->integer) {
				WriteTGA(name, imageBuff, atlasSize, atlasSize);
			}

	#if defined(RENDERER_VULKAN)
		image = R_CreateImage(name, NULL, imageBuff, atlasSize, atlasSize, R_FontAtlasFlags(), 0, 0);
	#else
		image = R_CreateImage(name, NULL, imageBuff, atlasSize, atlasSize, R_FontAtlasFlags());
	#endif
			h = RE_RegisterShaderFromImage(name, LIGHTMAP_2D, image, qfalse);
			for (j = lastStart; j < i; j++) {
				font->glyphs[j].glyph = h;
				Q_strncpyz(font->glyphs[j].shaderName, name, sizeof(font->glyphs[j].shaderName));
			}
			lastStart = i;
			Com_Memset(out, 0, atlasBytes);
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
	Q_strncpyz( font->name, runtimeRegKey, sizeof( font->name ) );
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

	ri.Free( out );
	}
}
}
void R_InitFreeType(void) {
	if ( FT_Init_FreeType( &ftLibrary ) ) {
		ri.Printf( PRINT_WARNING, "R_InitFreeType: Unable to initialize FreeType.\n" );
	} else {
#ifdef FT_LCD_FILTER_H
		FT_Library_SetLcdFilter( ftLibrary, FT_LCD_FILTER_DEFAULT );
#endif
		ri.Printf( PRINT_ALL, "FreeType: TrueType raster dpi=%i (r_fontDpi), hint=%i (r_fontHint), atlas=%ix%i (r_fontAtlasSize), atlas mipmaps=%s (r_fontMipmap), verticalHint=%i (r_fontVerticalHint), lcd=%i (r_fontLcd); Rougier HAL-00821839 / HAL-05430837 (kerning); apply with reloadTtf or vid_restart\n",
			R_FontDeviceDpi(), ri.Cvar_VariableIntegerValue( "r_fontHint" ),
			R_FontAtlasSize(), R_FontAtlasSize(),
			ri.Cvar_VariableIntegerValue( "r_fontMipmap" ) > 0 ? "on" : "off",
			ri.Cvar_VariableIntegerValue( "r_fontVerticalHint" ),
			ri.Cvar_VariableIntegerValue( "r_fontLcd" ) );
	}
	R_FontReleaseAllFaces();
	registeredFontCount = 0;
}


void R_DoneFreeType(void) {
	R_FontReleaseAllFaces();
	if ( ftLibrary ) {
		FT_Done_FreeType( ftLibrary );
		ftLibrary = NULL;
	}
	registeredFontCount = 0;
}

#endif
