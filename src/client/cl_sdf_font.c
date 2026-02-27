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

typedef struct {
	char            name[64];
	qboolean        active;
	qhandle_t       atlasShader;
	sdfGlyph_t      glyphs[SDF_MAX_GLYPHS];
	int             numGlyphs;
	float           lineHeight;
	float           base;
	float           atlasW;
	float           atlasH;
} sdfFont_t;

static sdfFont_t fonts[SDF_MAX_FONTS];
static int numFonts = 0;
static cvar_t *r_sdfFont;
static cvar_t *r_sdfSmoothing;

#define VALID_FONT(h) ((h) >= 0 && (h) < numFonts && fonts[(h)].active)

void SDF_Init( void ) {
	Com_Memset( fonts, 0, sizeof( fonts ) );
	numFonts = 0;

	r_sdfFont = Cvar_Get( "r_sdfFont", "", CVAR_ARCHIVE );
	Cvar_SetDescription( r_sdfFont, "SDF font to use for UI text (e.g. fonts/myfont). Expects fonts/myfont.fnt + fonts/myfont.tga." );

	r_sdfSmoothing = Cvar_Get( "r_sdfSmoothing", "0.1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_sdfSmoothing, "SDF edge smoothing width (smaller = sharper, 0.05-0.25 typical)." );

	Com_Printf( "SDF fonts: initialized\n" );
}

void SDF_Shutdown( void ) {
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
			sscanf( line, "common lineHeight=%f base=%f scaleW=%f scaleH=%f",
				&font->lineHeight, &font->base, &font->atlasW, &font->atlasH );
		}
		else if ( !Q_strncmp( line, "char ", 5 ) ) {
			int id = 0;
			float x = 0, y = 0, w = 0, h = 0, xoff = 0, yoff = 0, xadv = 0;
			sscanf( line, "char id=%d x=%f y=%f width=%f height=%f xoffset=%f yoffset=%f xadvance=%f",
				&id, &x, &y, &w, &h, &xoff, &yoff, &xadv );

			if ( id >= 0 && id < SDF_MAX_GLYPHS && font->atlasW > 0 && font->atlasH > 0 ) {
				sdfGlyph_t *g = &font->glyphs[id];
				g->id = id;
				g->x = x; g->y = y; g->w = w; g->h = h;
				g->xoffset = xoff; g->yoffset = yoff;
				g->xadvance = xadv;
				g->s0 = x / font->atlasW;
				g->t0 = y / font->atlasH;
				g->s1 = (x + w) / font->atlasW;
				g->t1 = (y + h) / font->atlasH;
				if ( id >= font->numGlyphs ) font->numGlyphs = id + 1;
			}
		}
	}

	return font->numGlyphs > 0;
}

sdfFontHandle_t SDF_LoadFont( const char *name, const char *atlasImage, const char *metricsFile ) {
	void *buf;
	int len, slot;

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

void SDF_DrawText( sdfFontHandle_t font, float x, float y, float scale,
	const char *text, const float *color, const sdfDrawParams_t *params )
{
	sdfFont_t *f;
	float curX, curY;
	const char *s;

	(void)params;

	if ( !VALID_FONT( font ) || !text || !text[0] ) return;
	f = &fonts[font];

	if ( color ) {
		re.SetColor( color );
	}

	curX = x;
	curY = y;

	for ( s = text; *s; s++ ) {
		int ch = (unsigned char)*s;

		if ( ch == '\n' ) {
			curX = x;
			curY += f->lineHeight * scale;
			continue;
		}

		if ( ch < 0 || ch >= f->numGlyphs ) continue;

		sdfGlyph_t *g = &f->glyphs[ch];
		if ( g->w <= 0 || g->h <= 0 ) {
			curX += g->xadvance * scale;
			continue;
		}

		float drawX = curX + g->xoffset * scale;
		float drawY = curY + g->yoffset * scale;
		float drawW = g->w * scale;
		float drawH = g->h * scale;

		re.DrawStretchPic( drawX, drawY, drawW, drawH,
			g->s0, g->t0, g->s1, g->t1, f->atlasShader );

		curX += g->xadvance * scale;
	}

	re.SetColor( NULL );
}

float SDF_TextWidth( sdfFontHandle_t font, float scale, const char *text ) {
	sdfFont_t *f;
	const char *s;
	float width = 0;

	if ( !VALID_FONT( font ) || !text ) return 0;
	f = &fonts[font];

	for ( s = text; *s; s++ ) {
		int ch = (unsigned char)*s;
		if ( ch >= 0 && ch < f->numGlyphs ) {
			width += f->glyphs[ch].xadvance * scale;
		}
	}

	return width;
}

float SDF_TextHeight( sdfFontHandle_t font, float scale ) {
	if ( !VALID_FONT( font ) ) return 16.0f;
	return fonts[font].lineHeight * scale;
}
