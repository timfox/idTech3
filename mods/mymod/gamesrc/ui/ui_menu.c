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
//
/*
=======================================================================

MAIN MENU

=======================================================================
*/


#include "ui_local.h"

#define MAINMENU_FONT_CONFIG          "fonts/fonts.cfg"
#define MAINMENU_MAX_FALLBACKS        3
#define MAINMENU_DEFAULT_TEXT_FONT    "fonts/FX300.ttf"
#define MAINMENU_DEFAULT_SMALL_FONT   "fonts/FX300.ttf"
#define MAINMENU_DEFAULT_BIG_FONT     "fonts/FX300.ttf"
#define MAINMENU_DEFAULT_TEXT_SIZE    18
#define MAINMENU_DEFAULT_SMALL_SIZE   14
#define MAINMENU_DEFAULT_BIG_SIZE     26
#define MAINMENU_FONT_BUFFER_SIZE     8192

typedef struct {
	fontInfo_t textFont;
	fontInfo_t smallFont;
	fontInfo_t bigFont;
	fontInfo_t fallbackFonts[MAINMENU_MAX_FALLBACKS];
	int        fallbackCount;
	qboolean   loaded;
} mainmenu_font_state_t;


#define ID_SINGLEPLAYER			10
#define ID_MULTIPLAYER			11
#define ID_SETUP				12
#define ID_DEMOS				13
#define ID_CINEMATICS			14
#define ID_CHALLENGES           18
#define ID_TEAMARENA		    15
#define ID_MODS					16
#define ID_EXIT					17

#define MAIN_BANNER_MODEL				"models/mapobjects/banner/banner5.md3"
#define MAIN_MENU_VERTICAL_SPACING		38  // Increased spacing for better readability
#define MAIN_MENU_ANIMATION_SPEED		0.003f  // Animation speed multiplier
#define MAIN_MENU_PULSE_INTENSITY		0.15f   // Pulse effect intensity
#define MAIN_MENU_VERSION_COLOR_R		0.7f    // Version text color (gray)
#define MAIN_MENU_VERSION_COLOR_G		0.7f
#define MAIN_MENU_VERSION_COLOR_B		0.7f
#define MAIN_MENU_VERSION_COLOR_A		0.8f

// Error message buffer size
#define ERROR_MESSAGE_SIZE				4096


typedef struct {
	menuframework_s	menu;

	menutext_s		singleplayer;
	menutext_s		multiplayer;
	menutext_s		setup;
	menutext_s		demos;
	menutext_s		cinematics;
	menutext_s              challenges;
	menutext_s		teamArena;
	menutext_s		mods;
	menutext_s		exit;

	qhandle_t		bannerModel;
} mainmenu_t;


static mainmenu_t s_main;

typedef struct {
	menuframework_s menu;
	char errorMessage[ERROR_MESSAGE_SIZE];
} errorMessage_t;

static errorMessage_t s_errorMessage;

static mainmenu_font_state_t s_mainFonts;

#define FONT_CHAR_VALID(c) ((c) >= 0 && (c) < GLYPHS_PER_FONT)
#define FONT_LOADED(f) ((f) && ((f)->glyphScale != 0.0f || (f)->name[0] != '\0'))

/* ------------------------------------------------------------------------- */
/* Modern font helpers for the main menu                                    */
/* ------------------------------------------------------------------------- */
static fontInfo_t *MainMenu_SelectFontForStyle( int style ) {
	if ( style & UI_SMALLFONT ) {
		if ( FONT_LOADED( &s_mainFonts.smallFont ) ) {
			return &s_mainFonts.smallFont;
		}
	}

	if ( style & UI_BIGFONT ) {
		if ( FONT_LOADED( &s_mainFonts.bigFont ) ) {
			return &s_mainFonts.bigFont;
		}
	}

	if ( FONT_LOADED( &s_mainFonts.textFont ) ) {
		return &s_mainFonts.textFont;
	}

	return NULL;
}

static void MainMenu_RegisterFontSafe( const char *path, int pointSize, fontInfo_t *outFont, const char *label ) {
	Com_Printf( S_COLOR_YELLOW "DEBUG: MainMenu_RegisterFontSafe called with path='%s', label='%s'\n", path, label );
	if ( !outFont ) {
		return;
	}

	memset( outFont, 0, sizeof( *outFont ) );
	if ( !path || !path[0] ) {
		Com_Printf( "MainMenu font %s: missing path\n", label ? label : "unknown" );
		return;
	}

	Com_Printf( "MainMenu font %s: attempting to load %s (%dpt)\n", label ? label : "font", path, pointSize );
	Com_Printf( "DEBUG: trap_R_RegisterFont called with path='%s'\n", path );
	trap_R_RegisterFont( path, pointSize, outFont );
	if ( FONT_LOADED( outFont ) ) {
		Com_Printf( S_COLOR_GREEN "MainMenu font %s: loaded %s (%dpt)\n", label ? label : "font", path, pointSize );
	} else {
		Com_Printf( S_COLOR_RED "MainMenu font %s: failed to load %s (%dpt), trying fonts/FX300.ttf fallback\n", label ? label : "font", path, pointSize );
		// Try fallback to fonts/FX300.ttf
		trap_R_RegisterFont( "fonts/FX300.ttf", pointSize, outFont );
		if ( FONT_LOADED( outFont ) ) {
			Com_Printf( S_COLOR_YELLOW "MainMenu font %s: loaded fallback fonts/FX300.ttf (%dpt)\n", label ? label : "font", pointSize );
		} else {
			Com_Printf( S_COLOR_RED "MainMenu font %s: failed to load fonts/FX300.ttf (%dpt)\n", label ? label : "font", pointSize );
		}
	}
}

static void MainMenu_LoadFontsFromConfig( void ) {
	trap_Print("MainMenu_LoadFontsFromConfig: called (REFACTORED)\n");
	if ( s_mainFonts.loaded ) {
		return;
	}

	memset( &s_mainFonts, 0, sizeof( s_mainFonts ) );

	// For now, use hardcoded defaults to ensure consistency
	// In the future, we can restore the flexible parsing
	MainMenu_RegisterFontSafe( "fonts/FX300.ttf", 16, &s_mainFonts.textFont, "text" );
	MainMenu_RegisterFontSafe( "fonts/FX300.ttf", 12, &s_mainFonts.smallFont, "small" );
	MainMenu_RegisterFontSafe( "fonts/FX300.ttf", 24, &s_mainFonts.bigFont, "big" );

	s_mainFonts.loaded = FONT_LOADED( &s_mainFonts.textFont );
	Com_Printf("DEBUG: Font loading completed\n");
}

static float MainMenu_TextWidth( const char *text, float scale, fontInfo_t *baseFont ) {
	if ( !text || !*text ) {
		return 0.0f;
	}

	if ( !baseFont ) {
		baseFont = &s_mainFonts.textFont;
	}

	if ( !FONT_LOADED( baseFont ) ) {
		// Fallback to legacy proportional metrics if modern font isn't available
		return UI_ProportionalStringWidth( text ) * scale;
	}

	return trap_R_Font_Width( text, scale, baseFont );
}

static void MainMenu_TextPaint( float x, float y, float scale, vec4_t color, const char *text, int style, fontInfo_t *baseFont ) {
	if ( !text || !*text ) {
		return;
	}

	fontInfo_t *font = baseFont ? baseFont : MainMenu_SelectFontForStyle( style );
	if ( !font || !FONT_LOADED( font ) ) {
		UI_DrawProportionalString( x, y, text, style, color );
		return;
	}

	// Adjust coordinates for 640x480 virtual space
	float adjX = x, adjY = y, w = 0, h = 0;
	UI_AdjustFrom640( &adjX, &adjY, &w, &h );

	// The engine Font_DrawString expects absolute pixel coordinates and handles 
	// alignment and colors internally. We need to pass the UI scale.
	trap_R_Font_DrawString( adjX, adjY, text, color, scale * uis.xscale, font, style );
}

static void MainMenu_DrawMenuItem( void *ptr ) {
	menutext_s *t = (menutext_s *)ptr;
	if ( !t ) {
		return;
	}

	qboolean hasFocus = ( Menu_ItemAtCursor( t->generic.parent ) == t );
	vec4_t color;
	int style = t->style;

	if ( t->generic.flags & QMF_GRAYED ) {
		VectorCopy( text_color_disabled, color );
		color[3] = text_color_disabled[3];
	} else {
		VectorCopy( t->color, color );
	}

	fontInfo_t *font = MainMenu_SelectFontForStyle( style );
	float sizeScale = 1.0f;
	if ( style & UI_SMALLFONT ) {
		sizeScale = 0.85f;
	} else if ( style & UI_BIGFONT ) {
		sizeScale = 1.25f;
	}

	float measuredWidth = MainMenu_TextWidth( t->string, sizeScale, font ? font : &s_mainFonts.textFont );
	float measuredHeight;
	if ( font && FONT_LOADED( font ) ) {
		measuredHeight = trap_R_Font_Height( font, sizeScale );
	} else {
		measuredHeight = PROP_HEIGHT * sizeScale;
	}

	float x = t->generic.x;
	float y = t->generic.y;
	if ( t->generic.flags & QMF_RIGHT_JUSTIFY ) {
		x -= measuredWidth;
	} else if ( t->generic.flags & QMF_CENTER_JUSTIFY ) {
		x -= measuredWidth * 0.5f;
	}

	t->generic.left   = x - 12;
	t->generic.right  = x + measuredWidth + 12;
	t->generic.top    = y - measuredHeight * 0.35f;
	t->generic.bottom = y + measuredHeight * 1.1f;

	vec4_t highlightBg;
	if ( t->generic.flags & QMF_PULSEIFFOCUS ) {
		if ( hasFocus ) {
			style |= UI_PULSE;
			Vector4Set( highlightBg, 0.15f, 0.2f, 0.3f, 0.3f );
			UI_FillRect( t->generic.left - 10, t->generic.top - 2,
			             ( t->generic.right - t->generic.left ) + 20,
			             ( t->generic.bottom - t->generic.top ) + 4, highlightBg );
			VectorCopy( text_color_highlight, color );
			color[3] = text_color_highlight[3];
		} else {
			style |= UI_INVERSE;
		}
	} else if ( hasFocus && ( t->generic.flags & QMF_HIGHLIGHT_IF_FOCUS ) ) {
		Vector4Set( highlightBg, 0.1f, 0.15f, 0.25f, 0.25f );
		UI_FillRect( t->generic.left - 10, t->generic.top - 2,
		             ( t->generic.right - t->generic.left ) + 20,
		             ( t->generic.bottom - t->generic.top ) + 4, highlightBg );
		VectorCopy( text_color_highlight, color );
		color[3] = text_color_highlight[3];
	}

	MainMenu_TextPaint( x, y, sizeScale, color, t->string, ( style & ~( UI_CENTER | UI_RIGHT ) ) | UI_DROPSHADOW, font );
}


/*
=================
Main_MenuEvent
=================
*/
static void Main_MenuEvent (void* ptr, int event)
{
	if( event != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
	case ID_SINGLEPLAYER:
		UI_SPLevelMenu();
		break;

	case ID_MULTIPLAYER:
		if(ui_setupchecked.integer)
			UI_ArenaServersMenu();
		else
			UI_FirstConnectMenu();
		break;

	case ID_SETUP:
		UI_SetupMenu();
		break;

	case ID_DEMOS:
		UI_DemosMenu();
		break;

	case ID_CINEMATICS:
		UI_CinematicsMenu();
		break;

	case ID_CHALLENGES:
		UI_Challenges();
		break;

	/*case ID_MODS:
		UI_ModsMenu();
		break;*/

	case ID_TEAMARENA:
		trap_Cvar_Set( "fs_game", "missionpack");
		trap_Cmd_ExecuteText( EXEC_APPEND, "vid_restart;" );
		break;

	case ID_EXIT:
		UI_CreditMenu();
		break;
	}
}


/*
===============
MainMenu_Cache
===============
*/
void MainMenu_Cache( void )
{
	MainMenu_LoadFontsFromConfig();

	// Try to load the enhanced banner model
	// Try multiple banner models and formats - prioritize universally supported formats
	// Order: MD3 (supported by all renderers), then renderer-specific formats
	const char *banner_candidates[] = {
		MAIN_BANNER_MODEL,                           // banner5.md3 (universally supported)
		"models/mapobjects/banner/banner5",          // Try without extension
		"models/mapobjects/banner/cube.md3",         // Cube MD3 (universally supported)
		"models/mapobjects/banner/cube.obj",         // OBJ format (OpenGL/Vulkan only)
		"models/mapobjects/banner/cube",             // Cube without extension
		"models/mapobjects/grenade.md3",             // Grenade model
		"models/powerups/health/red.md3",            // Health pack
		"models/misc/telep.md3",                     // Teleporter
		"models/weapons2/rocketl/rocket.md3",        // Rocket
		"models/players/sarge/head.md3",             // Player head
		NULL
	};

	for (int i = 0; banner_candidates[i]; i++) {
		s_main.bannerModel = trap_R_RegisterModel( banner_candidates[i] );
		if ( s_main.bannerModel ) {
			Com_Printf( "MainMenu_Cache: Successfully loaded banner model '%s' (handle %d)\n", banner_candidates[i], s_main.bannerModel );
			break;
		} else {
			Com_Printf( "MainMenu_Cache: Failed to load banner model '%s' (this may be normal during early initialization)\n", banner_candidates[i] );
		}
	}

	if ( !s_main.bannerModel ) {
		Com_Printf( "MainMenu_Cache: All banner models failed to load, will use 2D fallback (this is normal if renderer is not fully initialized)\n" );
	}
}

/*
===============
MainMenu_DrawBannerParticles
===============
*/
static void MainMenu_DrawBannerParticles( int x, int y, int w, int h, float time );

/*
===============
MainMenu_DrawFallbackBanner
===============
*/
static void MainMenu_DrawFallbackBanner( void )
{
	vec4_t color;
	float alpha;
	int x, y, w, h;

	// Calculate banner position and size (centered, taking up reasonable screen space)
	w = 400;
	h = 200;
	x = (640 - w) / 2;
	y = 100;  // Position near top of screen

	// Animated color and alpha for visual appeal
	float time = (float)uis.realtime / 1000.0f;
	alpha = 0.3f + 0.2f * sin(time * 0.5f);  // Pulsing alpha

	// Create a gradient banner background
	color[0] = 0.2f;  // Red
	color[1] = 0.4f;  // Green
	color[2] = 0.8f;  // Blue
	color[3] = alpha; // Alpha

	// Draw main banner background with rounded corners effect
	trap_R_SetColor( color );
	UI_FillRect( x, y, w, h, color );

	// Add a glowing border
	color[0] = 0.8f;
	color[1] = 0.6f;
	color[2] = 0.2f;
	color[3] = alpha * 1.5f;

	// Top border
	UI_FillRect( x, y, w, 4, color );
	// Bottom border
	UI_FillRect( x, y + h - 4, w, 4, color );
	// Left border
	UI_FillRect( x, y, 4, h, color );
	// Right border
	UI_FillRect( x + w - 4, y, 4, h, color );

	// Draw animated particles around the banner
	MainMenu_DrawBannerParticles(x, y, w, h, time);

	// Draw "Enhanced Engine" text in the center
	color[0] = 1.0f;
	color[1] = 1.0f;
	color[2] = 1.0f;
	color[3] = 1.0f;

	UI_DrawString( x + w/2, y + h/2 - 20, "ENHANCED", UI_CENTER | UI_DROPSHADOW, color );
	UI_DrawString( x + w/2, y + h/2 + 10, "ENGINE", UI_CENTER | UI_DROPSHADOW, color );

	// Draw version info
	color[3] = 0.7f;
	UI_DrawString( x + w/2, y + h - 30, "Modernized idTech3", UI_CENTER, color );

	trap_R_SetColor( NULL );
}

/*
===============
MainMenu_DrawBannerParticles
===============
*/
static void MainMenu_DrawBannerParticles( int x, int y, int w, int h, float time )
{
	vec4_t color = {1.0f, 1.0f, 1.0f, 0.6f};
	int i;

	// Draw some animated particles around the banner
	for (i = 0; i < 8; i++) {
		float angle = (time + i * 0.5f) * 0.8f;
		float radius = 60 + 20 * sin(time * 0.3f + i);
		float px = x + w/2 + cos(angle) * radius;
		float py = y + h/2 + sin(angle) * radius;

		// Vary particle size and color
		float size = 3 + 2 * sin(time * 2.0f + i);
		color[0] = 0.5f + 0.5f * sin(time * 1.5f + i);  // Red variation
		color[1] = 0.5f + 0.5f * sin(time * 1.7f + i);  // Green variation
		color[2] = 0.5f + 0.5f * sin(time * 1.9f + i);  // Blue variation

		trap_R_SetColor( color );
		UI_FillRect( px - size/2, py - size/2, size, size, color );
	}

	trap_R_SetColor( NULL );
}

static sfxHandle_t ErrorMessage_Key([[maybe_unused]] int key)
{
	trap_Cvar_Set( "com_errorMessage", "" );
	UI_MainMenu();
	return (menu_null_sound);
}

/*
===============
Main_MenuDraw
TTimo: this function is common to the main menu and errorMessage menu
===============
*/

static void Main_MenuDraw( void )
{
	refdef_t		refdef;
	refEntity_t		ent;
	vec3_t			origin;
	vec3_t			angles;
	float			adjust;
	float			x, y, w, h;
	vec4_t			color = {0.25f, 0.35f, 0.95f, 1.0f};  // Improved blue color
	vec4_t			titleColor = {1.0f, 1.0f, 1.0f, 1.0f};  // White for title
	float			pulse = 1.0f + MAIN_MENU_PULSE_INTENSITY * sin( uis.realtime * MAIN_MENU_ANIMATION_SPEED );
	fontInfo_t     *titleFont = MainMenu_SelectFontForStyle( UI_BIGFONT );
	fontInfo_t     *smallFont = MainMenu_SelectFontForStyle( UI_SMALLFONT );

	// setup the refdef

	memset( &refdef, 0, sizeof( refdef ) );

	refdef.rdflags = RDF_NOWORLDMODEL;

	AxisClear( refdef.viewaxis );

	x = 0;
	y = 0;
	w = 640;
	h = 120;
	UI_AdjustFrom640( &x, &y, &w, &h );
	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;

	adjust = 0; // JDC: Kenneth asked me to stop this 1.0 * sin( (float)uis.realtime / 1000 );
	refdef.fov_x = 60 + adjust;
	refdef.fov_y = 19.6875 + adjust;

	refdef.time = uis.realtime;

	// Calculate model position similar to UI_DrawPlayer
	// Cube is 2x2x2 units (with scale 1.0f)
	// Position it so it fills the viewport
	float len = 0.7f * 2.0f; // 70% of model height (cube is 2 units tall)
	origin[0] = len / tan( DEG2RAD(refdef.fov_x) * 0.5f );
	origin[1] = 0.0f;  // centered
	origin[2] = 0.0f;  // centered vertically

	trap_R_ClearScene();

	// Add a light source so the model is visible
	vec3_t lightPos;
	VectorCopy( origin, lightPos );
	lightPos[1] += 20; // Position light slightly in front
	trap_R_AddLightToScene( lightPos, 200.0f, 1.0f, 1.0f, 1.0f );

	// add the model - try lazy loading if not already loaded
	qboolean triedLazyLoad = qfalse;
	if ( !s_main.bannerModel ) {
		// Try lazy loading of banner models
		const char *banner_candidates[] = {
			"models/mapobjects/banner/cube.obj",         // OBJ format (preferred)
			"models/mapobjects/banner/cube",             // Cube without extension
			MAIN_BANNER_MODEL,                           // banner5.md3
			"models/mapobjects/banner/banner5",          // Try without extension
			"models/mapobjects/banner/cube.md3",         // Cube MD3
			"models/mapobjects/grenade.md3",             // Grenade model
			"models/powerups/health/red.md3",            // Health pack
			"models/misc/telep.md3",                     // Teleporter
			"models/weapons2/rocketl/rocket.md3",        // Rocket
			"models/players/sarge/head.md3",             // Player head
			NULL
		};

		for (int i = 0; banner_candidates[i]; i++) {
			s_main.bannerModel = trap_R_RegisterModel( banner_candidates[i] );
			if ( s_main.bannerModel ) {
				Com_Printf( "Main_MenuDraw: Lazy loaded banner model '%s' (handle %d)\n", banner_candidates[i], s_main.bannerModel );
				triedLazyLoad = qtrue;
				break;
			}
		}
	}

	if ( s_main.bannerModel ) {
		memset( &ent, 0, sizeof(ent) );

		adjust = 5.0 * sin( (float)uis.realtime / 5000 );
		VectorSet( angles, 0, 180 + adjust, 0 );
		AnglesToAxis( angles, ent.axis );

		// Scale the model - cube is 2x2x2 units, scale it up to be visible
		// Scale axes to make model 25x larger (50x50x50 units)
		VectorScale( ent.axis[0], 1.0f, ent.axis[0] );
		VectorScale( ent.axis[1], 1.0f, ent.axis[1] );
		VectorScale( ent.axis[2], 1.0f, ent.axis[2] );
		ent.nonNormalizedAxes = qtrue;

		// Set frame for static model
		ent.frame = 0;
		ent.oldframe = 0;
		ent.backlerp = 0;

		// Add some color modulation to make it more visible
		ent.shader.rgba[0] = 255;
		ent.shader.rgba[1] = 255;
		ent.shader.rgba[2] = 255;
		ent.shader.rgba[3] = 255;

		ent.hModel = s_main.bannerModel;
		ent.reType = RT_MODEL;
		VectorCopy( origin, ent.origin );
		VectorCopy( origin, ent.lightingOrigin );
		// Use RF_MINLIGHT to ensure model always has some light
		ent.renderfx = RF_MINLIGHT | RF_LIGHTING_ORIGIN | RF_NOSHADOW;
		VectorCopy( ent.origin, ent.oldorigin );

		trap_R_AddRefEntityToScene( &ent );
	} else {
		// Fallback: Draw a 2D banner effect when 3D model fails to load
		MainMenu_DrawFallbackBanner();
		if ( !triedLazyLoad ) {
			Com_Printf( "Main_MenuDraw: bannerModel is NULL, model not loaded (using 2D fallback)\n" );
		}
	}

	trap_R_RenderScene( &refdef );

	if (strlen(s_errorMessage.errorMessage)) {
		UI_DrawProportionalString_AutoWrapped( 320, 192, 600, 20, s_errorMessage.errorMessage, UI_CENTER|UI_SMALLFONT|UI_DROPSHADOW, menu_text_color );
	}
	else {
		// standard menu drawing
		Menu_Draw( &s_main.menu );
	}

	// Draw mod title with improved styling and modern font rendering
	vec4_t titleColorPulse;
	Vector4Copy( titleColor, titleColorPulse );
	titleColorPulse[3] *= pulse;  // Pulse alpha for subtle effect
	MainMenu_TextPaint( 320, 372, 0.9f, color, "", UI_CENTER|UI_SMALLFONT, smallFont );
	MainMenu_TextPaint( 320, 400, 1.1f, titleColorPulse, "MY MOD TEMPLATE", UI_CENTER|UI_BIGFONT|UI_DROPSHADOW, titleFont );

	// Draw version info with better positioning and styling
	vec4_t versionColor = {
		MAIN_MENU_VERSION_COLOR_R,
		MAIN_MENU_VERSION_COLOR_G,
		MAIN_MENU_VERSION_COLOR_B,
		MAIN_MENU_VERSION_COLOR_A
	};
	MainMenu_TextPaint( 640 - 20, 480 - 16, 0.9f, versionColor, "^7v1.0", UI_RIGHT|UI_SMALLFONT, smallFont );
	
	// Show protocol version if not standard
	int protocol = (int)trap_Cvar_VariableValue("protocol");
	if (protocol != 68) { // OA_STD_PROTOCOL
		MainMenu_TextPaint( 20, 480 - 16, 0.9f, versionColor, va("^7Protocol: %i", protocol), UI_SMALLFONT, smallFont );
	}
}


/*
===============
UI_TeamArenaExists
===============
*/
static qboolean UI_TeamArenaExists( void )
{
	int		numdirs;
	char	dirlist[2048];
	char	*dirptr;
	char  *descptr;
	int		i;
	int		dirlen;

	numdirs = trap_FS_GetFileList( "$modlist", "", dirlist, sizeof(dirlist) );
	dirptr  = dirlist;
	for( i = 0; i < numdirs; i++ ) {
		dirlen = strlen( dirptr ) + 1;
		descptr = dirptr + dirlen;
		if ( Q_strequal(dirptr, "missionpack") ) {
			return qtrue;
		}
		dirptr += dirlen + strlen(descptr) + 1;
	}
	return qfalse;
}


/*
===============
UI_MainMenu

The main menu only comes up when not in a game,
so make sure that the attract loop server is down
and that local cinematics are killed
===============
*/
void UI_MainMenu( void )
{
	int		y;
	qboolean teamArena = qfalse;
	int		style = UI_CENTER | UI_DROPSHADOW | UI_BIGFONT;

	trap_Cvar_Set( "sv_killserver", "1" );
	trap_Cvar_SetValue( "handicap", 100 ); //Reset handicap during server change

	memset( &s_main, 0 ,sizeof(mainmenu_t) );
	memset( &s_errorMessage, 0 ,sizeof(errorMessage_t) );

	// com_errorMessage would need that too
	MainMenu_Cache();

	trap_Cvar_VariableStringBuffer( "com_errorMessage", s_errorMessage.errorMessage, sizeof(s_errorMessage.errorMessage) );
	if (strlen(s_errorMessage.errorMessage)) {
		s_errorMessage.menu.draw = Main_MenuDraw;
		s_errorMessage.menu.key = ErrorMessage_Key;
		s_errorMessage.menu.fullscreen = qtrue;
		s_errorMessage.menu.wrapAround = qtrue;
		s_errorMessage.menu.showlogo = qtrue;

		trap_Key_SetCatcher( KEYCATCH_UI );
		uis.menusp = 0;
		UI_PushMenu ( &s_errorMessage.menu );

		return;
	}

	s_main.menu.draw = Main_MenuDraw;
	s_main.menu.fullscreen = qtrue;
	s_main.menu.wrapAround = qtrue;
	s_main.menu.showlogo = qtrue;

	y = 130;
	s_main.singleplayer.generic.type		= MTYPE_PTEXT;
	s_main.singleplayer.generic.flags		= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS|QMF_HIGHLIGHT_IF_FOCUS;
	s_main.singleplayer.generic.x			= 320;
	s_main.singleplayer.generic.y			= y;
	s_main.singleplayer.generic.id			= ID_SINGLEPLAYER;
	s_main.singleplayer.generic.callback	= Main_MenuEvent;
	s_main.singleplayer.string				= "SINGLE PLAYER";
	s_main.singleplayer.color				= color_red;
	s_main.singleplayer.style				= style;
	s_main.singleplayer.generic.ownerdraw  = MainMenu_DrawMenuItem;

	y += MAIN_MENU_VERTICAL_SPACING;
	s_main.multiplayer.generic.type			= MTYPE_PTEXT;
	s_main.multiplayer.generic.flags		= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_main.multiplayer.generic.x			= 320;
	s_main.multiplayer.generic.y			= y;
	s_main.multiplayer.generic.id			= ID_MULTIPLAYER;
	s_main.multiplayer.generic.callback		= Main_MenuEvent;
	s_main.multiplayer.string				= "MULTIPLAYER";
	s_main.multiplayer.color				= color_red;
	s_main.multiplayer.style				= style;
	s_main.multiplayer.generic.ownerdraw   = MainMenu_DrawMenuItem;

	y += MAIN_MENU_VERTICAL_SPACING;
	s_main.setup.generic.type				= MTYPE_PTEXT;
	s_main.setup.generic.flags				= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS|QMF_HIGHLIGHT_IF_FOCUS;
	s_main.setup.generic.x					= 320;
	s_main.setup.generic.y					= y;
	s_main.setup.generic.id					= ID_SETUP;
	s_main.setup.generic.callback			= Main_MenuEvent;
	s_main.setup.string						= "SETTINGS";
	s_main.setup.color						= color_red;
	s_main.setup.style						= style;
	s_main.setup.generic.ownerdraw         = MainMenu_DrawMenuItem;

	y += MAIN_MENU_VERTICAL_SPACING;
	s_main.demos.generic.type				= MTYPE_PTEXT;
	s_main.demos.generic.flags				= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS|QMF_HIGHLIGHT_IF_FOCUS;
	s_main.demos.generic.x					= 320;
	s_main.demos.generic.y					= y;
	s_main.demos.generic.id					= ID_DEMOS;
	s_main.demos.generic.callback			= Main_MenuEvent;
	s_main.demos.string						= "REPLAYS";
	s_main.demos.color						= color_red;
	s_main.demos.style						= style;
	s_main.demos.generic.ownerdraw         = MainMenu_DrawMenuItem;

	y += MAIN_MENU_VERTICAL_SPACING;
	s_main.cinematics.generic.type			= MTYPE_PTEXT;
	s_main.cinematics.generic.flags			= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS|QMF_HIGHLIGHT_IF_FOCUS;
	s_main.cinematics.generic.x				= 320;
	s_main.cinematics.generic.y				= y;
	s_main.cinematics.generic.id			= ID_CINEMATICS;
	s_main.cinematics.generic.callback		= Main_MenuEvent;
	s_main.cinematics.string				= "CINEMATICS";
	s_main.cinematics.color					= color_red;
	s_main.cinematics.style					= style;
	s_main.cinematics.generic.ownerdraw    = MainMenu_DrawMenuItem;

	y += MAIN_MENU_VERTICAL_SPACING;
	s_main.challenges.generic.type			= MTYPE_PTEXT;
	s_main.challenges.generic.flags			= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS|QMF_HIGHLIGHT_IF_FOCUS;
	s_main.challenges.generic.x				= 320;
	s_main.challenges.generic.y				= y;
	s_main.challenges.generic.id			= ID_CHALLENGES;
	s_main.challenges.generic.callback		= Main_MenuEvent;
	s_main.challenges.string				= "STATISTICS";
	s_main.challenges.color					= color_red;
	s_main.challenges.style					= style;
	s_main.challenges.generic.ownerdraw    = MainMenu_DrawMenuItem;

	if (UI_TeamArenaExists()) {
		teamArena = qtrue;
		y += MAIN_MENU_VERTICAL_SPACING;
		s_main.teamArena.generic.type			= MTYPE_PTEXT;
		s_main.teamArena.generic.flags			= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
		s_main.teamArena.generic.x				= 320;
		s_main.teamArena.generic.y				= y;
		s_main.teamArena.generic.id				= ID_TEAMARENA;
		s_main.teamArena.generic.callback		= Main_MenuEvent;
		s_main.teamArena.string					= "MISSION PACK";
		s_main.teamArena.color					= color_red;
		s_main.teamArena.style					= style;
		s_main.teamArena.generic.ownerdraw     = MainMenu_DrawMenuItem;
	}

	y += MAIN_MENU_VERTICAL_SPACING;
	s_main.exit.generic.type				= MTYPE_PTEXT;
	s_main.exit.generic.flags				= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS|QMF_HIGHLIGHT_IF_FOCUS;
	s_main.exit.generic.x					= 320;
	s_main.exit.generic.y					= y;
	s_main.exit.generic.id					= ID_EXIT;
	s_main.exit.generic.callback			= Main_MenuEvent;
	s_main.exit.string						= "EXIT";
	s_main.exit.color						= color_red;
	s_main.exit.style						= style;
	s_main.exit.generic.ownerdraw          = MainMenu_DrawMenuItem;

	Menu_AddItem( &s_main.menu,	&s_main.singleplayer );
	Menu_AddItem( &s_main.menu,	&s_main.multiplayer );
	Menu_AddItem( &s_main.menu,	&s_main.setup );
	Menu_AddItem( &s_main.menu,	&s_main.demos );
	Menu_AddItem( &s_main.menu,	&s_main.cinematics );
	Menu_AddItem( &s_main.menu,	&s_main.challenges );
	if (teamArena) {
		Menu_AddItem( &s_main.menu,	&s_main.teamArena );
	}
	Menu_AddItem( &s_main.menu,	&s_main.exit );

	trap_Key_SetCatcher( KEYCATCH_UI );
	uis.menusp = 0;
	UI_PushMenu ( &s_main.menu );

}
