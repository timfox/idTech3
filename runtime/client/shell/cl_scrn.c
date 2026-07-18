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
// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

#include "client.h"
#include "cl_emoji.h"
#include "cl_voip.h"
#include "cl_menuvideo.h"
#include "cl_superhud.h"
#include "cl_sdf_font.h"
#include "cl_vector_font.h"
#include "q_utf8.h"

static qboolean	scr_initialized;		// ready to draw

cvar_t		*cl_timegraph;
static cvar_t		*cl_debuggraph;
static cvar_t		*cl_graphheight;
static cvar_t		*cl_graphscale;
static cvar_t		*cl_graphshift;
static cvar_t		*ui_scale;
static cvar_t		*r_fontConsoleAlign;
static cvar_t		*r_fontConsoleProportional;
static cvar_t		*r_fontShadow;
static cvar_t		*r_fontSubpixel;
static cvar_t		*r_fontSubpixelPos;
static cvar_t		*r_fontKerning;
static cvar_t		*r_textMode;
static cvar_t		*cl_builtInTtfConsole;

static qboolean SCR_ConsoleTtfEnabled( void ) {
	return ( qboolean)( Cvar_VariableIntegerValue( "cl_builtInTtf" ) &&
		cl_builtInTtfConsole && cl_builtInTtfConsole->integer &&
		cls.builtInTtfActive );
}

static void SCR_DrawStretchPicTtf( float ax, float ay, float aw, float ah,
		float s1, float t1, float s2, float t2, qhandle_t shader ) {
	if ( r_fontSubpixelPos && r_fontSubpixelPos->integer && re.DrawStretchPicSubpixel ) {
		float shift = ax - floorf( ax );
		if ( shift < 0.0f ) {
			shift += 1.0f;
		}
		re.DrawStretchPicSubpixel( ax, ay, aw, ah, s1, t1, s2, t2, shader, shift );
	} else {
		re.DrawStretchPic( ax, ay, aw, ah, s1, t1, s2, t2, shader );
	}
}

/*
================
SCR_DrawNamedPic

Coordinates are 640*480 virtual values
=================
*/
void SCR_DrawNamedPic( float x, float y, float width, float height, const char *picname ) {
	qhandle_t	hShader;

	assert( width != 0 );

	hShader = re.RegisterShader( picname );
	SCR_AdjustFrom640( &x, &y, &width, &height );
	re.DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}


/*
================
SCR_AdjustFrom640

Adjusted for resolution and screen aspect ratio
================
*/
void SCR_AdjustFrom640( float *x, float *y, float *w, float *h ) {
	float scale;
	float offsetX;
	float offsetY;
	float uiScale;

	scale = (float)cls.glconfig.vidWidth / 640.0f;
	{
		const float yScale = (float)cls.glconfig.vidHeight / 480.0f;
		if ( yScale < scale ) {
			scale = yScale;
		}
	}

	offsetX = ( cls.glconfig.vidWidth - ( 640.0f * scale ) ) * 0.5f;
	offsetY = ( cls.glconfig.vidHeight - ( 480.0f * scale ) ) * 0.5f;

	uiScale = 1.0f;
	if ( ui_scale ) {
		uiScale = Com_Clamp( 0.5f, 4.0f, ui_scale->value );
	}

	if ( x ) {
		const float vx = ( ( *x - 320.0f ) * uiScale ) + 320.0f;
		*x = ( vx * scale ) + offsetX;
	}
	if ( y ) {
		const float vy = ( ( *y - 240.0f ) * uiScale ) + 240.0f;
		*y = ( vy * scale ) + offsetY;
	}
	if ( w ) {
		*w = ( *w * uiScale ) * scale;
	}
	if ( h ) {
		*h = ( *h * uiScale ) * scale;
	}
}

/*
================
SCR_FillRect

Coordinates are 640*480 virtual values
=================
*/
void SCR_FillRect( float x, float y, float width, float height, const float *color ) {
	re.SetColor( color );

	SCR_AdjustFrom640( &x, &y, &width, &height );
	re.DrawStretchPic( x, y, width, height, 0, 0, 0, 0, cls.whiteShader );

	re.SetColor( NULL );
}


/*
================
SCR_DrawPic

Coordinates are 640*480 virtual values
=================
*/
void SCR_DrawPic( float x, float y, float width, float height, qhandle_t hShader ) {
	SCR_AdjustFrom640( &x, &y, &width, &height );
	re.DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}


/*
================
SCR_TtfPointSizeForPixelHeight

Pick a FreeType point size so cap height is at least targetPx at r_fontDpi.
================
*/
static int SCR_TtfPointSizeForPixelHeight( int targetPx ) {
	int dpi;
	int pt;

	if ( targetPx < 8 ) {
		targetPx = 8;
	}
	if ( targetPx > 96 ) {
		targetPx = 96;
	}

	dpi = Cvar_VariableIntegerValue( "r_fontDpi" );
	if ( dpi < 72 ) {
		dpi = 72;
	}
	if ( dpi > 144 ) {
		dpi = 144;
	}

	pt = ( targetPx * 72 + dpi - 1 ) / dpi;
	if ( pt < 8 ) {
		pt = 8;
	}
	if ( pt > 72 ) {
		pt = 72;
	}
	return pt;
}

static float SCR_UiPixelScale( void ) {
	float pixelScale;

	if ( cls.glconfig.vidHeight > 0 && cls.glconfig.vidWidth > 0 ) {
		pixelScale = (float)cls.glconfig.vidHeight / 480.0f;
		{
			const float sx = (float)cls.glconfig.vidWidth / 640.0f;
			if ( sx < pixelScale ) {
				pixelScale = sx;
			}
		}
	} else {
		pixelScale = 1.0f;
	}

	if ( ui_scale ) {
		pixelScale *= Com_Clamp( 0.5f, 4.0f, ui_scale->value );
	}

	return pixelScale;
}

static int SCR_ComputeHudTtfPointSize( void ) {
	int basePt;
	int targetPx;
	float pixelScale;

	basePt = Cvar_VariableIntegerValue( "r_fontSize" );
	if ( basePt <= 0 ) {
		basePt = 16;
	}

	pixelScale = SCR_UiPixelScale();
	targetPx = (int)( (float)BIGCHAR_HEIGHT * pixelScale + 0.5f );
	if ( targetPx < basePt ) {
		targetPx = basePt;
	}

	return SCR_TtfPointSizeForPixelHeight( targetPx );
}

static int SCR_ComputeConsoleTtfPointSize( void ) {
	int targetPx;
	float scale;

	scale = 1.0f;
	if ( con_scale ) {
		scale = con_scale->value;
		if ( scale < 0.5f ) {
			scale = 0.5f;
		}
		if ( scale > 8.0f ) {
			scale = 8.0f;
		}
	}

	targetPx = (int)( (float)SMALLCHAR_HEIGHT * scale * cls.con_factor + 0.5f );
	if ( targetPx < 8 ) {
		targetPx = 8;
	}

	return SCR_TtfPointSizeForPixelHeight( targetPx );
}

static int SCR_ComputeConsoleTtfCellWidth( const fontInfo_t *font, int refLinePx );

/*
================
CL_RefreshBuiltInTrueTypeFonts

Re-rasterize when console cell size or resolution changes (avoids blurry upscale).
================
*/
void CL_RefreshBuiltInTrueTypeFonts( void ) {
	int wantHud;
	int wantCon;

	if ( !Cvar_VariableIntegerValue( "cl_builtInTtf" ) || !re.RegisterFont ) {
		return;
	}

	wantHud = SCR_ComputeHudTtfPointSize();
	wantCon = SCR_ComputeConsoleTtfPointSize();

	if ( cls.builtInTtfActive && wantHud == cls.builtInHudPointSize && wantCon == cls.builtInConsolePointSize ) {
		return;
	}

	if ( re.ClearTrueTypeFontCache ) {
		re.ClearTrueTypeFontCache();
	}

	CL_RegisterBuiltInTrueTypeFonts();
}

/*
================
CL_RegisterBuiltInTrueTypeFonts

Loads r_font (and optional r_consoleFont) via the renderer FreeType path so
HUD / console text uses the same glyph atlas as the UI instead of only the
legacy 16x16 bitmap charset (when r_font is set and BUILD_FREETYPE is on).
================
*/
void CL_RegisterBuiltInTrueTypeFonts( void ) {
	const char *hudPath;
	const char *conPath;
	int hudPt;
	int conPt;
	glyphInfo_t *g;

	if ( !Cvar_VariableIntegerValue( "cl_builtInTtf" ) ) {
		return;
	}

	cls.builtInTtfActive = qfalse;
	Com_Memset( &cls.builtInHudFont, 0, sizeof( cls.builtInHudFont ) );
	Com_Memset( &cls.builtInConsoleFont, 0, sizeof( cls.builtInConsoleFont ) );
	cls.builtInHudRefLinePx = 0;
	cls.builtInConsoleRefLinePx = 0;
	cls.builtInHudPointSize = 0;
	cls.builtInConsolePointSize = 0;
	cls.builtInConsoleCellW = 0;

	if ( !re.RegisterFont ) {
		return;
	}

	hudPath = Cvar_VariableString( "r_font" );
	if ( !hudPath || !hudPath[0] ) {
		return;
	}

	hudPt = SCR_ComputeHudTtfPointSize();
	conPt = SCR_ComputeConsoleTtfPointSize();

	re.RegisterFont( hudPath, hudPt, &cls.builtInHudFont );
	g = &cls.builtInHudFont.glyphs[ (int)'M' & 255 ];
	if ( !g->glyph || g->imageHeight <= 0 ) {
		g = &cls.builtInHudFont.glyphs[ (int)'0' & 255 ];
	}
	if ( !g->glyph || g->imageHeight <= 0 ) {
		Com_Memset( &cls.builtInHudFont, 0, sizeof( cls.builtInHudFont ) );
		Com_Printf( S_COLOR_YELLOW "Client: could not load r_font \"%s\" (TrueType); using bitmap charset for HUD\n", hudPath );
		return;
	}

	cls.builtInHudRefLinePx = g->top - g->bottom;
	if ( cls.builtInHudRefLinePx <= 0 ) {
		cls.builtInHudRefLinePx = g->imageHeight;
	}

	conPath = Cvar_VariableString( "r_consoleFont" );
	if ( conPath && conPath[0] && Q_stricmp( conPath, hudPath ) != 0 ) {
		re.RegisterFont( conPath, conPt, &cls.builtInConsoleFont );
		g = &cls.builtInConsoleFont.glyphs[ (int)'M' & 255 ];
		if ( !g->glyph || g->imageHeight <= 0 ) {
			g = &cls.builtInConsoleFont.glyphs[ (int)'0' & 255 ];
		}
		if ( !g->glyph || g->imageHeight <= 0 ) {
			Com_Printf( S_COLOR_YELLOW "Client: r_consoleFont \"%s\" failed; using r_font for console\n", conPath );
			conPath = hudPath;
			re.RegisterFont( conPath, conPt, &cls.builtInConsoleFont );
		} else {
			Com_Printf( "Client: console TrueType font \"%s\" @ %dpt (cell %dpx)\n", conPath, conPt, smallchar_height );
		}
	} else {
		conPath = hudPath;
		re.RegisterFont( conPath, conPt, &cls.builtInConsoleFont );
	}

	g = &cls.builtInConsoleFont.glyphs[ (int)'M' & 255 ];
	if ( !g->glyph || g->imageHeight <= 0 ) {
		g = &cls.builtInConsoleFont.glyphs[ (int)'0' & 255 ];
	}
	if ( !g->glyph || g->imageHeight <= 0 ) {
		Com_Memset( &cls.builtInConsoleFont, 0, sizeof( cls.builtInConsoleFont ) );
		Com_Printf( S_COLOR_YELLOW "Client: console TrueType @ %dpt failed; console falls back to bitmap/SDF\n", conPt );
	} else {
		cls.builtInConsoleRefLinePx = g->top - g->bottom;
		if ( cls.builtInConsoleRefLinePx <= 0 ) {
			cls.builtInConsoleRefLinePx = g->imageHeight;
		}
	}

	cls.builtInTtfActive = qtrue;
	cls.builtInHudPointSize = hudPt;
	cls.builtInConsolePointSize = conPt;
	cls.builtInConsoleCellW = SCR_ComputeConsoleTtfCellWidth( &cls.builtInConsoleFont, cls.builtInConsoleRefLinePx );
	if ( cls.builtInConsoleCellW > 0 && cl_builtInTtfConsole && cl_builtInTtfConsole->integer ) {
		static int prevConsoleCellW;
		if ( !Cvar_VariableString( "r_consoleFont" )[0] || Q_stricmp( Cvar_VariableString( "r_consoleFont" ), hudPath ) == 0 ) {
			Com_Printf( "Client: console TrueType \"%s\" @ %dpt (cell %dx%dpx)\n",
				conPath, conPt, cls.builtInConsoleCellW, smallchar_height );
		}
		if ( cls.builtInConsoleCellW != prevConsoleCellW && con_scale ) {
			prevConsoleCellW = cls.builtInConsoleCellW;
			con_scale->modified = qtrue;
			Con_CheckResize();
		}
	} else if ( !cl_builtInTtfConsole || !cl_builtInTtfConsole->integer ) {
		Com_Printf( "Client: console text using bitmap charset (cl_builtInTtfConsole 0)\n" );
	}
	Com_Printf( "Client: built-in HUD TrueType font \"%s\" @ %dpt (target ~%dpx, dpi %d)\n",
		hudPath, hudPt, (int)( (float)BIGCHAR_HEIGHT * SCR_UiPixelScale() + 0.5f ), Cvar_VariableIntegerValue( "r_fontDpi" ) );
	if ( r_fontKerning ) {
		Com_Printf( "Client: r_fontKerning %i (proportional xSkip + FreeType kern; Rougier HAL-05430837)\n",
			r_fontKerning->integer );
	}
	if ( r_fontConsoleAlign ) {
		Com_Printf( "Client: r_fontConsoleAlign %i (TrueType pixel console/HUD: 0=top of cell, 1=baseline in cell)\n",
			r_fontConsoleAlign->integer );
	}
}


/*
================
SCR_ComputeConsoleTtfCellWidth

Monospace advance for TrueType console rows: at least the legacy 8px bitmap
width, widened when 12pt+ glyphs need more than 8px at the current cell height.
================
*/
static int SCR_ComputeConsoleTtfCellWidth( const fontInfo_t *font, int refLinePx ) {
	int maxSkip = 0;
	int w;
	int i;

	(void)refLinePx;

	if ( !font ) {
		return smallchar_width > 0 ? smallchar_width : SMALLCHAR_WIDTH;
	}

	for ( i = GLYPH_CHARSTART; i <= GLYPH_CHAREND; i++ ) {
		const glyphInfo_t *g = &font->glyphs[i];
		if ( g->xSkip > maxSkip ) {
			maxSkip = g->xSkip;
		}
	}

	w = smallchar_width > 0 ? smallchar_width : SMALLCHAR_WIDTH;
	if ( maxSkip > 0 && maxSkip > w ) {
		w = maxSkip;
	}
	if ( w < SMALLCHAR_WIDTH ) {
		w = SMALLCHAR_WIDTH;
	}
	return w;
}

/*
================
SCR_ConsoleCharWidth

Effective horizontal console cell size (bitmap 8px or wider TrueType metrics).
================
*/
int SCR_ConsoleCharWidth( void ) {
	if ( SCR_ConsoleTtfEnabled() && cls.builtInConsoleCellW > 0 ) {
		return cls.builtInConsoleCellW;
	}
	return smallchar_width > 0 ? smallchar_width : SMALLCHAR_WIDTH;
}

/*
================
SCR_TtfConsoleGlyphRect

Fit a glyph into a console cell without squashing: preserve atlas aspect ratio.
leftAlign 1 = ink at cell origin (proportional / kerning-friendly);
0 = center in fixed monospace cell (legacy grid).
================
*/
static void SCR_TtfConsoleGlyphRect( float xx, float y, const glyphInfo_t *g, float cellW, float cellH,
		int refLinePx, qboolean baselineAlign, qboolean leftAlign, float *outAx, float *outAy, float *outAw, float *outAh ) {
	float drawW;
	float drawH;

	(void)refLinePx;

	drawW = (float)g->imageWidth;
	drawH = (float)g->imageHeight;
	if ( drawW < 1.0f ) {
		drawW = 1.0f;
	}
	if ( drawH < 1.0f ) {
		drawH = 1.0f;
	}

	*outAw = drawW;
	*outAh = drawH;
	if ( leftAlign ) {
		*outAx = xx;
	} else {
		*outAx = xx + ( cellW - drawW ) * 0.5f;
	}

	if ( baselineAlign && g->height > 0 ) {
		const float desc = Com_Clamp( 2.0f, 12.0f, cellH * 0.22f );
		const float baselineInCell = cellH - desc;
		const float inkTop = (float)( g->imageHeight - g->top );
		*outAy = y + baselineInCell - ( (float)g->height - inkTop );
	} else {
		*outAy = y + ( cellH - drawH );
	}
}


static float SCR_TtfScaledTop( const glyphInfo_t *g, float cellH ) {
	if ( !g || g->imageHeight <= 0 || cellH <= 0.0f ) {
		return 0.0f;
	}
	return (float)g->top * cellH / (float)g->imageHeight;
}

/*
SCR_TtfCellAy
Vertical origin for a TrueType glyph stretched into a square cell (virtual 640x480 or screen pixels).
When baseline alignment is on, a row baseline sits above a descender gutter so cap height and descenders fit more naturally than pinning the atlas rect to y.
*/
static float SCR_TtfCellAy( float y, const glyphInfo_t *g, float cellH, int refLinePx, qboolean baselineAlign ) {
	if ( !baselineAlign || refLinePx <= 0 || cellH <= 0.0f || !g || g->imageHeight <= 0 ) {
		return y;
	}
	const float desc = Com_Clamp( 2.0f, 12.0f, cellH * 0.22f );
	const float baselineInCell = cellH - desc;
	return y + baselineInCell - SCR_TtfScaledTop( g, cellH );
}

static float SCR_TtfShadowOffset( void ) {
	int sh = 2;
	if ( r_fontShadow ) {
		sh = r_fontShadow->integer;
	}
	if ( sh < 0 ) {
		sh = 0;
	}
	if ( sh > 8 ) {
		sh = 8;
	}
	return (float)sh;
}

static float SCR_TtfSubpixelBias( void ) {
	if ( r_fontSubpixelPos && r_fontSubpixelPos->integer ) {
		return 0.0f;
	}
	return ( r_fontSubpixel && r_fontSubpixel->integer ) ? 0.375f : 0.0f;
}

static int SCR_TextRenderMode( void ) {
	int mode = 0;

	if ( r_textMode ) {
		mode = r_textMode->integer;
	}
	if ( mode < 0 ) {
		mode = 0;
	}
	if ( mode > 4 ) {
		mode = 4;
	}
	return mode;
}

static float SCR_TtfGlyphAdvance( const fontInfo_t *font, int refLinePx, float cellH,
		const glyphInfo_t *g, int prevCh, int ch ) {
	float adv;

	if ( !g || refLinePx <= 0 || cellH <= 0.0f ) {
		return cellH;
	}
	adv = (float)g->xSkip * cellH / (float)refLinePx;
	if ( r_fontKerning && r_fontKerning->integer && prevCh >= 0 && re.GetFontKerning ) {
		adv += re.GetFontKerning( font, prevCh, ch ) * cellH / (float)refLinePx;
	}
	if ( adv < 1.0f ) {
		adv = 1.0f;
	}
	return adv;
}


static qboolean SCR_DrawBuiltInTtfStringExtVirtual( int x, int y, float size, const char *string,
		const fontInfo_t *font, const float *setColor, qboolean forceColor, qboolean noColorEscape ) {
	vec4_t color;
	const char *s;
	float xx;
	const float cell = size;
	const float shOff = SCR_TtfShadowOffset();
	const float sp = SCR_TtfSubpixelBias();
	const qboolean baselineAlign = ( r_fontConsoleAlign && r_fontConsoleAlign->integer ) ? qtrue : qfalse;
	const int refPx = cls.builtInHudRefLinePx;
	int prevCh;

	if ( !string || !string[0] || !setColor || !font ) {
		return qfalse;
	}

	/* Match SCR_DrawChar: cell is `size` x `size` in 640x480 virtual units; do not scale quad by
	 * atlas pixel dimensions * useScale or huge clampedSize values blow up SCR_AdjustFrom640. */
	if ( shOff > 0.0f ) {
		color[0] = color[1] = color[2] = 0.0f;
		color[3] = setColor[3];
		re.SetColor( color );
		s = string;
		xx = (float)x;
		prevCh = -1;
		while ( *s ) {
			int ch;
			const glyphInfo_t *g;
			float ax, ay, aw, ah;

			if ( !noColorEscape && Q_IsColorString( s ) ) {
				s += 2;
				continue;
			}
			if ( (unsigned char)*s >= 0x80 ) {
				uint32_t cp = Q_UTF8_Decode( &s );
				if ( CL_Emoji_IsEnabled() && Q_UTF8_IsEmoji( cp ) ) {
					xx += cell;
					continue;
				}
				ch = ( cp < 256 ) ? (int)( cp & 0xFF ) : '?';
			} else {
				ch = (unsigned char)*s;
				s++;
			}
			ch &= 255;
			g = &font->glyphs[ch];
			if ( !g->glyph || g->imageHeight <= 0 || g->imageWidth <= 0 ) {
				xx += cell;
				continue;
			}
			aw = cell;
			ah = cell;
			ax = xx + shOff;
			ay = SCR_TtfCellAy( (float)y, g, cell, refPx, baselineAlign ) + shOff;
			SCR_AdjustFrom640( &ax, &ay, &aw, &ah );
			ax += sp;
			ay += sp;
			SCR_DrawStretchPicTtf( ax, ay, aw, ah, g->s, g->t, g->s2, g->t2, g->glyph );
			xx += SCR_TtfGlyphAdvance( font, refPx, cell, g, prevCh, ch );
			prevCh = ch;
		}
	}

	s = string;
	xx = (float)x;
	prevCh = -1;
	Com_Memcpy( color, setColor, sizeof( color ) );
	re.SetColor( setColor );
	while ( *s ) {
		int ch;
		const glyphInfo_t *g;
		float ax, ay, aw, ah;

		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				Com_Memcpy( color, g_color_table[ ColorIndexFromChar( *( s + 1 ) ) ], sizeof( color ) );
				color[3] = setColor[3];
				re.SetColor( color );
			}
			if ( !noColorEscape ) {
				s += 2;
				continue;
			}
		}
		if ( (unsigned char)*s >= 0x80 ) {
			uint32_t cp = Q_UTF8_Decode( &s );
			if ( CL_Emoji_IsEnabled() && Q_UTF8_IsEmoji( cp ) && CL_Emoji_DrawChar( (int)xx, (int)y, (int)cell, (int)cell, cp ) ) {
				re.SetColor( forceColor ? setColor : color );
				xx += cell;
				continue;
			}
			ch = ( cp < 256 ) ? (int)( cp & 0xFF ) : '?';
		} else {
			ch = (unsigned char)*s;
			s++;
		}
		ch &= 255;
		g = &font->glyphs[ch];
		if ( !g->glyph || g->imageHeight <= 0 || g->imageWidth <= 0 ) {
			xx += cell;
			continue;
		}
		aw = cell;
		ah = cell;
		ax = xx;
		ay = SCR_TtfCellAy( (float)y, g, cell, refPx, baselineAlign );
		SCR_AdjustFrom640( &ax, &ay, &aw, &ah );
		ax += sp;
		ay += sp;
		SCR_DrawStretchPicTtf( ax, ay, aw, ah, g->s, g->t, g->s2, g->t2, g->glyph );
		xx += SCR_TtfGlyphAdvance( font, refPx, cell, g, prevCh, ch );
		prevCh = ch;
	}

	re.SetColor( NULL );
	return qtrue;
}


static qboolean SCR_DrawBuiltInTtfStringExtPixels( int x, int y, const char *string, const fontInfo_t *font,
		int refLinePx, const float *setColor, qboolean forceColor, qboolean noColorEscape ) {
	vec4_t color;
	const char *s;
	float xx;
	const float cellH = (float)smallchar_height;
	const float cellW = (float)SCR_ConsoleCharWidth();
	const qboolean baselineAlign = ( r_fontConsoleAlign && r_fontConsoleAlign->integer ) ? qtrue : qfalse;
	const qboolean prop = ( r_fontConsoleProportional && r_fontConsoleProportional->integer ) ? qtrue : qfalse;
	int prevCh = -1;

	if ( !string || !string[0] || !setColor || !font ) {
		return qfalse;
	}

	/* Console TrueType: 1:1 atlas pixels. Proportional mode uses xSkip + FreeType kerning. */
	s = string;
	xx = (float)x;
	Com_Memcpy( color, setColor, sizeof( color ) );
	re.SetColor( setColor );
	while ( *s ) {
		int ch;
		const glyphInfo_t *g;
		float ax, ay, aw, ah;
		float adv;

		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				Com_Memcpy( color, g_color_table[ ColorIndexFromChar( *( s + 1 ) ) ], sizeof( color ) );
				color[3] = setColor[3];
				re.SetColor( color );
			}
			if ( !noColorEscape ) {
				s += 2;
				continue;
			}
		}
		if ( (unsigned char)*s >= 0x80 ) {
			uint32_t cp = Q_UTF8_Decode( &s );
			if ( CL_Emoji_IsEnabled() && Q_UTF8_IsEmoji( cp ) && CL_Emoji_DrawChar( (int)xx, (int)y, (int)cellW, smallchar_height, cp ) ) {
				re.SetColor( forceColor ? setColor : color );
				xx += cellW;
				prevCh = -1;
				continue;
			}
			ch = ( cp < 256 ) ? (int)( cp & 0xFF ) : '?';
		} else {
			ch = (unsigned char)*s;
			s++;
		}
		ch &= 255;
		g = &font->glyphs[ch];
		if ( !g->glyph || g->imageHeight <= 0 || g->imageWidth <= 0 ) {
			xx += prop ? ( cellH * 0.5f ) : cellW;
			prevCh = -1;
			continue;
		}
		if ( prop ) {
			adv = SCR_TtfGlyphAdvance( font, refLinePx, cellH, g, prevCh, ch );
			SCR_TtfConsoleGlyphRect( xx, (float)y, g, adv, cellH, refLinePx, baselineAlign, qtrue,
				&ax, &ay, &aw, &ah );
			re.DrawStretchPic( ax, ay, aw, ah, g->s, g->t, g->s2, g->t2, g->glyph );
			xx += adv;
		} else {
			SCR_TtfConsoleGlyphRect( xx, (float)y, g, cellW, cellH, refLinePx, baselineAlign, qfalse,
				&ax, &ay, &aw, &ah );
			re.DrawStretchPic( ax, ay, aw, ah, g->s, g->t, g->s2, g->t2, g->glyph );
			xx += cellW;
		}
		prevCh = ch;
	}

	re.SetColor( NULL );
	return qtrue;
}


/*
** SCR_DrawChar
** chars are drawn at 640*480 virtual screen size
*/
static void SCR_DrawChar( int x, int y, float size, int ch ) {
	int row, col;
	float frow, fcol;
	float	ax, ay, aw, ah;

	ch &= 255;

	if ( ch == ' ' ) {
		return;
	}

	if ( y < -size ) {
		return;
	}

	ax = x;
	ay = y;
	aw = size;
	ah = size;
	SCR_AdjustFrom640( &ax, &ay, &aw, &ah );

	row = ch>>4;
	col = ch&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	re.DrawStretchPic( ax, ay, aw, ah,
					   fcol, frow, 
					   fcol + size, frow + size, 
					   cls.charSetShader );
}


/*
** SCR_DrawSmallChar
** small chars are drawn at native screen resolution
*/
void SCR_DrawSmallChar( int x, int y, int ch ) {
	int row, col;
	float frow, fcol;
	float size;

	ch &= 255;

	if ( ch == ' ' ) {
		return;
	}

	if ( y < -smallchar_height ) {
		return;
	}

	row = ch>>4;
	col = ch&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	re.DrawStretchPic( x, y, smallchar_width, smallchar_height,
					   fcol, frow, 
					   fcol + size, frow + size, 
					   cls.charSetShader );
}


/*
** SCR_DrawSmallString
** small string are drawn at native screen resolution.
** Uses SDF when enabled for resolution-independent sharp text.
*/
void SCR_DrawSmallString( int x, int y, const char *s, int len ) {
	int row, col, ch;
	float frow, fcol;
	float size;
	const char *end;
	vec4_t white = { 1.0f, 1.0f, 1.0f, 1.0f };

	if ( y < -smallchar_height ) {
		return;
	}

	/* GPU vector outlines (Lengyel 2017) when r_vectorFont 1. */
	if ( len > 0 && len < 1024 ) {
		char buf[1024];
		int n = len;
		if ( n >= (int)sizeof( buf ) ) n = (int)sizeof( buf ) - 1;
		Com_Memcpy( buf, s, (size_t)n );
		buf[n] = '\0';
		if ( VectorFont_DrawStringExt( x, y, (float)smallchar_height, buf, white, qtrue, qtrue, qfalse ) ) {
			return;
		}
	}

	/* Prefer SDF, then optional FreeType console, then bitmap charset. */
	if ( SDF_IsEnabled() && len > 0 && len < 1024 ) {
		char buf[1024];
		int n = len;
		if ( n >= (int)sizeof( buf ) ) n = (int)sizeof( buf ) - 1;
		Com_Memcpy( buf, s, (size_t)n );
		buf[n] = '\0';
		if ( SDF_DrawStringExt( x, y, (float)smallchar_height, buf, white, qtrue, qtrue, SDF_COORDS_SCREEN ) ) {
			return;
		}
	}

	if ( SCR_ConsoleTtfEnabled() && len > 0 && len < 1024 ) {
		char buf[1024];
		int n = len;
		if ( n >= (int)sizeof( buf ) ) n = (int)sizeof( buf ) - 1;
		Com_Memcpy( buf, s, (size_t)n );
		buf[n] = '\0';
		if ( SCR_DrawBuiltInTtfStringExtPixels( x, y, buf, &cls.builtInConsoleFont, cls.builtInConsoleRefLinePx, white, qtrue, qtrue ) ) {
			return;
		}
	}

	size = 0.0625;
	end = s + len;

	while ( s < end && *s ) {
		if ( (unsigned char)*s >= 0x80 ) {
			uint32_t cp = Q_UTF8_Decode( &s );
			ch = ( cp < 256 ) ? (int)( cp & 0xFF ) : '?';
		} else {
			ch = (unsigned char)*s;
			s++;
		}
		row = ch >> 4;
		col = ch & 15;

		frow = row * 0.0625;
		fcol = col * 0.0625;

		re.DrawStretchPic( x, y, smallchar_width, smallchar_height,
						   fcol, frow, fcol + size, frow + size, 
						   cls.charSetShader );

		x += smallchar_width;
	}
}


/*
==================
SCR_DrawBigString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void SCR_DrawStringExt( int x, int y, float size, const char *string, const float *setColor, qboolean forceColor,
		qboolean noColorEscape ) {
	vec4_t		color;
	const char	*s;
	int			xx;
	const float	clampedSize = Com_Clamp( 1.0f, 256.0f, size );
	const int textMode = SCR_TextRenderMode();

	if ( textMode == 0 || textMode == 3 ) {
		if ( VectorFont_DrawStringExt( x, y, clampedSize, string, setColor, forceColor, noColorEscape, qtrue ) ) {
			return;
		}
	}

	if ( textMode == 4 ) {
		goto bitmap_fallback;
	}

	if ( ( textMode == 0 || textMode == 1 ) && cls.builtInTtfActive &&
			SCR_DrawBuiltInTtfStringExtVirtual( x, y, clampedSize, string, &cls.builtInHudFont, setColor, forceColor, noColorEscape ) ) {
		return;
	}

	if ( ( textMode == 0 || textMode == 2 ) &&
			SDF_DrawStringExt( x, y, clampedSize, string, setColor, forceColor, noColorEscape, SDF_COORDS_VIRTUAL_640 ) ) {
		return;
	}

	if ( textMode == 2 || textMode == 3 ) {
		return;
	}

bitmap_fallback:
	// draw the drop shadow
	color[0] = color[1] = color[2] = 0.0;
	color[3] = setColor[3];
	re.SetColor( color );
	s = string;
	xx = x;
	while ( *s ) {
		int ch;
		if ( !noColorEscape && Q_IsColorString( s ) ) {
			s += 2;
			continue;
		}
		if ( (unsigned char)*s >= 0x80 ) {
			uint32_t cp = Q_UTF8_Decode( &s );
			if ( CL_Emoji_IsEnabled() && Q_UTF8_IsEmoji( cp ) ) {
				xx += (int)clampedSize;
				continue;
			}
			ch = ( cp < 256 ) ? (int)( cp & 0xFF ) : '?';
		} else {
			ch = (unsigned char)*s;
			s++;
		}
		SCR_DrawChar( xx+2, y+2, clampedSize, ch );
		xx += (int)clampedSize;
	}


	// draw the colored text
	s = string;
	xx = x;
	Com_Memcpy( color, setColor, sizeof( color ) );
	re.SetColor( setColor );
	while ( *s ) {
		int ch;
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				Com_Memcpy( color, g_color_table[ ColorIndexFromChar( *(s+1) ) ], sizeof( color ) );
				color[3] = setColor[3];
				re.SetColor( color );
			}
			if ( !noColorEscape ) {
				s += 2;
				continue;
			}
		}
		if ( (unsigned char)*s >= 0x80 ) {
			uint32_t cp = Q_UTF8_Decode( &s );
			if ( CL_Emoji_IsEnabled() && Q_UTF8_IsEmoji( cp ) && CL_Emoji_DrawChar( xx, y, clampedSize, clampedSize, cp ) ) {
				re.SetColor( forceColor ? setColor : color );
				xx += (int)clampedSize;
				continue;
			}
			ch = ( cp < 256 ) ? (int)( cp & 0xFF ) : '?';
		} else {
			ch = (unsigned char)*s;
			s++;
		}
		SCR_DrawChar( xx, y, clampedSize, ch );
		xx += (int)clampedSize;
	}
	re.SetColor( NULL );
}


/*
==================
SCR_DrawBigString
==================
*/
void SCR_DrawBigString( int x, int y, const char *s, float alpha, qboolean noColorEscape ) {
	float	color[4];

	color[0] = color[1] = color[2] = 1.0;
	color[3] = alpha;
	SCR_DrawStringExt( x, y, BIGCHAR_WIDTH, s, color, qfalse, noColorEscape );
}


/*
==================
SCR_DrawSmallString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color. Uses SDF when enabled for resolution-independent sharp text.
==================
*/
void SCR_DrawSmallStringExt( int x, int y, const char *string, const float *setColor, qboolean forceColor,
		qboolean noColorEscape ) {
	vec4_t		color;
	const char	*s;
	int			xx;
	int			ch;
	const float	sdfSize = (float)smallchar_height;
	const int textMode = SCR_TextRenderMode();

	if ( textMode == 4 ) {
		goto bitmap_fallback;
	}

	if ( ( textMode == 0 || textMode == 2 ) && SDF_IsEnabled() &&
			SDF_DrawStringExt( x, y, sdfSize, string, setColor, forceColor, noColorEscape, SDF_COORDS_SCREEN ) ) {
		return;
	}

	if ( ( textMode == 0 || textMode == 1 ) && SCR_ConsoleTtfEnabled() &&
			SCR_DrawBuiltInTtfStringExtPixels( x, y, string, &cls.builtInConsoleFont, cls.builtInConsoleRefLinePx, setColor, forceColor, noColorEscape ) ) {
		return;
	}

	if ( textMode == 2 || textMode == 3 ) {
		return;
	}

bitmap_fallback:
	// draw the colored text (bitmap fallback)
	s = string;
	xx = x;
	re.SetColor( setColor );
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				Com_Memcpy( color, g_color_table[ ColorIndexFromChar( *(s+1) ) ], sizeof( color ) );
				color[3] = setColor[3];
				re.SetColor( color );
			}
			if ( !noColorEscape ) {
				s += 2;
				continue;
			}
		}
		if ( (unsigned char)*s >= 0x80 ) {
			uint32_t cp = Q_UTF8_Decode( &s );
			ch = ( cp < 256 ) ? (int)( cp & 0xFF ) : '?';
		} else {
			ch = (unsigned char)*s;
			s++;
		}
		SCR_DrawSmallChar( xx, y, ch );
		xx += smallchar_width;
	}
	re.SetColor( NULL );
}


/*
** SCR_Strlen -- skips color escape codes
*/
static int SCR_Strlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}


/*
** SCR_GetBigStringWidth
*/ 
int SCR_GetBigStringWidth( const char *str ) {
	return SCR_Strlen( str ) * BIGCHAR_WIDTH;
}


//===============================================================================

/*
=================
SCR_DrawDemoRecording
=================
*/
static void SCR_DrawDemoRecording( void ) {
	char	string[sizeof(clc.recordNameShort)+32];
	int		pos;

	if ( !clc.demorecording ) {
		return;
	}
	if ( clc.spDemoRecording ) {
		return;
	}

	pos = FS_FTell( clc.recordfile );

	if (cl_drawRecording->integer == 1) {
		Com_sprintf( string, sizeof( string ), "RECORDING %s: %ik", clc.recordNameShort, pos / 1024 );
		SCR_DrawStringExt(320 - strlen(string) * 4, 20, 8, string, g_color_table[ColorIndex(COLOR_WHITE)], qtrue, qfalse);
	} else if (cl_drawRecording->integer == 2) {
		Com_sprintf( string, sizeof( string ), "RECORDING: %ik", pos / 1024 );
		SCR_DrawStringExt(320 - strlen(string) * 4, 20, 8, string, g_color_table[ColorIndex(COLOR_WHITE)], qtrue, qfalse);
	}
}


#ifdef USE_OPUS
/*
=================
SCR_DrawVoipMeter
=================
*/
static void SCR_DrawVoipMeter( void ) {
	char	buffer[16];
	char	string[256];
	int limit, i;

	if ( !CL_VoIP_GetShowMeter() )
		return;  // player doesn't want to show meter at all.
	else if ( !CL_VoIP_IsSending() )
		return;  // not recording at the moment.
	else if ( cls.state != CA_ACTIVE )
		return;  // not connected to a server.
	else if ( clc.demoplaying )
		return;  // playing back a demo.
	else if ( !CL_VoIP_IsEnabled() )
		return;  // client has VoIP support disabled.

	limit = (int) ( CL_VoIP_GetPower() * 10.0f );
	if (limit > 10)
		limit = 10;

	for (i = 0; i < limit; i++)
		buffer[i] = '*';
	while (i < 10)
		buffer[i++] = ' ';
	buffer[i] = '\0';

	Com_sprintf( string, sizeof( string ), "VoIP: [%s]", buffer );
	SCR_DrawStringExt( 320 - strlen( string ) * 4, 10, 8, string, g_color_table[ ColorIndex( COLOR_WHITE ) ], qtrue, qfalse );
}
#endif


/*
===============================================================================

DEBUG GRAPH

===============================================================================
*/

static	int			current;
static	float		values[1024];

/*
==============
SCR_DebugGraph
==============
*/
void SCR_DebugGraph( float value )
{
	values[current] = value;
	current = (current + 1) % ARRAY_LEN(values);
}


/*
==============
SCR_DrawDebugGraph
==============
*/
static void SCR_DrawDebugGraph( void )
{
	int		a, x, y, w, i, h;
	float	v;

	//
	// draw the graph
	//
	w = cls.glconfig.vidWidth;
	x = 0;
	y = cls.glconfig.vidHeight;
	re.SetColor( g_color_table[ ColorIndex( COLOR_BLACK ) ] );
	re.DrawStretchPic(x, y - cl_graphheight->integer, 
		w, cl_graphheight->integer, 0, 0, 0, 0, cls.whiteShader );
	re.SetColor( NULL );

	for (a=0 ; a<w ; a++)
	{
		i = (ARRAY_LEN(values)+current-1-(a % ARRAY_LEN(values))) % ARRAY_LEN(values);
		v = values[i];
		v = v * cl_graphscale->integer + cl_graphshift->integer;
		
		if (v < 0)
			v += cl_graphheight->integer * (1+(int)(-v / cl_graphheight->integer));
		h = (int)v % cl_graphheight->integer;
		re.DrawStretchPic( x+w-1-a, y - h, 1, h, 0, 0, 0, 0, cls.whiteShader );
	}
}

//=============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init( void ) {
	cl_timegraph = Cvar_Get ("timegraph", "0", CVAR_CHEAT);
	cl_debuggraph = Cvar_Get ("debuggraph", "0", CVAR_CHEAT);
	cl_graphheight = Cvar_Get ("graphheight", "32", CVAR_CHEAT);
	cl_graphscale = Cvar_Get ("graphscale", "1", CVAR_CHEAT);
	cl_graphshift = Cvar_Get ("graphshift", "0", CVAR_CHEAT);
	ui_scale = Cvar_Get( "ui_scale", "1.0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( ui_scale, "0.5", "4.0", CV_FLOAT );
	Cvar_SetDescription( ui_scale, "UI scale factor for menus and HUD. Increase for 4K/ultra-wide displays." );

	{
		cvar_t *cl_builtInTtf = Cvar_Get( "cl_builtInTtf", "1", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( cl_builtInTtf,
			"When 1 (default), load r_font (FreeType) for engine HUD and console text instead of only the bitmap charset; "
			"drawn before pre-baked SDF when both are available. Set 0 to prefer SDF (r_sdfEnable) or legacy bigchars if r_font fails. "
			"Requires BUILD_FREETYPE and valid font files (e.g. base/fonts/Inter-Regular.ttf)." );

		cl_builtInTtfConsole = Cvar_Get( "cl_builtInTtfConsole", "1", CVAR_ARCHIVE_ND );
		Cvar_CheckRange( cl_builtInTtfConsole, "0", "1", CV_INTEGER );
		Cvar_SetDescription( cl_builtInTtfConsole,
			"When 1 (default), draw console/notify with FreeType (r_font / r_consoleFont). Set 0 to use the legacy 8x16 bitmap charset." );
	}

	r_fontConsoleAlign = Cvar_Get( "r_fontConsoleAlign", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( r_fontConsoleAlign, "0", "1", CV_INTEGER );
	Cvar_SetDescription( r_fontConsoleAlign,
		"TrueType console / pixel HUD strings: 1 = baseline-aligned inside each fixed cell (default); 0 = legacy top-aligned to cell top." );

	r_fontConsoleProportional = Cvar_Get( "r_fontConsoleProportional", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( r_fontConsoleProportional, "0", "1", CV_INTEGER );
	Cvar_SetDescription( r_fontConsoleProportional,
		"When 1, console/notify TrueType text advances by glyph xSkip + FreeType kerning (r_fontKerning). When 0, use a fixed monospace cell grid." );

	r_fontShadow = Cvar_Get( "r_fontShadow", "2", CVAR_ARCHIVE );
	Cvar_CheckRange( r_fontShadow, "0", "8", CV_INTEGER );
	Cvar_SetDescription( r_fontShadow,
		"Drop-shadow offset in pixels (console/notify) or virtual units (640x480 HUD bigchars). 0 disables the shadow pass for TrueType." );

	r_fontSubpixel = Cvar_Get( "r_fontSubpixel", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( r_fontSubpixel, "0", "1", CV_INTEGER );
	Cvar_SetDescription( r_fontSubpixel,
		"When 1, nudge TrueType draw positions by 0.375px after projection; can sharpen linear-filtered edges on some displays. Ignored when r_fontSubpixelPos is 1." );

	r_fontSubpixelPos = Cvar_Get( "r_fontSubpixelPos", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( r_fontSubpixelPos, "0", "1", CV_INTEGER );
	Cvar_SetDescription( r_fontSubpixelPos,
		"Rougier HAL-00821839 subpixel glyph positioning (Vulkan). 1 = fractional horizontal placement via uiSubpixelText shader. Best with r_fontLcd 1; apply reloadTtf after atlas changes." );

	r_fontKerning = Cvar_Get( "r_fontKerning", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( r_fontKerning, "0", "1", CV_INTEGER );
	Cvar_SetDescription( r_fontKerning,
		"Rougier HAL-05430839: apply FreeType GPOS/kern pairs when advancing TrueType HUD/console text (with proportional xSkip)." );

	r_textMode = Cvar_Get( "r_textMode", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( r_textMode, "0", "4", CV_INTEGER );
	Cvar_SetDescription( r_textMode,
		"Engine text renderer: 0=auto (vector→FreeType→SDF→bitmap), 1=texture/FreeType, 2=SDF, 3=vector, 4=bitmap only." );

	{
		cvar_t *ui_open_tab = Cvar_Get( "ui_open_tab", "", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( ui_open_tab, "Requested tab when opening main menu (credits, audio, gameplay). Set by engine for 'open <tab>' fallback; UI should read and clear when switching." );
		/* This cvar is a one-shot request channel and should never persist between boots. */
		if ( ui_open_tab->string[0] ) {
			Cvar_Set( "ui_open_tab", "" );
		}
	}

	scr_initialized = qtrue;
}


/*
==================
SCR_Done
==================
*/
void SCR_Done( void ) {
	scr_initialized = qfalse;
}


//=======================================================

/*
==================
SCR_DrawScreenField

This will be called twice if rendering in stereo mode
==================
*/
static void SCR_DrawScreenField( stereoFrame_t stereoFrame ) {
	qboolean uiFullscreen;

	re.BeginFrame( stereoFrame );

	uiFullscreen = (uivm && VM_Call( uivm, 0, UI_IS_FULLSCREEN ));

	// wide aspect ratio screens need to have the sides cleared
	// unless they are displaying game renderings
	if ( uiFullscreen || cls.state < CA_LOADING ) {
		if ( cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640 ) {
			// draw vertical bars on sides for legacy mods
			const int w = (cls.glconfig.vidWidth - ((cls.glconfig.vidHeight * 640) / 480)) /2;
			re.SetColor( g_color_table[ ColorIndex( COLOR_BLACK ) ] );
			re.DrawStretchPic( 0, 0, w, cls.glconfig.vidHeight, 0, 0, 0, 0, cls.whiteShader );
			re.DrawStretchPic( cls.glconfig.vidWidth - w, 0, w, cls.glconfig.vidHeight, 0, 0, 0, 0, cls.whiteShader );
			re.SetColor( NULL );
		}
	}

	// if the menu is going to cover the entire screen, we
	// don't need to render anything under it
	if ( uivm && !uiFullscreen ) {
		switch( cls.state ) {
		default:
			Com_Error( ERR_FATAL, "SCR_DrawScreenField: bad cls.state" );
			break;
		case CA_CINEMATIC:
			SCR_DrawCinematic();
			break;
		case CA_DISCONNECTED:
			// force menu up
			S_StopAllSounds();
			VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, UIMENU_MAIN );
			CL_JsNotifyMenuChanged( UIMENU_MAIN );
			break;
			case CA_CONNECTING:
			case CA_CHALLENGING:
			case CA_CONNECTED:
				// connecting clients will only show the connection dialog
				// refresh to update the time
				VM_Call( uivm, 1, UI_REFRESH, cls.realtime );
				VM_Call( uivm, 1, UI_DRAW_CONNECT_SCREEN, qfalse );
				break;
			case CA_LOADING:
			case CA_PRIMED:
				// draw the game information screen and loading progress
				if ( cgvm ) {
					CL_CGameRendering( stereoFrame );
				}
				// also draw the connection information, so it doesn't
				// flash away too briefly on local or lan games
				// refresh to update the time
				VM_Call( uivm, 1, UI_REFRESH, cls.realtime );
				VM_Call( uivm, 1, UI_DRAW_CONNECT_SCREEN, qtrue );
				break;
			case CA_ACTIVE:
				// always supply STEREO_CENTER as vieworg offset is now done by the engine.
				CL_CGameRendering( stereoFrame );
				SHUD_Render( cls.glconfig.vidWidth, cls.glconfig.vidHeight );
				SCR_DrawDemoRecording();
#ifdef USE_OPUS
				SCR_DrawVoipMeter();
#endif
				// PBR cubemap selection overlay (renderer-updated cvar string).
				if ( Cvar_VariableIntegerValue( "r_pbr_showCubemap" ) ) {
					const char *info = Cvar_VariableString( "r_pbr_cubemapInfo" );
					if ( info && info[0] ) {
					SCR_DrawSmallString( 8, 64, info, (int)strlen( info ) );
				} else {
					const char *fallback = "PBR cubemap: (no data)";
					SCR_DrawSmallString( 8, 64, fallback, (int)strlen( fallback ) );
					}
				}
				break;
		}
	}

	// menu background video draws behind the UI
	MenuVideo_Frame();
	MenuVideo_Draw();

	// the menu draws next
	if ( Key_GetCatcher( ) & KEYCATCH_UI && uivm ) {
		VM_Call( uivm, 1, UI_REFRESH, cls.realtime );
	}

	// console draws next
	Con_DrawConsole ();

	// debug graph can be drawn on top of anything
	if ( cl_debuggraph->integer || cl_timegraph->integer || cl_debugMove->integer ) {
		SCR_DrawDebugGraph ();
	}
}


/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen( void ) {
	static int recursive;
	static int framecount;
	static int next_frametime;

	if ( !scr_initialized )
		return; // not initialized yet

	if ( framecount == cls.framecount ) {
		int ms = Sys_Milliseconds();
		if ( next_frametime && ms - next_frametime < 0 ) {
			re.ThrottleBackend();
		} else {
			next_frametime = ms + 16; // limit to 60 FPS
		}
	} else {
		next_frametime = 0;
		framecount = cls.framecount;
	}

	if ( ++recursive > 2 ) {
		Com_Error( ERR_FATAL, "SCR_UpdateScreen: recursively called" );
	}
	recursive = 1;

	// If there is no VM, there are also no rendering commands issued. Stop the renderer in
	// that case.
	if ( uivm )
	{
		int in_anaglyphMode = Cvar_VariableIntegerValue("r_anaglyphMode");
		// if running in stereo, we need to draw the frame twice
		if ( cls.glconfig.stereoEnabled || in_anaglyphMode) {
			SCR_DrawScreenField( STEREO_LEFT );
			SCR_DrawScreenField( STEREO_RIGHT );
		} else {
			SCR_DrawScreenField( STEREO_CENTER );
		}

		if ( com_speeds->integer ) {
			re.EndFrame( &time_frontend, &time_backend );
		} else {
			re.EndFrame( NULL, NULL );
		}
	}

	recursive = 0;
}
