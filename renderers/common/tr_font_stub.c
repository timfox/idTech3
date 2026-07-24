/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Font loading stub for non-FreeType builds.
Loads pre-cached .dat font files and their atlas textures so that
custom fonts work even without a FreeType dependency at runtime.
Falls back to the bitmap charset for uncached fonts.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "tr_public.h"

#if !defined(BUILD_FREETYPE)

extern qhandle_t RE_RegisterShaderNoMip( const char *name );

#define MAX_FONTS_STUB 16
static int registeredFontCount = 0;
static fontInfo_t registeredFont[MAX_FONTS_STUB];

static int fdOffset;
static byte *fdFile;

/*
 * UTF-8 fix: Byte 0xC2 is the start byte for 2-byte sequences U+0080..U+00BF
 * (e.g. © = 0xC2 0xA9). UI draws byte-by-byte, so 0xC2 was shown as Â.
 * Make glyph 0xC2 zero-width so only the continuation byte's glyph is drawn.
 */
static void Font_ApplyUtf8GlyphFix( fontInfo_t *font ) {
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
	int i = ((unsigned int)fdFile[fdOffset] |
		((unsigned int)fdFile[fdOffset+1]<<8) |
		((unsigned int)fdFile[fdOffset+2]<<16) |
		((unsigned int)fdFile[fdOffset+3]<<24));
	fdOffset += 4;
	return i;
}

static float readFloat( void ) {
	union { byte b[4]; float f; } u;
#if defined Q3_BIG_ENDIAN
	u.b[0] = fdFile[fdOffset+3];
	u.b[1] = fdFile[fdOffset+2];
	u.b[2] = fdFile[fdOffset+1];
	u.b[3] = fdFile[fdOffset+0];
#else
	u.b[0] = fdFile[fdOffset+0];
	u.b[1] = fdFile[fdOffset+1];
	u.b[2] = fdFile[fdOffset+2];
	u.b[3] = fdFile[fdOffset+3];
#endif
	fdOffset += 4;
	return u.f;
}

static qboolean Font_LoadCached( const char *datName, fontInfo_t *font ) {
	void *faceData;
	int len, i;

	len = ri.FS_ReadFile( datName, NULL );
	if ( len != sizeof( fontInfo_t ) ) {
		return qfalse;
	}

	ri.FS_ReadFile( datName, &faceData );
	if ( !faceData ) {
		return qfalse;
	}

	fdOffset = 0;
	fdFile = (byte *)faceData;

	for ( i = 0; i < GLYPHS_PER_FONT; i++ ) {
		font->glyphs[i].height      = readInt();
		font->glyphs[i].top         = readInt();
		font->glyphs[i].bottom      = readInt();
		font->glyphs[i].pitch       = readInt();
		font->glyphs[i].xSkip       = readInt();
		font->glyphs[i].imageWidth   = readInt();
		font->glyphs[i].imageHeight  = readInt();
		font->glyphs[i].s            = readFloat();
		font->glyphs[i].t            = readFloat();
		font->glyphs[i].s2           = readFloat();
		font->glyphs[i].t2           = readFloat();
		font->glyphs[i].glyph        = readInt();
		Q_strncpyz( font->glyphs[i].shaderName,
			(const char *)&fdFile[fdOffset],
			sizeof( font->glyphs[i].shaderName ) );
		fdOffset += sizeof( font->glyphs[i].shaderName );
	}
	font->glyphScale = readFloat();
	Com_Memcpy( font->name, &fdFile[fdOffset], MAX_QPATH );

	Q_strncpyz( font->name, datName, sizeof( font->name ) );
	for ( i = GLYPH_START; i <= GLYPH_END; i++ ) {
		font->glyphs[i].glyph = RE_RegisterShaderNoMip( font->glyphs[i].shaderName );
	}

	Font_ApplyUtf8GlyphFix( font );

	ri.FS_FreeFile( faceData );

	ri.Printf( PRINT_DEVELOPER, "Font loaded from cache: %s\n", datName );
	return qtrue;
}

void RE_RegisterFont( const char *fontName, int pointSize, fontInfo_t *font ) {
	int i;
	char datPath[MAX_QPATH];
	char namedDat[MAX_QPATH];

	if ( !fontName ) {
		ri.Printf( PRINT_ALL, "RE_RegisterFont: called with empty name\n" );
		return;
	}

	if ( pointSize <= 0 ) {
		pointSize = 12;
	}

	if ( registeredFontCount >= MAX_FONTS_STUB ) {
		ri.Printf( PRINT_WARNING, "RE_RegisterFont: too many fonts registered (%d)\n", MAX_FONTS_STUB );
		return;
	}

	Com_sprintf( datPath, sizeof( datPath ), "fonts/fontImage_%i.dat", pointSize );

	for ( i = 0; i < registeredFontCount; i++ ) {
		if ( Q_stricmp( datPath, registeredFont[i].name ) == 0 ) {
			Com_Memcpy( font, &registeredFont[i], sizeof( fontInfo_t ) );
			return;
		}
	}

	if ( fontName[0] && Q_stricmp( fontName, "default" ) != 0 ) {
		const char *baseName = fontName;
		const char *slash = strrchr( fontName, '/' );
		if ( slash ) baseName = slash + 1;
		const char *dot = strrchr( baseName, '.' );

		if ( dot ) {
			int nameLen = (int)( dot - baseName );
			char cleanName[64];
			if ( nameLen >= (int)sizeof( cleanName ) ) nameLen = (int)sizeof( cleanName ) - 1;
			Com_Memcpy( cleanName, baseName, nameLen );
			cleanName[nameLen] = '\0';
			Com_sprintf( namedDat, sizeof( namedDat ), "fonts/%s_%i.dat", cleanName, pointSize );
		} else {
			Com_sprintf( namedDat, sizeof( namedDat ), "fonts/%s_%i.dat", baseName, pointSize );
		}

		for ( i = 0; i < registeredFontCount; i++ ) {
			if ( Q_stricmp( namedDat, registeredFont[i].name ) == 0 ) {
				Com_Memcpy( font, &registeredFont[i], sizeof( fontInfo_t ) );
				return;
			}
		}

		if ( Font_LoadCached( namedDat, font ) ) {
			Com_Memcpy( &registeredFont[registeredFontCount++], font, sizeof( fontInfo_t ) );
			return;
		}
	}

	if ( Font_LoadCached( datPath, font ) ) {
		Com_Memcpy( &registeredFont[registeredFontCount++], font, sizeof( fontInfo_t ) );
		return;
	}

	ri.Printf( PRINT_DEVELOPER, "RE_RegisterFont: no cached font data for '%s' at %dpt\n", fontName, pointSize );
}

qboolean RE_RegisterFontAtlas( const char *fontName, int pointSize, const char *alphabet, fontAtlasInfo_t *out ) {
	(void)fontName;
	(void)pointSize;
	(void)alphabet;
	if ( out ) {
		Com_Memset( out, 0, sizeof( *out ) );
	}
	ri.Printf( PRINT_WARNING, "RE_RegisterFontAtlas: FreeType not available\n" );
	return qfalse;
}

void RE_ClearTrueTypeFontCache( void ) {
	registeredFontCount = 0;
	Com_Memset( registeredFont, 0, sizeof( registeredFont ) );
}

float RE_GetFontKerning( const fontInfo_t *font, int prevIndex, int nextIndex ) {
	(void)font;
	(void)prevIndex;
	(void)nextIndex;
	return 0.0f;
}

void R_InitFreeType( void ) {
	registeredFontCount = 0;
	ri.Printf( PRINT_ALL,
		"[Font] stub: cached .dat fonts only (build with -DBUILD_FREETYPE=ON for TTF rasterization)\n" );
}

void R_DoneFreeType( void ) {
	registeredFontCount = 0;
}

#endif
