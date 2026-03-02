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
#include "cl_menuvideo.h"
#include "cl_sdf_font.h"
#include "../qcommon/q_utf8.h"

static qboolean	scr_initialized;		// ready to draw

cvar_t		*cl_timegraph;
static cvar_t		*cl_debuggraph;
static cvar_t		*cl_graphheight;
static cvar_t		*cl_graphscale;
static cvar_t		*cl_graphshift;
static cvar_t		*ui_scale;

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
	float	xscale;
	float	yscale;
	float	aspectRatio;
	float	baseAspectRatio;

	// Calculate aspect ratios
	aspectRatio = (float)cls.glconfig.vidWidth / (float)cls.glconfig.vidHeight;
	baseAspectRatio = 640.0f / 480.0f; // 4:3 aspect ratio

	// For widescreen displays, we want to scale to fit height and center horizontally
	if ( aspectRatio > baseAspectRatio ) {
		// Widescreen: scale to fit height, center horizontally
		yscale = cls.glconfig.vidHeight / 480.0f;
		xscale = yscale; // Use same scale for consistent UI proportions

		// Center horizontally
		if ( x ) {
			float scaledWidth = 640.0f * xscale;
			float offset = (cls.glconfig.vidWidth - scaledWidth) * 0.5f;
			*x = *x * xscale + offset;
		}
		if ( w ) {
			*w *= xscale;
		}
	} else {
		// Tall screen or 4:3: scale to fit width
		xscale = cls.glconfig.vidWidth / 640.0f;
		yscale = xscale; // Maintain aspect ratio

		if ( x ) {
			*x *= xscale;
		}
		if ( w ) {
			*w *= xscale;
		}
	}

	// Apply vertical scaling
	if ( y ) {
		*y *= yscale;
	}
	if ( h ) {
		*h *= yscale;
	}

	// Apply UI scale factor for higher resolutions
	if ( ui_scale ) {
		float scale = ui_scale->value;
		if ( x ) {
			*x *= scale;
		}
		if ( y ) {
			*y *= scale;
		}
		if ( w ) {
			*w *= scale;
		}
		if ( h ) {
			*h *= scale;
		}
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
** small string are drawn at native screen resolution
*/
void SCR_DrawSmallString( int x, int y, const char *s, int len ) {
	int row, col, ch;
	float frow, fcol;
	float size;
	const char *end;

	if ( y < -smallchar_height ) {
		return;
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

	if ( SDF_DrawStringExt( x, y, clampedSize, string, setColor, forceColor, noColorEscape ) ) {
		return;
	}

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
to a fixed color.
==================
*/
void SCR_DrawSmallStringExt( int x, int y, const char *string, const float *setColor, qboolean forceColor,
		qboolean noColorEscape ) {
	vec4_t		color;
	const char	*s;
	int			xx;
	int			ch;

	// draw the colored text
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


#ifdef USE_VOIP
/*
=================
SCR_DrawVoipMeter
=================
*/
static void SCR_DrawVoipMeter( void ) {
	char	buffer[16];
	char	string[256];
	int limit, i;

	if (!cl_voipShowMeter->integer)
		return;  // player doesn't want to show meter at all.
	else if (!cl_voipSend->integer)
		return;  // not recording at the moment.
	else if (clc.state != CA_ACTIVE)
		return;  // not connected to a server.
	else if (!clc.voipEnabled)
		return;  // server doesn't support VoIP.
	else if (clc.demoplaying)
		return;  // playing back a demo.
	else if (!cl_voip->integer)
		return;  // client has VoIP support disabled.

	limit = (int) (clc.voipPower * 10.0f);
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
	ui_scale = Cvar_Get ("ui_scale", "1.0", CVAR_ARCHIVE_ND);
	Cvar_CheckRange( ui_scale, "0.5", "4.0", CV_FLOAT );
	Cvar_SetDescription( ui_scale, "UI scale factor for high-resolution displays" );

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
			SCR_DrawDemoRecording();
#ifdef USE_VOIP
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
