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


#define ID_SINGLEPLAYER			10
#define ID_MULTIPLAYER			11
#define ID_SETUP				12
#define ID_DEMOS				13
#define ID_CINEMATICS			14
#define ID_CHALLENGES                   18
#define ID_TEAMARENA		15
#define ID_MODS					16
#define ID_EXIT					17

#define MAIN_BANNER_MODEL				"models/mapobjects/banner/cube.obj"
#define MAIN_MENU_VERTICAL_SPACING		38  // Increased spacing for better readability
#define MAIN_MENU_ANIMATION_SPEED		0.003f  // Animation speed multiplier
#define MAIN_MENU_PULSE_INTENSITY		0.15f   // Pulse effect intensity


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
	char errorMessage[4096];
} errorMessage_t;

static errorMessage_t s_errorMessage;


/*
=================
Main_MenuEvent
=================
*/
void Main_MenuEvent (void* ptr, int event)
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
	s_main.bannerModel = trap_R_RegisterModel( MAIN_BANNER_MODEL );
	if ( s_main.bannerModel ) {
		Com_Printf( "MainMenu_Cache: Successfully loaded banner model '%s' (handle %d)\n", MAIN_BANNER_MODEL, s_main.bannerModel );
	} else {
		Com_Printf( "MainMenu_Cache: Failed to load banner model '%s'\n", MAIN_BANNER_MODEL );
	}
}

sfxHandle_t ErrorMessage_Key([[maybe_unused]] int key)
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

	// add the model
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
		// Debug: draw text if model failed to load
		static int debugPrintTime = 0;
		if ( uis.realtime > debugPrintTime + 5000 ) {
			Com_Printf( "Main_MenuDraw: bannerModel is NULL, model not loaded\n" );
			debugPrintTime = uis.realtime;
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

	// Draw mod title with improved styling
	vec4_t titleColorPulse;
	Vector4Copy( titleColor, titleColorPulse );
	titleColorPulse[3] *= pulse;  // Pulse alpha for subtle effect
	UI_DrawProportionalString( 320, 372, "", UI_CENTER|UI_SMALLFONT, color );
	UI_DrawProportionalString( 320, 400, "MY MOD TEMPLATE", UI_CENTER|UI_SMALLFONT|UI_DROPSHADOW, titleColorPulse );

	// Draw version info with better positioning and styling
	vec4_t versionColor = {0.7f, 0.7f, 0.7f, 0.8f};  // Subtle gray
	UI_DrawString( 640-20, 480-16, "^7v1.0", UI_SMALLFONT|UI_RIGHT, versionColor );
	if ((int)trap_Cvar_VariableValue("protocol")!=68) { // OA_STD_PROTOCOL
		UI_DrawString( 20, 480-16, va("^7Protocol: %i",(int)trap_Cvar_VariableValue("protocol")), UI_SMALLFONT, versionColor);
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
