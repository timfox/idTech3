/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

SDF font implementation.

Loads BMFont-format .fnt metric files and SDF atlas textures.
Renders text using the standard engine DrawStretchPic with the
SDF alpha channel providing resolution-independent edge detection.

For full SDF shader effects (outline, shadow, glow), use the
dedicated sdf_text.frag shader via a custom render path.
For basic sharp text, the standard alpha-tested rendering with
a high-resolution SDF atlas already provides a major improvement
over the 256x256 bitmap font.
===========================================================================
*/

#include "client.h"
#include "cl_sdf_font.h"
#include "cl_emoji.h"
#include "../qcommon/q_utf8.h"

#define SDF_MAX_METRICS_FILE_SIZE (1024 * 1024)

typedef struct {
	char            name[64];
	qboolean        active;
	qhandle_t       atlasShader;
	sdfGlyph_t      glyphs[SDF_MAX_GLYPHS];
	sdfKerning_t    kernings[SDF_MAX_KERNINGS];
	int             numGlyphs;
	int             numKernings;
	float           lineHeight;
	float           base;
	float           atlasW;
	float           atlasH;
} sdfFont_t;

static sdfFont_t fonts[SDF_MAX_FONTS];
static int numFonts = 0;
static sdfFontHandle_t defaultFontHandle = SDF_INVALID_HANDLE;
static char loadedFontBase[MAX_QPATH];
static char loadedMetricsPath[MAX_QPATH];
static char loadedAtlasPath[MAX_QPATH];

static cvar_t *r_sdfEnable;
static cvar_t *r_sdfFont;
static cvar_t *r_sdfFontMetrics;
static cvar_t *r_sdfFontAtlas;
static cvar_t *r_sdfSmoothing;

#define VALID_FONT(h) ((h) >= 0 && (h) < numFonts && fonts[(h)].active)

static const sdfGlyph_t *SDF_FindGlyph( const sdfFont_t *font, uint32_t codepoint ) {
	int i;

	if ( !font ) {
		return NULL;
	}

	for ( i = 0; i < font->numGlyphs; i++ ) {
		if ( (uint32_t)font->glyphs[i].id == codepoint ) {
			return &font->glyphs[i];
		}
	}

	return NULL;
}

static float SDF_FindKerning( const sdfFont_t *font, uint32_t first, uint32_t second ) {
	int i;

	if ( !font || first == 0 || second == 0 ) {
		return 0.0f;
	}

	for ( i = 0; i < font->numKernings; i++ ) {
		const sdfKerning_t *k = &font->kernings[i];
		if ( k->first == first && k->second == second ) {
			return k->amount;
		}
	}

	return 0.0f;
}

static float SDF_LineScale( const sdfFont_t *font, float size ) {
	const float base = ( font && font->lineHeight > 0.0f ) ? font->lineHeight : 16.0f;
	return size / base;
}

static qboolean SDF_ParseCommonLine( sdfFont_t *font, const char *line ) {
	float lineHeight = 0.0f, base = 0.0f, atlasW = 0.0f, atlasH = 0.0f;
	const int parsed = sscanf( line, "common lineHeight=%f base=%f scaleW=%f scaleH=%f",
		&lineHeight, &base, &atlasW, &atlasH );

	if ( parsed != 4 ) {
		return qfalse;
	}
	if ( lineHeight <= 0.0f || atlasW <= 0.0f || atlasH <= 0.0f ) {
		return qfalse;
	}

	font->lineHeight = lineHeight;
	font->base = base;
	font->atlasW = atlasW;
	font->atlasH = atlasH;
	return qtrue;
}

static qboolean SDF_ParseCharLine( sdfFont_t *font, const char *line ) {
	int id = 0;
	float x = 0, y = 0, w = 0, h = 0, xoff = 0, yoff = 0, xadv = 0;
	sdfGlyph_t *g;

	const int parsed = sscanf( line,
		"char id=%d x=%f y=%f width=%f height=%f xoffset=%f yoffset=%f xadvance=%f",
		&id, &x, &y, &w, &h, &xoff, &yoff, &xadv );

	if ( parsed != 8 ) {
		return qfalse;
	}
	if ( id < 0 || font->atlasW <= 0.0f || font->atlasH <= 0.0f ) {
		return qfalse;
	}

	if ( font->numGlyphs >= SDF_MAX_GLYPHS ) {
		return qfalse;
	}

	g = &font->glyphs[font->numGlyphs++];
	Com_Memset( g, 0, sizeof( *g ) );
	g->id = id;
	g->x = x;
	g->y = y;
	g->w = w;
	g->h = h;
	g->xoffset = xoff;
	g->yoffset = yoff;
	g->xadvance = xadv;
	g->s0 = x / font->atlasW;
	g->t0 = y / font->atlasH;
	g->s1 = ( x + w ) / font->atlasW;
	g->t1 = ( y + h ) / font->atlasH;
	return qtrue;
}

static qboolean SDF_ParseKerningLine( sdfFont_t *font, const char *line ) {
	int first = 0;
	int second = 0;
	float amount = 0.0f;
	sdfKerning_t *k;
	const int parsed = sscanf( line, "kerning first=%d second=%d amount=%f", &first, &second, &amount );

	if ( parsed != 3 ) {
		return qfalse;
	}
	if ( first < 0 || second < 0 ) {
		return qfalse;
	}
	if ( font->numKernings >= SDF_MAX_KERNINGS ) {
		return qfalse;
	}

	k = &font->kernings[font->numKernings++];
	k->first = (uint32_t)first;
	k->second = (uint32_t)second;
	k->amount = amount;
	return qtrue;
}

void SDF_Init( void ) {
	Com_Memset( fonts, 0, sizeof( fonts ) );
	numFonts = 0;
	defaultFontHandle = SDF_INVALID_HANDLE;
	loadedFontBase[0] = '\0';
	loadedMetricsPath[0] = '\0';
	loadedAtlasPath[0] = '\0';

	r_sdfEnable = Cvar_Get( "r_sdfEnable", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_sdfEnable, "Enable SDF HUD text rendering for supported paths (0 = off, 1 = on)." );

	r_sdfFont = Cvar_Get( "r_sdfFont", "", CVAR_ARCHIVE );
	Cvar_SetDescription( r_sdfFont, "SDF font base name (e.g. fonts/myfont). Expects myfont.fnt + image atlas." );
	r_sdfFontMetrics = Cvar_Get( "r_sdfFontMetrics", "", CVAR_ARCHIVE );
	Cvar_SetDescription( r_sdfFontMetrics, "Optional explicit path to BMFont metrics file (.fnt)." );
	r_sdfFontAtlas = Cvar_Get( "r_sdfFontAtlas", "", CVAR_ARCHIVE );
	Cvar_SetDescription( r_sdfFontAtlas, "Optional explicit atlas image path for SDF font." );

	r_sdfSmoothing = Cvar_Get( "r_sdfSmoothing", "0.1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_sdfSmoothing, "SDF edge smoothing width (smaller = sharper, 0.05-0.25 typical)." );

	Com_Printf( "SDF fonts: initialized\n" );
}

void SDF_Shutdown( void ) {
	defaultFontHandle = SDF_INVALID_HANDLE;
	loadedFontBase[0] = '\0';
	loadedMetricsPath[0] = '\0';
	loadedAtlasPath[0] = '\0';
	numFonts = 0;
}

/* Parse a BMFont .fnt text format file */
static qboolean SDF_ParseFNT( sdfFont_t *font, const char *data ) {
	const char *p = data;
	char line[1024];

	while ( p && *p ) {
		int i = 0;
		while ( *p && *p != '\n' && *p != '\r' && i < (int)sizeof(line) - 1 ) {
			line[i++] = *p++;
		}
		line[i] = '\0';
		while ( *p == '\n' || *p == '\r' ) p++;

		if ( !Q_strncmp( line, "common ", 7 ) ) {
			SDF_ParseCommonLine( font, line );
		}
		else if ( !Q_strncmp( line, "char ", 5 ) ) {
			SDF_ParseCharLine( font, line );
		}
		else if ( !Q_strncmp( line, "kerning ", 8 ) ) {
			SDF_ParseKerningLine( font, line );
		}
	}

	return font->numGlyphs > 0;
}

sdfFontHandle_t SDF_LoadFont( const char *name, const char *atlasImage, const char *metricsFile ) {
	void *buf;
	int len, slot;

	if ( !name || !name[0] || !atlasImage || !atlasImage[0] || !metricsFile || !metricsFile[0] ) {
		Com_Printf( S_COLOR_YELLOW "SDF: invalid font arguments\n" );
		return SDF_INVALID_HANDLE;
	}

	if ( numFonts >= SDF_MAX_FONTS ) return SDF_INVALID_HANDLE;

	slot = numFonts++;
	Com_Memset( &fonts[slot], 0, sizeof( sdfFont_t ) );
	Q_strncpyz( fonts[slot].name, name, sizeof( fonts[slot].name ) );

	/* Load atlas texture */
	if ( re.RegisterShaderNoMip ) {
		fonts[slot].atlasShader = re.RegisterShaderNoMip( atlasImage );
	}
	if ( !fonts[slot].atlasShader ) {
		Com_Printf( S_COLOR_YELLOW "SDF: atlas '%s' not found\n", atlasImage );
		numFonts--;
		return SDF_INVALID_HANDLE;
	}

	/* Load metrics file */
	len = FS_ReadFile( metricsFile, &buf );
	if ( len <= 0 || !buf ) {
		Com_Printf( S_COLOR_YELLOW "SDF: metrics '%s' not found\n", metricsFile );
		numFonts--;
		return SDF_INVALID_HANDLE;
	}
	if ( len > SDF_MAX_METRICS_FILE_SIZE ) {
		Com_Printf( S_COLOR_YELLOW "SDF: metrics '%s' too large (%d bytes, max %d)\n",
			metricsFile, len, SDF_MAX_METRICS_FILE_SIZE );
		FS_FreeFile( buf );
		numFonts--;
		return SDF_INVALID_HANDLE;
	}

	if ( !SDF_ParseFNT( &fonts[slot], (const char *)buf ) ) {
		Com_Printf( S_COLOR_YELLOW "SDF: failed to parse '%s'\n", metricsFile );
		FS_FreeFile( buf );
		numFonts--;
		return SDF_INVALID_HANDLE;
	}

	FS_FreeFile( buf );
	fonts[slot].active = qtrue;

	Com_Printf( "SDF: loaded '%s' (%d glyphs, %.0fx%.0f atlas)\n",
		name, fonts[slot].numGlyphs, fonts[slot].atlasW, fonts[slot].atlasH );

	return slot;
}

void SDF_FreeFont( sdfFontHandle_t h ) {
	if ( VALID_FONT( h ) ) fonts[h].active = qfalse;
}

static qboolean SDF_ResolveDefaultPaths( char *metricsPath, size_t metricsPathSize, char *atlasPath, size_t atlasPathSize ) {
	if ( r_sdfFontMetrics && r_sdfFontMetrics->string[0] ) {
		Q_strncpyz( metricsPath, r_sdfFontMetrics->string, metricsPathSize );
	} else {
		if ( !r_sdfFont || !r_sdfFont->string[0] ) {
			return qfalse;
		}
		Com_sprintf( metricsPath, metricsPathSize, "%s.fnt", r_sdfFont->string );
	}

	if ( r_sdfFontAtlas && r_sdfFontAtlas->string[0] ) {
		Q_strncpyz( atlasPath, r_sdfFontAtlas->string, atlasPathSize );
	} else {
		if ( !r_sdfFont || !r_sdfFont->string[0] ) {
			return qfalse;
		}
		Q_strncpyz( atlasPath, r_sdfFont->string, atlasPathSize );
	}

	return qtrue;
}

static qboolean SDF_EnsureDefaultFont( void ) {
	char metricsPath[MAX_QPATH];
	char atlasPath[MAX_QPATH];
	const char *fontBase;

	if ( !r_sdfEnable || !r_sdfEnable->integer ) {
		return qfalse;
	}
	if ( !r_sdfFont || !r_sdfFont->string[0] ) {
		return qfalse;
	}

	fontBase = r_sdfFont->string;
	if ( !SDF_ResolveDefaultPaths( metricsPath, sizeof( metricsPath ), atlasPath, sizeof( atlasPath ) ) ) {
		return qfalse;
	}

	if ( defaultFontHandle != SDF_INVALID_HANDLE &&
		 VALID_FONT( defaultFontHandle ) &&
		 !Q_stricmp( loadedFontBase, fontBase ) &&
		 !Q_stricmp( loadedMetricsPath, metricsPath ) &&
		 !Q_stricmp( loadedAtlasPath, atlasPath ) ) {
		return qtrue;
	}

	if ( defaultFontHandle != SDF_INVALID_HANDLE ) {
		SDF_FreeFont( defaultFontHandle );
		defaultFontHandle = SDF_INVALID_HANDLE;
	}

	defaultFontHandle = SDF_LoadFont( fontBase, atlasPath, metricsPath );
	if ( defaultFontHandle == SDF_INVALID_HANDLE ) {
		return qfalse;
	}

	Q_strncpyz( loadedFontBase, fontBase, sizeof( loadedFontBase ) );
	Q_strncpyz( loadedMetricsPath, metricsPath, sizeof( loadedMetricsPath ) );
	Q_strncpyz( loadedAtlasPath, atlasPath, sizeof( loadedAtlasPath ) );
	return qtrue;
}

qboolean SDF_IsEnabled( void ) {
	return SDF_EnsureDefaultFont();
}

void SDF_DrawText( sdfFontHandle_t font, float x, float y, float scale,
	const char *text, const float *color, const sdfDrawParams_t *params )
{
	sdfFont_t *f;
	float curX, curY;
	const char *s;
	uint32_t prev = 0;

	(void)params;

	if ( !VALID_FONT( font ) || !text || !text[0] ) return;
	f = &fonts[font];

	if ( color ) {
		re.SetColor( color );
	}

	s = text;
	curX = x;
	curY = y;

	while ( *s ) {
		uint32_t cp;
		const sdfGlyph_t *g;
		float advance;

		if ( Q_IsColorString( s ) ) {
			s += 2;
			continue;
		}

		cp = Q_UTF8_Decode( &s );
		if ( cp == '\n' ) {
			curX = x;
			curY += f->lineHeight * scale;
			prev = 0;
			continue;
		}

		g = SDF_FindGlyph( f, cp );
		if ( !g ) {
			g = SDF_FindGlyph( f, (uint32_t)'?' );
			if ( !g ) {
				prev = 0;
				continue;
			}
		}

		advance = ( g->xadvance + SDF_FindKerning( f, prev, cp ) ) * scale;

		if ( g->w > 0.0f && g->h > 0.0f ) {
			float drawX = curX + g->xoffset * scale;
			float drawY = curY + g->yoffset * scale;
			float drawW = g->w * scale;
			float drawH = g->h * scale;
			float ax = drawX;
			float ay = drawY;
			float aw = drawW;
			float ah = drawH;

			SCR_AdjustFrom640( &ax, &ay, &aw, &ah );

			re.DrawStretchPic( ax, ay, aw, ah,
				g->s0, g->t0, g->s1, g->t1, f->atlasShader );
		}

		curX += advance;
		prev = cp;
	}

	re.SetColor( NULL );
}

qboolean SDF_DrawStringExt( int x, int y, float size, const char *string,
	const float *setColor, qboolean forceColor, qboolean noColorEscape ) {
	const char *s;
	sdfFont_t *f;
	float xx;
	float yy;
	uint32_t prevCp;
	vec4_t color;
	float scale;
	const float clampedSize = Com_Clamp( 1.0f, 256.0f, size );
	const sdfGlyph_t *fallbackGlyph;

	if ( !string || !string[0] || !setColor ) {
		return qfalse;
	}
	if ( !SDF_EnsureDefaultFont() ) {
		return qfalse;
	}
	if ( !VALID_FONT( defaultFontHandle ) ) {
		return qfalse;
	}
	f = &fonts[defaultFontHandle];

	scale = SDF_LineScale( f, clampedSize );
	fallbackGlyph = SDF_FindGlyph( f, (uint32_t)'?' );

	/* drop shadow pass */
	color[0] = color[1] = color[2] = 0.0f;
	color[3] = setColor[3];
	re.SetColor( color );
	s = string;
	xx = (float)x;
	yy = (float)y;
	prevCp = 0;
	while ( *s ) {
		const char *prevPtr = s;
		uint32_t cp;
		const sdfGlyph_t *g;

		if ( !noColorEscape && Q_IsColorString( s ) ) {
			s += 2;
			continue;
		}

		cp = Q_UTF8_Decode( &s );
		if ( cp == '\n' ) {
			xx = (float)x;
			yy += f->lineHeight * scale;
			prevCp = 0;
			continue;
		}

		if ( CL_Emoji_IsEnabled() && ( (unsigned char)prevPtr[0] >= 0x80 ) && Q_UTF8_IsEmoji( cp ) ) {
			xx += clampedSize;
			prevCp = cp;
			continue;
		}

		g = SDF_FindGlyph( f, cp );
		if ( !g ) {
			g = fallbackGlyph;
		}
		if ( !g ) {
			xx += clampedSize;
			prevCp = cp;
			continue;
		}

		if ( g->w > 0.0f && g->h > 0.0f ) {
			float drawX = xx + 2.0f + g->xoffset * scale;
			float drawY = yy + 2.0f + g->yoffset * scale;
			float drawW = g->w * scale;
			float drawH = g->h * scale;
			float ax = drawX;
			float ay = drawY;
			float aw = drawW;
			float ah = drawH;
			SCR_AdjustFrom640( &ax, &ay, &aw, &ah );
			re.DrawStretchPic( ax, ay, aw, ah, g->s0, g->t0, g->s1, g->t1, f->atlasShader );
		}

		xx += ( g->xadvance + SDF_FindKerning( f, prevCp, cp ) ) * scale;
		prevCp = cp;
	}

	/* color pass */
	s = string;
	xx = (float)x;
	yy = (float)y;
	Com_Memcpy( color, setColor, sizeof( color ) );
	re.SetColor( setColor );
	prevCp = 0;
	while ( *s ) {
		const char *prevPtr = s;
		uint32_t cp;
		const sdfGlyph_t *g;

		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				Com_Memcpy( color, g_color_table[ColorIndexFromChar( *(s + 1) )], sizeof( color ) );
				color[3] = setColor[3];
				re.SetColor( color );
			}
			if ( !noColorEscape ) {
				s += 2;
				continue;
			}
		}

		cp = Q_UTF8_Decode( &s );
		if ( cp == '\n' ) {
			xx = (float)x;
			yy += f->lineHeight * scale;
			prevCp = 0;
			continue;
		}

		if ( CL_Emoji_IsEnabled() && ( (unsigned char)prevPtr[0] >= 0x80 ) && Q_UTF8_IsEmoji( cp ) ) {
			if ( CL_Emoji_DrawChar( (int)xx, (int)yy, clampedSize, clampedSize, cp ) ) {
				re.SetColor( forceColor ? setColor : color );
				xx += clampedSize;
				prevCp = cp;
				continue;
			}
		}

		g = SDF_FindGlyph( f, cp );
		if ( !g ) {
			g = fallbackGlyph;
		}
		if ( !g ) {
			xx += clampedSize;
			prevCp = cp;
			continue;
		}

		if ( g->w > 0.0f && g->h > 0.0f ) {
			float drawX = xx + g->xoffset * scale;
			float drawY = yy + g->yoffset * scale;
			float drawW = g->w * scale;
			float drawH = g->h * scale;
			float ax = drawX;
			float ay = drawY;
			float aw = drawW;
			float ah = drawH;
			SCR_AdjustFrom640( &ax, &ay, &aw, &ah );
			re.DrawStretchPic( ax, ay, aw, ah, g->s0, g->t0, g->s1, g->t1, f->atlasShader );
		}

		xx += ( g->xadvance + SDF_FindKerning( f, prevCp, cp ) ) * scale;
		prevCp = cp;
	}

	re.SetColor( NULL );
	return qtrue;
}

float SDF_TextWidth( sdfFontHandle_t font, float scale, const char *text ) {
	sdfFont_t *f;
	const char *s, *prevPtr;
	const sdfGlyph_t *g, *fallbackGlyph;
	float width = 0;
	float lineWidth = 0;
	uint32_t prevCp = 0;

	if ( !VALID_FONT( font ) || !text ) return 0;
	f = &fonts[font];
	fallbackGlyph = SDF_FindGlyph( f, (uint32_t)'?' );
	s = text;

	while ( *s ) {
		uint32_t cp;

		if ( Q_IsColorString( s ) ) {
			s += 2;
			continue;
		}

		prevPtr = s;
		cp = Q_UTF8_Decode( &s );
		if ( cp == '\n' ) {
			if ( lineWidth > width ) {
				width = lineWidth;
			}
			lineWidth = 0;
			prevCp = 0;
			continue;
		}

		if ( CL_Emoji_IsEnabled() && ( (unsigned char)prevPtr[0] >= 0x80 ) && Q_UTF8_IsEmoji( cp ) ) {
			lineWidth += f->lineHeight * scale;
			prevCp = cp;
			continue;
		}

		g = SDF_FindGlyph( f, cp );
		if ( !g ) {
			g = fallbackGlyph;
		}
		if ( g ) {
			lineWidth += ( g->xadvance + SDF_FindKerning( f, prevCp, cp ) ) * scale;
			prevCp = cp;
		}
	}

	if ( lineWidth > width ) {
		width = lineWidth;
	}

	return width;
}

float SDF_TextHeight( sdfFontHandle_t font, float scale ) {
	if ( !VALID_FONT( font ) ) return 16.0f;
	return fonts[font].lineHeight * scale;
}
