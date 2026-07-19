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

#include <SDL3/SDL.h>
#include <math.h>

#include "../../client/client.h"
#include "sdl_glw.h"

void IN_Frame( void );

static cvar_t *in_keyboardDebug;
static cvar_t *in_forceCharset;

#ifdef USE_JOYSTICK
static SDL_Gamepad *gamepad;
static SDL_Joystick *stick = NULL;
static int joystick_hotplug_watch_added = 0;
#endif

static qboolean mouseAvailable = qfalse;
static qboolean mouseActive = qfalse;
static qboolean mouseRelativeWanted = qfalse;
static qboolean mouseRelativeActive = qfalse;

/* Last absolute mouse position when in UI mode (ungrabbed). Used to compute deltas
 * from SDL_EVENT_MOUSE_MOTION x,y so the UI cursor tracks correctly after warp-to-center. */
static int last_ui_mouse_x = -1;
static int last_ui_mouse_y = -1;

/* SDL3 reports float relative deltas (sub-pixel on HiDPI). Accumulate fractions so
 * slow motion is not truncated to zero by (int) cast. */
static float mouse_frac_x = 0.0f;
static float mouse_frac_y = 0.0f;

/* Diagnostics for input_status / in_mouseDebug. */
static float s_dbg_raw_xrel = 0.0f;
static float s_dbg_raw_yrel = 0.0f;
static int s_dbg_post_dx = 0;
static int s_dbg_post_dy = 0;
static int s_dbg_accum_dx = 0;
static int s_dbg_accum_dy = 0;
static int s_dbg_event_count = 0;
static int s_dbg_relative_fail = 0;

static cvar_t *in_mouse;
static cvar_t *in_mouseDebug;

#ifdef USE_JOYSTICK
static cvar_t *in_joystick;
static cvar_t *in_joystickThreshold;
static cvar_t *in_joystickNo;
static cvar_t *in_joystickUseAnalog;
static cvar_t *in_gamepadEvents;
static cvar_t *in_gamepadMappingFile;
static cvar_t *in_gamepadRumble;

static cvar_t *j_pitch;
static cvar_t *j_yaw;
static cvar_t *j_forward;
static cvar_t *j_side;
static cvar_t *j_up;
static cvar_t *j_pitch_axis;
static cvar_t *j_yaw_axis;
static cvar_t *j_forward_axis;
static cvar_t *j_side_axis;
static cvar_t *j_up_axis;
#endif

#define Com_QueueEvent Sys_QueEvent

static cvar_t *cl_consoleKeys;
static cvar_t *ui_displayScale;
static cvar_t *r_displayScale;
static cvar_t *ui_fileDialogResult;
static cvar_t *ui_fileDialogStatus;
static cvar_t *ui_fileDialogFilter;
static cvar_t *ui_imeComposition;
static cvar_t *ui_imeCompositionStart;
static cvar_t *ui_imeCompositionLength;
static cvar_t *ui_imeCandidates;

static int in_eventTime = 0;
static qboolean mouse_focus;

#ifdef SDL_INIT_CAMERA
typedef struct {
	SDL_Camera *camera;
	SDL_CameraID id;
	SDL_CameraSpec spec;
	qboolean hasSpec;
	qboolean active;
	int permission;
	unsigned int frameCount;
	int lastWidth;
	int lastHeight;
	SDL_PixelFormat lastFormat;
	Uint64 lastTimestampNS;
} sdlWebcamState_t;

static sdlWebcamState_t s_webcam;
static cvar_t *cl_webcamEnable;
static cvar_t *cl_webcamDevice;
static cvar_t *cl_webcamWidth;
static cvar_t *cl_webcamHeight;
static cvar_t *cl_webcamFps;
static cvar_t *cl_webcamPoll;
static cvar_t *in_webcam;
#endif

#define CTRL(a) ((a)-'a'+1)

static void IN_ClearImeState( void )
{
	if ( ui_imeComposition ) {
		Cvar_Set( "ui_imeComposition", "" );
	}
	if ( ui_imeCompositionStart ) {
		Cvar_Set( "ui_imeCompositionStart", "-1" );
	}
	if ( ui_imeCompositionLength ) {
		Cvar_Set( "ui_imeCompositionLength", "-1" );
	}
	if ( ui_imeCandidates ) {
		Cvar_Set( "ui_imeCandidates", "" );
	}
}

static void IN_UpdateDisplayScaleCvars( void )
{
	float scale;

	if ( !SDL_window ) {
		return;
	}

	scale = SDL_GetWindowDisplayScale( SDL_window );
	if ( scale <= 0.0f ) {
		scale = 1.0f;
	}

	if ( r_displayScale ) {
		Cvar_SetValue( "r_displayScale", scale );
	}
	if ( ui_displayScale ) {
		Cvar_SetValue( "ui_displayScale", scale );
	}
}

static void IN_UpdateTextInputArea( void )
{
	SDL_Rect rect;
	int catcher;
	int windowW, windowH;
	float scaleX, scaleY;
	int cursor;

	if ( !SDL_window ) {
		return;
	}

	SDL_GetWindowSize( SDL_window, &windowW, &windowH );
	if ( windowW <= 0 || windowH <= 0 ) {
		return;
	}

	scaleX = (float)windowW / 640.0f;
	scaleY = (float)windowH / 480.0f;
	catcher = Key_GetCatcher();

	if ( catcher & KEYCATCH_CONSOLE ) {
		rect.x = (int)( 16.0f * scaleX );
		rect.y = (int)( ( 480.0f - 24.0f ) * scaleY );
		rect.w = (int)( ( 640.0f - 24.0f ) * scaleX );
		rect.h = (int)( 20.0f * scaleY );
		cursor = (int)( g_consoleField.cursor * 8.0f * scaleX );
		SDL_SetTextInputArea( SDL_window, &rect, cursor );
		return;
	}

	if ( catcher & KEYCATCH_MESSAGE ) {
		const int skip = chat_team ? 10 : 5;

		rect.x = (int)( skip * 16.0f * scaleX );
		rect.y = (int)( 8.0f * scaleY );
		rect.w = (int)( ( 640.0f - ( skip + 1 ) * 16.0f ) * scaleX );
		rect.h = (int)( 24.0f * scaleY );
		cursor = (int)( chatField.cursor * 16.0f * scaleX );
		SDL_SetTextInputArea( SDL_window, &rect, cursor );
		return;
	}

	if ( catcher & KEYCATCH_UI ) {
		rect.x = (int)( 32.0f * scaleX );
		rect.y = (int)( ( 480.0f - 48.0f ) * scaleY );
		rect.w = (int)( ( 640.0f - 64.0f ) * scaleX );
		rect.h = (int)( 28.0f * scaleY );
		SDL_SetTextInputArea( SDL_window, &rect, 0 );
		return;
	}

	rect.x = 0;
	rect.y = 0;
	rect.w = windowW;
	rect.h = windowH;
	SDL_SetTextInputArea( SDL_window, &rect, 0 );
}

/*
===============
IN_PrintKey
===============
*/
static void IN_PrintKey( SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, keyNum_t key, qboolean down )
{
	if( down )
		Com_Printf( "+ " );
	else
		Com_Printf( "  " );

	Com_Printf( "Scancode: 0x%02x(%s) Sym: 0x%02x(%s)",
			scancode, SDL_GetScancodeName( scancode ),
			keycode, SDL_GetKeyName( keycode ) );

	if( mod & SDL_KMOD_LSHIFT )   Com_Printf( " SDL_KMOD_LSHIFT" );
	if( mod & SDL_KMOD_RSHIFT )   Com_Printf( " SDL_KMOD_RSHIFT" );
	if( mod & SDL_KMOD_LCTRL )    Com_Printf( " SDL_KMOD_LCTRL" );
	if( mod & SDL_KMOD_RCTRL )    Com_Printf( " SDL_KMOD_RCTRL" );
	if( mod & SDL_KMOD_LALT )     Com_Printf( " SDL_KMOD_LALT" );
	if( mod & SDL_KMOD_RALT )     Com_Printf( " SDL_KMOD_RALT" );
	if( mod & SDL_KMOD_LGUI )     Com_Printf( " SDL_KMOD_LGUI" );
	if( mod & SDL_KMOD_RGUI )     Com_Printf( " SDL_KMOD_RGUI" );
	if( mod & SDL_KMOD_NUM )      Com_Printf( " SDL_KMOD_NUM" );
	if( mod & SDL_KMOD_CAPS )     Com_Printf( " SDL_KMOD_CAPS" );
	if( mod & SDL_KMOD_MODE )     Com_Printf( " SDL_KMOD_MODE" );

	Com_Printf( " Q:0x%02x(%s)\n", key, Key_KeynumToString( key ) );
}


#define MAX_CONSOLE_KEYS 16

/*
===============
IN_IsConsoleKey

Could use SDL_Scancode when situation improves instead of both methods.
===============
*/
static qboolean IN_IsConsoleKey( keyNum_t key, int character )
{
	typedef struct consoleKey_s
	{
		enum
		{
			QUAKE_KEY,
			CHARACTER
		} type;

		union
		{
			keyNum_t key;
			int character;
		} u;
	} consoleKey_t;

	static consoleKey_t consoleKeys[ MAX_CONSOLE_KEYS ];
	static int numConsoleKeys = 0;
	int i;

	// Only parse the variable when it changes
	if ( cl_consoleKeys->modified )
	{
		const char *text_p, *token;

		cl_consoleKeys->modified = qfalse;
		text_p = cl_consoleKeys->string;
		numConsoleKeys = 0;

		while( numConsoleKeys < MAX_CONSOLE_KEYS )
		{
			consoleKey_t *c = &consoleKeys[ numConsoleKeys ];
			int charCode = 0;

			token = COM_Parse( &text_p );
			if( !token[ 0 ] )
				break;

			charCode = Com_HexStrToInt( token );

			if( charCode > 0 )
			{
				c->type = CHARACTER;
				c->u.character = charCode;
			}
			else
			{
				c->type = QUAKE_KEY;
				c->u.key = Key_StringToKeynum( token );

				// 0 isn't a key
				if ( c->u.key <= 0 )
					continue;
			}

			numConsoleKeys++;
		}
	}

	// If the character is the same as the key, prefer the character
	if ( key == (keyNum_t)character )
		key = 0;

	for ( i = 0; i < numConsoleKeys; i++ )
	{
		consoleKey_t *c = &consoleKeys[ i ];

		switch ( c->type )
		{
			case QUAKE_KEY:
				if( key && c->u.key == key )
					return qtrue;
				break;

			case CHARACTER:
				if( c->u.character == character )
					return qtrue;
				break;
		}
	}

	return qfalse;
}


/*
===============
IN_TranslateSDLToQ3Key
===============
*/
static keyNum_t IN_TranslateSDLToQ3Key( SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod, qboolean down )
{
	keyNum_t key = 0;

	if ( scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_0 )
	{
		// Always map the number keys as such even if they actually map
		// to other characters (eg, "1" is "&" on an AZERTY keyboard).
		// This is required for SDL before 2.0.6, except on Windows
		// which already had this behavior.
		if( scancode == SDL_SCANCODE_0 )
			key = '0';
		else
			key = '1' + scancode - SDL_SCANCODE_1;
	}
	else if ( in_forceCharset->integer > 0 )
	{
		if ( scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z )
		{
			key = 'a' + scancode - SDL_SCANCODE_A;
		}
		else
		{
			switch ( scancode )
			{
				case SDL_SCANCODE_MINUS:        key = '-';  break;
				case SDL_SCANCODE_EQUALS:       key = '=';  break;
				case SDL_SCANCODE_LEFTBRACKET:  key = '[';  break;
				case SDL_SCANCODE_RIGHTBRACKET: key = ']';  break;
				case SDL_SCANCODE_NONUSBACKSLASH:
				case SDL_SCANCODE_BACKSLASH:    key = '\\'; break;
				case SDL_SCANCODE_SEMICOLON:    key = ';';  break;
				case SDL_SCANCODE_APOSTROPHE:   key = '\''; break;
				case SDL_SCANCODE_COMMA:        key = ',';  break;
				case SDL_SCANCODE_PERIOD:       key = '.';  break;
				case SDL_SCANCODE_SLASH:        key = '/';  break;
				default:
					/* key = 0 */
					break;
			}
		}
	}

	if( !key && keycode >= SDLK_SPACE && keycode < SDLK_DELETE )
	{
		// These happen to match the ASCII chars
		key = (int)keycode;
	}
	else if( !key )
	{
		switch( keycode )
		{
			case SDLK_PAGEUP:       key = K_PGUP;          break;
			case SDLK_KP_9:         key = K_KP_PGUP;       break;
			case SDLK_PAGEDOWN:     key = K_PGDN;          break;
			case SDLK_KP_3:         key = K_KP_PGDN;       break;
			case SDLK_KP_7:         key = K_KP_HOME;       break;
			case SDLK_HOME:         key = K_HOME;          break;
			case SDLK_KP_1:         key = K_KP_END;        break;
			case SDLK_END:          key = K_END;           break;
			case SDLK_KP_4:         key = K_KP_LEFTARROW;  break;
			case SDLK_LEFT:         key = K_LEFTARROW;     break;
			case SDLK_KP_6:         key = K_KP_RIGHTARROW; break;
			case SDLK_RIGHT:        key = K_RIGHTARROW;    break;
			case SDLK_KP_2:         key = K_KP_DOWNARROW;  break;
			case SDLK_DOWN:         key = K_DOWNARROW;     break;
			case SDLK_KP_8:         key = K_KP_UPARROW;    break;
			case SDLK_UP:           key = K_UPARROW;       break;
			case SDLK_ESCAPE:       key = K_ESCAPE;        break;
			case SDLK_KP_ENTER:     key = K_KP_ENTER;      break;
			case SDLK_RETURN:       key = K_ENTER;         break;
			case SDLK_TAB:          key = K_TAB;           break;
			case SDLK_F1:           key = K_F1;            break;
			case SDLK_F2:           key = K_F2;            break;
			case SDLK_F3:           key = K_F3;            break;
			case SDLK_F4:           key = K_F4;            break;
			case SDLK_F5:           key = K_F5;            break;
			case SDLK_F6:           key = K_F6;            break;
			case SDLK_F7:           key = K_F7;            break;
			case SDLK_F8:           key = K_F8;            break;
			case SDLK_F9:           key = K_F9;            break;
			case SDLK_F10:          key = K_F10;           break;
			case SDLK_F11:          key = K_F11;           break;
			case SDLK_F12:          key = K_F12;           break;
			case SDLK_F13:          key = K_F13;           break;
			case SDLK_F14:          key = K_F14;           break;
			case SDLK_F15:          key = K_F15;           break;

			case SDLK_BACKSPACE:    key = K_BACKSPACE;     break;
			case SDLK_KP_PERIOD:    key = K_KP_DEL;        break;
			case SDLK_DELETE:       key = K_DEL;           break;
			case SDLK_PAUSE:        key = K_PAUSE;         break;

			case SDLK_LSHIFT:
			case SDLK_RSHIFT:       key = K_SHIFT;         break;

			case SDLK_LCTRL:
			case SDLK_RCTRL:        key = K_CTRL;          break;

#ifdef __APPLE__
			case SDLK_RGUI:
			case SDLK_LGUI:         key = K_COMMAND;       break;
#else
			case SDLK_RGUI:
			case SDLK_LGUI:         key = K_SUPER;         break;
#endif

			case SDLK_RALT:
			case SDLK_LALT:         key = K_ALT;           break;

			case SDLK_KP_5:         key = K_KP_5;          break;
			case SDLK_INSERT:       key = K_INS;           break;
			case SDLK_KP_0:         key = K_KP_INS;        break;
			case SDLK_KP_MULTIPLY:  key = '*'; /*K_KP_STAR;*/ break;
			case SDLK_KP_PLUS:      key = K_KP_PLUS;       break;
			case SDLK_KP_MINUS:     key = K_KP_MINUS;      break;
			case SDLK_KP_DIVIDE:    key = K_KP_SLASH;      break;

			case SDLK_MODE:         key = K_MODE;          break;
			case SDLK_HELP:         key = K_HELP;          break;
			case SDLK_PRINTSCREEN:  key = K_PRINT;         break;
			case SDLK_SYSREQ:       key = K_SYSREQ;        break;
			case SDLK_MENU:         key = K_MENU;          break;
			case SDLK_APPLICATION:	key = K_MENU;          break;
			case SDLK_POWER:        key = K_POWER;         break;
			case SDLK_UNDO:         key = K_UNDO;          break;
			case SDLK_SCROLLLOCK:   key = K_SCROLLOCK;     break;
			case SDLK_NUMLOCKCLEAR: key = K_KP_NUMLOCK;    break;
			case SDLK_CAPSLOCK:     key = K_CAPSLOCK;      break;

			default:
#if 1
				key = 0;
#else
				if( !( keycode & SDLK_SCANCODE_MASK ) && scancode <= 95 )
				{
					// Map Unicode characters to 95 world keys using the key's scan code.
					/* World keys may not cover all scancodes. */
					// Maybe create a map of scancode to quake key at start up and on
					// key map change; allocate world key numbers as needed similar
					// to SDL 1.2.
					key = K_WORLD_0 + (int)scancode;
				}
#endif
				break;
		}
	}

	if ( in_keyboardDebug->integer )
		IN_PrintKey( scancode, keycode, mod, key, down );

	if ( scancode == SDL_SCANCODE_GRAVE )
	{
		//SDL_Keycode translated = SDL_GetKeyFromScancode( SDL_SCANCODE_GRAVE );

		//if ( translated == SDLK_CARET )
		{
			// Console keys can't be bound or generate characters
			key = K_CONSOLE;
		}
	}
	else if ( IN_IsConsoleKey( key, 0 ) )
	{
		// Console keys can't be bound or generate characters
		key = K_CONSOLE;
	}

	return key;
}


/*
===============
IN_GobbleMotionEvents
===============
*/
static void IN_GobbleMouseEvents( void )
{
	SDL_Event dummy[ 1 ];
	int val = 0;

	// Gobble any mouse events
	SDL_PumpEvents();

	while( ( val = SDL_PeepEvents( dummy, ARRAY_LEN( dummy ), SDL_GETEVENT,
		SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_WHEEL ) ) > 0 ) { }

	if ( val < 0 )
		Com_Printf( "%s failed: %s\n", __func__, SDL_GetError() );
}


//#define DEBUG_EVENTS

/*
===============
IN_SetRelativeMouse
===============
*/
static qboolean IN_SetRelativeMouse( qboolean enable )
{
	qboolean ok;

	mouseRelativeWanted = enable;
	ok = SDL_SetWindowRelativeMouseMode( SDL_window, enable ) ? qtrue : qfalse;
	if ( !ok ) {
		s_dbg_relative_fail++;
		Com_DPrintf( "SDL_SetWindowRelativeMouseMode(%d) failed: %s\n",
			(int)enable, SDL_GetError() );
		mouseRelativeActive = SDL_GetWindowRelativeMouseMode( SDL_window ) ? qtrue : qfalse;
		return mouseRelativeActive;
	}
	mouseRelativeActive = enable;
	return qtrue;
}


/*
===============
IN_WarpToWindowCenter
===============
*/
static void IN_WarpToWindowCenter( void )
{
	int cx, cy;

	if ( !SDL_window ) {
		return;
	}
	/* Prefer live logical size; fall back to cached. */
	{
		int lw = 0, lh = 0;
		if ( SDL_GetWindowSize( SDL_window, &lw, &lh ) && lw > 0 && lh > 0 ) {
			glw_state.window_width = lw;
			glw_state.window_height = lh;
		}
	}
	cx = glw_state.window_width / 2;
	cy = glw_state.window_height / 2;
	if ( cx < 1 ) {
		cx = 1;
	}
	if ( cy < 1 ) {
		cy = 1;
	}
	SDL_WarpMouseInWindow( SDL_window, (float)cx, (float)cy );
}


void IN_GetAbsMouse( int *x, int *y ) {
	if ( x ) {
		*x = ( last_ui_mouse_x < 0 ) ? ( glw_state.window_width / 2 ) : last_ui_mouse_x;
	}
	if ( y ) {
		*y = ( last_ui_mouse_y < 0 ) ? ( glw_state.window_height / 2 ) : last_ui_mouse_y;
	}
}

/*
===============
IN_PointerUiMode

Q3 UI catcher or HavenRP City Menu — free look off, absolute cursor on.
===============
*/
static qboolean IN_PointerUiMode( void ) {
	if ( Key_GetCatcher() & KEYCATCH_UI ) {
		return qtrue;
	}
	return CL_RpMenuActive();
}

/*
===============
IN_ActivateMouse
===============
*/
static void IN_ActivateMouse( void )
{
	if ( !mouseAvailable )
		return;

	if ( !mouseActive )
	{
		IN_GobbleMouseEvents();
		mouse_frac_x = 0.0f;
		mouse_frac_y = 0.0f;

		IN_SetRelativeMouse( in_mouse->integer == 1 );
		SDL_SetWindowMouseGrab( SDL_window, true );

		if ( glw_state.isFullscreen )
			SDL_HideCursor();

		/* Only warp when relative mode is unavailable — warps under relative mode
		 * generate spurious motion on some Wayland/X11 stacks. */
		if ( !mouseRelativeActive ) {
			IN_WarpToWindowCenter();
		}

#ifdef DEBUG_EVENTS
		Com_Printf( "%4i %s\n", Sys_Milliseconds(), __func__ );
#endif
	}

	// in_nograb makes no sense in fullscreen mode
	if ( !glw_state.isFullscreen )
	{
		if ( in_nograb->modified || !mouseActive )
		{
			if ( in_nograb->integer ) {
				IN_SetRelativeMouse( qfalse );
				SDL_SetWindowMouseGrab( SDL_window, false );
			} else {
				IN_SetRelativeMouse( in_mouse->integer == 1 );
				SDL_SetWindowMouseGrab( SDL_window, true );
			}

			in_nograb->modified = qfalse;
		}
	}

	mouseActive = qtrue;
}


/*
===============
IN_DeactivateMouse
===============
*/
static void IN_DeactivateMouse( void )
{
	const char* drv = SDL_GetCurrentVideoDriver();
	qboolean uiActive = IN_PointerUiMode();

	if ( !mouseAvailable )
		return;

	if ( mouseActive )
	{
#ifdef DEBUG_EVENTS
		Com_Printf( "%4i %s\n", Sys_Milliseconds(), __func__ );
#endif
		IN_GobbleMouseEvents();

		SDL_SetWindowMouseGrab( SDL_window, false );
		IN_SetRelativeMouse( qfalse );
		mouse_frac_x = 0.0f;
		mouse_frac_y = 0.0f;

		if ( gw_active ) {
			IN_WarpToWindowCenter();
			/* Set last to 0 so the warp's motion event produces delta (cx,cy), moving
			 * the UI cursor from 0,0 to center. */
			last_ui_mouse_x = 0;
			last_ui_mouse_y = 0;
		} else
		{
			if ( glw_state.isFullscreen )
				SDL_ShowCursor();

			if ( drv && strcmp( drv, "x11" ) == 0 ) {
				SDL_WarpMouseGlobal( (float)( glw_state.desktop_width / 2 ), (float)( glw_state.desktop_height / 2 ) );
			}
		}

		mouseActive = qfalse;
	}

	/* In menu/UI mode the engine draws its own cursor, so hide the OS cursor to avoid
	 * a second pointer drifting away from the in-game one. Keep fullscreen non-UI
	 * paths hidden as well to match captured-mouse behavior. */
	if ( uiActive || glw_state.isFullscreen )
		SDL_HideCursor();
	else
		SDL_ShowCursor();
}


#ifdef USE_JOYSTICK
// We translate axes movement into keypresses
static const int joy_keys[16] = {
	K_LEFTARROW, K_RIGHTARROW,
	K_UPARROW, K_DOWNARROW,
	K_JOY17, K_JOY18,
	K_JOY19, K_JOY20,
	K_JOY21, K_JOY22,
	K_JOY23, K_JOY24,
	K_JOY25, K_JOY26,
	K_JOY27, K_JOY28
};

// translate hat events into keypresses
// the 4 highest buttons are used for the first hat ...
static const int hat_keys[16] = {
	K_JOY29, K_JOY30,
	K_JOY31, K_JOY32,
	K_JOY25, K_JOY26,
	K_JOY27, K_JOY28,
	K_JOY21, K_JOY22,
	K_JOY23, K_JOY24,
	K_JOY17, K_JOY18,
	K_JOY19, K_JOY20
};


struct
{
	qboolean buttons[SDL_GAMEPAD_BUTTON_COUNT + 1];
	unsigned int oldaxes;
	int oldaaxes[MAX_JOYSTICK_AXIS];
	unsigned int oldhats;
} stick_state;


/*
===============
IN_InitJoystick (forward decl for hotplug callback)
===============
*/
static void IN_InitJoystick( void );

/*
===============
IN_JoystickHotplugWatch
===============
Called when a joystick is added or removed. Re-initializes to pick up changes.
*/
static bool SDLCALL IN_JoystickHotplugWatch( void *userdata, SDL_Event *event )
{
	(void)userdata;
	if ( event->type == SDL_EVENT_JOYSTICK_ADDED || event->type == SDL_EVENT_JOYSTICK_REMOVED )
	{
		IN_InitJoystick();
		if ( event->type == SDL_EVENT_JOYSTICK_ADDED )
			Com_DPrintf( "Joystick hotplug: device added, re-initialized\n" );
		else
			Com_DPrintf( "Joystick hotplug: device removed, re-initialized\n" );
	}
	return true;
}


/*
===============
IN_InitJoystick
===============
*/
static void IN_InitJoystick( void )
{
	cvar_t *cv;
	int i = 0;
	int total = 0;
	char buf[16384] = "";
	SDL_JoystickID *joyIds = NULL;

	if (gamepad)
		SDL_CloseGamepad(gamepad);

	if (stick != NULL)
		SDL_CloseJoystick(stick);

	stick = NULL;
	gamepad = NULL;
	memset(&stick_state, 0, sizeof (stick_state));

	if (!SDL_WasInit(SDL_INIT_JOYSTICK))
	{
		Com_DPrintf("Calling SDL_Init(SDL_INIT_JOYSTICK)...\n");
		if (!SDL_Init(SDL_INIT_JOYSTICK))
		{
			Com_DPrintf("SDL_Init(SDL_INIT_JOYSTICK) failed: %s\n", SDL_GetError());
			return;
		}
		Com_DPrintf("SDL_Init(SDL_INIT_JOYSTICK) passed.\n");
	}

	if (!SDL_WasInit(SDL_INIT_GAMEPAD))
	{
		Com_DPrintf("Calling SDL_Init(SDL_INIT_GAMEPAD)...\n");
		if (!SDL_Init(SDL_INIT_GAMEPAD))
		{
			Com_DPrintf("SDL_Init(SDL_INIT_GAMEPAD) failed: %s\n", SDL_GetError());
			return;
		}
		Com_DPrintf("SDL_Init(SDL_INIT_GAMEPAD) passed.\n");
	}

	SDL_SetGamepadEventsEnabled( !in_gamepadEvents || in_gamepadEvents->integer ? true : false );
	if ( in_gamepadMappingFile && in_gamepadMappingFile->string[0] ) {
		int added = SDL_AddGamepadMappingsFromFile( in_gamepadMappingFile->string );
		if ( added < 0 )
			Com_DPrintf( "SDL3 gamepad mappings '%s' failed: %s\n", in_gamepadMappingFile->string, SDL_GetError() );
		else if ( added > 0 )
			Com_DPrintf( "SDL3 gamepad mappings: loaded %d from %s\n", added, in_gamepadMappingFile->string );
	}

	joyIds = SDL_GetJoysticks(&total);
	Com_DPrintf("%d possible joysticks\n", total);

	// Print list and build cvar to allow ui to select joystick.
	for (i = 0; i < total; i++)
	{
		const char *name = joyIds ? SDL_GetJoystickNameForID(joyIds[i]) : NULL;
		Q_strcat(buf, sizeof(buf), name ? name : "(unknown)");
		Q_strcat(buf, sizeof(buf), "\n");
	}

	cv = Cvar_Get( "in_availableJoysticks", buf, CVAR_ROM );
	Cvar_SetDescription( cv, "List of available joysticks." );

	if( !in_joystick->integer ) {
		Com_DPrintf( "Joystick is not active.\n" );
		if ( joyIds )
			SDL_free( joyIds );
		SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
		return;
	}

	in_joystickNo = Cvar_Get( "in_joystickNo", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( in_joystickNo, "Select which joystick to use." );
	if( in_joystickNo->integer < 0 || in_joystickNo->integer >= total )
		Cvar_Set( "in_joystickNo", "0" );

	in_joystickUseAnalog = Cvar_Get( "in_joystickUseAnalog", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( in_joystickUseAnalog, "Do not translate joystick axis events to keyboard commands." );

	if ( !joyIds || total <= 0 ) {
		Com_DPrintf( "No joystick opened: no devices\n" );
		if ( joyIds )
			SDL_free( joyIds );
		return;
	}

	stick = SDL_OpenJoystick( joyIds[in_joystickNo->integer] );

	if (stick == NULL) {
		Com_DPrintf( "No joystick opened: %s\n", SDL_GetError() );
		SDL_free( joyIds );
		return;
	}

	if (SDL_IsGamepad(joyIds[in_joystickNo->integer]))
		gamepad = SDL_OpenGamepad(joyIds[in_joystickNo->integer]);

	if ( gamepad ) {
		SDL_PropertiesID props = SDL_GetGamepadProperties( gamepad );
		if ( SDL_GetBooleanProperty( props, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false ) ) {
			(void)SDL_SetGamepadLED( gamepad, 32, 96, 192 );
		}
	}

	Com_DPrintf( "Joystick %d opened\n", in_joystickNo->integer );
	Com_DPrintf( "Name:       %s\n", SDL_GetJoystickNameForID(joyIds[in_joystickNo->integer]) );
	Com_DPrintf( "Axes:       %d\n", SDL_GetNumJoystickAxes(stick) );
	Com_DPrintf( "Hats:       %d\n", SDL_GetNumJoystickHats(stick) );
	Com_DPrintf( "Buttons:    %d\n", SDL_GetNumJoystickButtons(stick) );
	Com_DPrintf( "Balls:      %d\n", SDL_GetNumJoystickBalls(stick) );
	Com_DPrintf( "Use Analog: %s\n", in_joystickUseAnalog->integer ? "Yes" : "No" );
	Com_DPrintf( "Is gamepad: %s\n", gamepad ? "Yes" : "No" );

	SDL_free( joyIds );

	/* Register hotplug callback if joystick is active */
	if ( in_joystick->integer && !joystick_hotplug_watch_added )
	{
		SDL_AddEventWatch( IN_JoystickHotplugWatch, NULL );
		joystick_hotplug_watch_added = 1;
		Com_DPrintf( "Joystick hotplug: event watch registered\n" );
	}
}


/*
===============
IN_ShutdownJoystick
===============
*/
static void IN_ShutdownJoystick( void )
{
	if ( !SDL_WasInit( SDL_INIT_GAMEPAD ) )
		return;

	if ( !SDL_WasInit( SDL_INIT_JOYSTICK ) )
		return;

	if (gamepad)
	{
		SDL_CloseGamepad(gamepad);
		gamepad = NULL;
	}

	if (stick)
	{
		SDL_CloseJoystick(stick);
		stick = NULL;
	}

	if ( joystick_hotplug_watch_added )
	{
		SDL_RemoveEventWatch( IN_JoystickHotplugWatch, NULL );
		joystick_hotplug_watch_added = 0;
	}

	SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

static void IN_GamepadStatus_f( void )
{
	int total = 0;
	SDL_JoystickID *joyIds;
	int i;

	joyIds = SDL_GetJoysticks( &total );
	Com_Printf( "SDL3 gamepad status:\n" );
	Com_Printf( "  in_joystick=%d selected=%d analog=%d threshold=%.3f events=%d rumble=%d\n",
		in_joystick ? in_joystick->integer : 0,
		in_joystickNo ? in_joystickNo->integer : 0,
		in_joystickUseAnalog ? in_joystickUseAnalog->integer : 0,
		in_joystickThreshold ? in_joystickThreshold->value : 0.0f,
		in_gamepadEvents ? in_gamepadEvents->integer : 1,
		in_gamepadRumble ? in_gamepadRumble->integer : 1 );
	Com_Printf( "  devices=%d activeJoystick=%s activeGamepad=%s\n",
		total, stick ? "yes" : "no", gamepad ? "yes" : "no" );

	if ( gamepad ) {
		SDL_PropertiesID props = SDL_GetGamepadProperties( gamepad );
		Com_Printf( "  activeName=%s rumbleCap=%s triggerRumbleCap=%s\n",
			SDL_GetGamepadName( gamepad ) ? SDL_GetGamepadName( gamepad ) : "(unknown)",
			SDL_GetBooleanProperty( props, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false ) ? "yes" : "no",
			SDL_GetBooleanProperty( props, SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN, false ) ? "yes" : "no" );
	} else if ( stick ) {
		char guid[64];
		SDL_GUIDToString( SDL_GetJoystickGUID( stick ), guid, sizeof( guid ) );
		Com_Printf( "  activeName=%s guid=%s axes=%d buttons=%d hats=%d\n",
			SDL_GetJoystickName( stick ) ? SDL_GetJoystickName( stick ) : "(unknown)", guid,
			SDL_GetNumJoystickAxes( stick ), SDL_GetNumJoystickButtons( stick ), SDL_GetNumJoystickHats( stick ) );
	}

	for ( i = 0; joyIds && i < total; i++ ) {
		char guid[64];
		SDL_GUIDToString( SDL_GetJoystickGUIDForID( joyIds[i] ), guid, sizeof( guid ) );
		Com_Printf( "  [%d] %s%s guid=%s\n", i,
			SDL_GetJoystickNameForID( joyIds[i] ) ? SDL_GetJoystickNameForID( joyIds[i] ) : "(unknown)",
			SDL_IsGamepad( joyIds[i] ) ? " (gamepad)" : "", guid );
	}

	if ( joyIds ) {
		SDL_free( joyIds );
	}
}

static void IN_GamepadLoadMappings_f( void )
{
	const char *path;
	int added;

	if ( Cmd_Argc() > 1 ) {
		path = Cmd_Argv( 1 );
	} else if ( in_gamepadMappingFile && in_gamepadMappingFile->string[0] ) {
		path = in_gamepadMappingFile->string;
	} else {
		Com_Printf( "usage: gamepad_load_mappings <path-to-gamecontrollerdb.txt>\n" );
		return;
	}

	added = SDL_AddGamepadMappingsFromFile( path );
	if ( added < 0 ) {
		Com_Printf( "SDL3 gamepad mapping load failed for '%s': %s\n", path, SDL_GetError() );
		return;
	}

	Com_Printf( "SDL3 gamepad mappings: loaded %d mapping(s) from %s\n", added, path );
	IN_InitJoystick();
}

static void IN_GamepadRumble_f( void )
{
	int low, high, ms;

	if ( !gamepad ) {
		Com_Printf( "gamepad_rumble: no active SDL3 gamepad\n" );
		return;
	}
	if ( in_gamepadRumble && !in_gamepadRumble->integer ) {
		Com_Printf( "gamepad_rumble: disabled by in_gamepadRumble 0\n" );
		return;
	}
	if ( Cmd_Argc() < 4 ) {
		Com_Printf( "usage: gamepad_rumble <low 0-65535> <high 0-65535> <milliseconds>\n" );
		return;
	}

	low = Com_Clamp( 0, 65535, atoi( Cmd_Argv( 1 ) ) );
	high = Com_Clamp( 0, 65535, atoi( Cmd_Argv( 2 ) ) );
	ms = Com_Clamp( 0, 10000, atoi( Cmd_Argv( 3 ) ) );
	if ( !SDL_RumbleGamepad( gamepad, (Uint16)low, (Uint16)high, (Uint32)ms ) ) {
		Com_Printf( "gamepad_rumble failed: %s\n", SDL_GetError() );
	}
}

static void IN_GamepadTriggerRumble_f( void )
{
	int left, right, ms;

	if ( !gamepad ) {
		Com_Printf( "gamepad_trigger_rumble: no active SDL3 gamepad\n" );
		return;
	}
	if ( in_gamepadRumble && !in_gamepadRumble->integer ) {
		Com_Printf( "gamepad_trigger_rumble: disabled by in_gamepadRumble 0\n" );
		return;
	}
	if ( Cmd_Argc() < 4 ) {
		Com_Printf( "usage: gamepad_trigger_rumble <left 0-65535> <right 0-65535> <milliseconds>\n" );
		return;
	}

	left = Com_Clamp( 0, 65535, atoi( Cmd_Argv( 1 ) ) );
	right = Com_Clamp( 0, 65535, atoi( Cmd_Argv( 2 ) ) );
	ms = Com_Clamp( 0, 10000, atoi( Cmd_Argv( 3 ) ) );
	if ( !SDL_RumbleGamepadTriggers( gamepad, (Uint16)left, (Uint16)right, (Uint32)ms ) ) {
		Com_Printf( "gamepad_trigger_rumble failed: %s\n", SDL_GetError() );
	}
}

static void IN_GamepadLed_f( void )
{
	int red, green, blue;
	SDL_PropertiesID props;

	if ( !gamepad ) {
		Com_Printf( "gamepad_led: no active SDL3 gamepad\n" );
		return;
	}
	if ( Cmd_Argc() < 4 ) {
		Com_Printf( "usage: gamepad_led <red 0-255> <green 0-255> <blue 0-255>\n" );
		return;
	}

	props = SDL_GetGamepadProperties( gamepad );
	if ( !SDL_GetBooleanProperty( props, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false ) ) {
		Com_Printf( "gamepad_led: active gamepad has no RGB LED capability\n" );
		return;
	}

	red = Com_Clamp( 0, 255, atoi( Cmd_Argv( 1 ) ) );
	green = Com_Clamp( 0, 255, atoi( Cmd_Argv( 2 ) ) );
	blue = Com_Clamp( 0, 255, atoi( Cmd_Argv( 3 ) ) );
	if ( !SDL_SetGamepadLED( gamepad, (Uint8)red, (Uint8)green, (Uint8)blue ) ) {
		Com_Printf( "gamepad_led failed: %s\n", SDL_GetError() );
	}
}

typedef struct sdlDialogResult_s
{
	char *path;
	int filter;
	qboolean cancelled;
	qboolean failed;
} sdlDialogResult_t;

static void SDLCALL IN_FileDialogApplyResult( void *userdata )
{
	sdlDialogResult_t *result = (sdlDialogResult_t *)userdata;

	if ( ui_fileDialogResult ) {
		Cvar_Set( "ui_fileDialogResult", result && result->path ? result->path : "" );
	}
	if ( ui_fileDialogFilter ) {
		Cvar_SetValue( "ui_fileDialogFilter", result ? result->filter : -1 );
	}
	if ( ui_fileDialogStatus ) {
		if ( !result ) {
			Cvar_Set( "ui_fileDialogStatus", "error" );
		} else if ( result->failed ) {
			Cvar_Set( "ui_fileDialogStatus", "error" );
		} else if ( result->cancelled ) {
			Cvar_Set( "ui_fileDialogStatus", "cancelled" );
		} else {
			Cvar_Set( "ui_fileDialogStatus", "selected" );
		}
	}

	if ( result ) {
		if ( result->failed ) {
			Com_Printf( "filedialog: %s\n", SDL_GetError() );
		} else if ( result->cancelled ) {
			Com_Printf( "filedialog: cancelled\n" );
		} else if ( result->path ) {
			Com_Printf( "filedialog: %s\n", result->path );
		}
		SDL_free( result->path );
		SDL_free( result );
	}
}

static void SDLCALL IN_FileDialogCallback( void *userdata, const char * const *filelist, int filter )
{
	sdlDialogResult_t *result = (sdlDialogResult_t *)SDL_calloc( 1, sizeof( *result ) );

	(void)userdata;
	if ( !result ) {
		return;
	}

	result->filter = filter;
	if ( !filelist ) {
		result->failed = qtrue;
	} else if ( !filelist[0] ) {
		result->cancelled = qtrue;
	} else {
		result->path = SDL_strdup( filelist[0] );
	}

	SDL_RunOnMainThread( IN_FileDialogApplyResult, result, false );
}

static void IN_FileDialogOpen_f( void )
{
	const char *defaultLocation = Cmd_Argc() > 1 ? Cmd_ArgsFrom( 1 ) : NULL;
	static const SDL_DialogFileFilter filters[] = {
		{ "All Files", "*" }
	};

	if ( ui_fileDialogStatus ) {
		Cvar_Set( "ui_fileDialogStatus", "pending" );
	}
	SDL_ShowOpenFileDialog( IN_FileDialogCallback, NULL, SDL_window, filters, ARRAY_LEN( filters ), defaultLocation, false );
}

static void IN_FileDialogSave_f( void )
{
	const char *defaultLocation = Cmd_Argc() > 1 ? Cmd_ArgsFrom( 1 ) : NULL;
	static const SDL_DialogFileFilter filters[] = {
		{ "All Files", "*" }
	};

	if ( ui_fileDialogStatus ) {
		Cvar_Set( "ui_fileDialogStatus", "pending" );
	}
	SDL_ShowSaveFileDialog( IN_FileDialogCallback, NULL, SDL_window, filters, ARRAY_LEN( filters ), defaultLocation );
}


static qboolean KeyToAxisAndSign(int keynum, int *outAxis, int *outSign)
{
	const char *bind;

	if (!keynum)
		return qfalse;

	bind = Key_GetBinding(keynum);

	if (!bind || *bind != '+')
		return qfalse;

	*outSign = 0;

	if (Q_stricmp(bind, "+forward") == 0)
	{
		*outAxis = j_forward_axis->integer;
		*outSign = j_forward->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+back") == 0)
	{
		*outAxis = j_forward_axis->integer;
		*outSign = j_forward->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+moveleft") == 0)
	{
		*outAxis = j_side_axis->integer;
		*outSign = j_side->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+moveright") == 0)
	{
		*outAxis = j_side_axis->integer;
		*outSign = j_side->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+lookup") == 0)
	{
		*outAxis = j_pitch_axis->integer;
		*outSign = j_pitch->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+lookdown") == 0)
	{
		*outAxis = j_pitch_axis->integer;
		*outSign = j_pitch->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+left") == 0)
	{
		*outAxis = j_yaw_axis->integer;
		*outSign = j_yaw->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+right") == 0)
	{
		*outAxis = j_yaw_axis->integer;
		*outSign = j_yaw->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+moveup") == 0)
	{
		*outAxis = j_up_axis->integer;
		*outSign = j_up->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+movedown") == 0)
	{
		*outAxis = j_up_axis->integer;
		*outSign = j_up->value > 0.0f ? -1 : 1;
	}

	return *outSign != 0;
}


/*
===============
IN_GamepadMove
===============
*/
static void IN_GamepadMove( void )
{
	int i;
	int translatedAxes[MAX_JOYSTICK_AXIS];
	qboolean translatedAxesSet[MAX_JOYSTICK_AXIS];

	SDL_UpdateGamepads();

	// check buttons
	for (i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++)
	{
		qboolean pressed = SDL_GetGamepadButton(gamepad, (SDL_GamepadButton)i) ? qtrue : qfalse;
		if (pressed != stick_state.buttons[i])
		{
			if ( i >= SDL_GAMEPAD_BUTTON_MISC1 ) {
				Com_QueueEvent(in_eventTime, SE_KEY, K_PAD0_MISC1 + i - SDL_GAMEPAD_BUTTON_MISC1, pressed, 0, NULL);
			} else
			{
				Com_QueueEvent(in_eventTime, SE_KEY, K_PAD0_A + i, pressed, 0, NULL);
			}
			stick_state.buttons[i] = pressed;
		}
	}

	// must defer translated axes until all real axes are processed
	// must be done this way to prevent a later mapped axis from zeroing out a previous one
	if (in_joystickUseAnalog->integer)
	{
		for (i = 0; i < MAX_JOYSTICK_AXIS; i++)
		{
			translatedAxes[i] = 0;
			translatedAxesSet[i] = qfalse;
		}
	}

	// check axes
	for (i = 0; i < SDL_GAMEPAD_AXIS_COUNT; i++)
	{
		int axis = SDL_GetGamepadAxis(gamepad, (SDL_GamepadAxis)i);
		int oldAxis = stick_state.oldaaxes[i];

		// Smoothly ramp from dead zone to maximum value
		float f = ((float)abs(axis) / 32767.0f - in_joystickThreshold->value) / (1.0f - in_joystickThreshold->value);

		if (f < 0.0f)
			f = 0.0f;

		axis = (int)(32767 * ((axis < 0) ? -f : f));

		if (axis != oldAxis)
		{
			const int negMap[SDL_GAMEPAD_AXIS_COUNT] = { K_PAD0_LEFTSTICK_LEFT,  K_PAD0_LEFTSTICK_UP,   K_PAD0_RIGHTSTICK_LEFT,  K_PAD0_RIGHTSTICK_UP, 0, 0 };
			const int posMap[SDL_GAMEPAD_AXIS_COUNT] = { K_PAD0_LEFTSTICK_RIGHT, K_PAD0_LEFTSTICK_DOWN, K_PAD0_RIGHTSTICK_RIGHT, K_PAD0_RIGHTSTICK_DOWN, K_PAD0_LEFTTRIGGER, K_PAD0_RIGHTTRIGGER };

			qboolean posAnalog = qfalse, negAnalog = qfalse;
			int negKey = negMap[i];
			int posKey = posMap[i];

			if (in_joystickUseAnalog->integer)
			{
				int posAxis = 0, posSign = 0, negAxis = 0, negSign = 0;

				// get axes and axes signs for keys if available
				posAnalog = KeyToAxisAndSign(posKey, &posAxis, &posSign);
				negAnalog = KeyToAxisAndSign(negKey, &negAxis, &negSign);

				// positive to negative/neutral -> keyup if axis hasn't yet been set
				if (posAnalog && !translatedAxesSet[posAxis] && oldAxis > 0 && axis <= 0)
				{
					translatedAxes[posAxis] = 0;
					translatedAxesSet[posAxis] = qtrue;
				}

				// negative to positive/neutral -> keyup if axis hasn't yet been set
				if (negAnalog && !translatedAxesSet[negAxis] && oldAxis < 0 && axis >= 0)
				{
					translatedAxes[negAxis] = 0;
					translatedAxesSet[negAxis] = qtrue;
				}

				// negative/neutral to positive -> keydown
				if (posAnalog && axis > 0)
				{
					translatedAxes[posAxis] = axis * posSign;
					translatedAxesSet[posAxis] = qtrue;
				}

				// positive/neutral to negative -> keydown
				if (negAnalog && axis < 0)
				{
					translatedAxes[negAxis] = -axis * negSign;
					translatedAxesSet[negAxis] = qtrue;
				}
			}

			// keyups first so they get overridden by keydowns later

			// positive to negative/neutral -> keyup
			if (!posAnalog && posKey && oldAxis > 0 && axis <= 0)
				Com_QueueEvent(in_eventTime, SE_KEY, posKey, qfalse, 0, NULL);

			// negative to positive/neutral -> keyup
			if (!negAnalog && negKey && oldAxis < 0 && axis >= 0)
				Com_QueueEvent(in_eventTime, SE_KEY, negKey, qfalse, 0, NULL);

			// negative/neutral to positive -> keydown
			if (!posAnalog && posKey && oldAxis <= 0 && axis > 0)
				Com_QueueEvent(in_eventTime, SE_KEY, posKey, qtrue, 0, NULL);

			// positive/neutral to negative -> keydown
			if (!negAnalog && negKey && oldAxis >= 0 && axis < 0)
				Com_QueueEvent(in_eventTime, SE_KEY, negKey, qtrue, 0, NULL);

			stick_state.oldaaxes[i] = axis;
		}
	}

	// set translated axes
	if (in_joystickUseAnalog->integer)
	{
		for (i = 0; i < MAX_JOYSTICK_AXIS; i++)
		{
			if (translatedAxesSet[i])
				Com_QueueEvent(in_eventTime, SE_JOYSTICK_AXIS, i, translatedAxes[i], 0, NULL);
		}
	}
}


/*
===============
IN_JoyMove
===============
*/
static void IN_JoyMove( void )
{
	unsigned int axes = 0;
	unsigned int hats = 0;
	int total = 0;
	int i = 0;

	in_eventTime = Sys_Milliseconds();

	if (gamepad)
	{
		IN_GamepadMove();
		return;
	}

	if (!stick)
		return;

	SDL_UpdateJoysticks();

	// update the ball state.
	total = SDL_GetNumJoystickBalls(stick);
	if (total > 0)
	{
		int balldx = 0;
		int balldy = 0;
		for (i = 0; i < total; i++)
		{
			int dx = 0;
			int dy = 0;
			SDL_GetJoystickBall(stick, i, &dx, &dy);
			balldx += dx;
			balldy += dy;
		}
		if (balldx || balldy)
		{
			/* May need adjustment for stick balls vs mice. */
			// Scale like the mouse input...
			if (abs(balldx) > 1)
				balldx *= 2;
			if (abs(balldy) > 1)
				balldy *= 2;
			Com_QueueEvent( in_eventTime, SE_MOUSE, balldx, balldy, 0, NULL );
		}
	}

	// now query the stick buttons...
	total = SDL_GetNumJoystickButtons(stick);
	if (total > 0)
	{
		if ( total > 0 && (size_t)total > ARRAY_LEN(stick_state.buttons) )
			total = (int)ARRAY_LEN(stick_state.buttons);
		for (i = 0; i < total; i++)
		{
			qboolean pressed = (SDL_GetJoystickButton(stick, i));
			if (pressed != stick_state.buttons[i])
			{
				Com_QueueEvent( in_eventTime, SE_KEY, K_JOY1 + i, pressed, 0, NULL );
				stick_state.buttons[i] = pressed;
			}
		}
	}

	// look at the hats...
	total = SDL_GetNumJoystickHats(stick);
	if (total > 0)
	{
		if (total > 4) total = 4;
		for (i = 0; i < total; i++)
		{
			((Uint8 *)&hats)[i] = SDL_GetJoystickHat(stick, i);
		}
	}

	// update hat state
	if (hats != stick_state.oldhats)
	{
		for( i = 0; i < 4; i++ ) {
			if( ((Uint8 *)&hats)[i] != ((Uint8 *)&stick_state.oldhats)[i] ) {
				// release event
				switch( ((Uint8 *)&stick_state.oldhats)[i] ) {
					case SDL_HAT_UP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qfalse, 0, NULL );
						break;
					case SDL_HAT_RIGHT:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qfalse, 0, NULL );
						break;
					case SDL_HAT_DOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qfalse, 0, NULL );
						break;
					case SDL_HAT_LEFT:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qfalse, 0, NULL );
						break;
					case SDL_HAT_RIGHTUP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qfalse, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qfalse, 0, NULL );
						break;
					case SDL_HAT_RIGHTDOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qfalse, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qfalse, 0, NULL );
						break;
					case SDL_HAT_LEFTUP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qfalse, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qfalse, 0, NULL );
						break;
					case SDL_HAT_LEFTDOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qfalse, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qfalse, 0, NULL );
						break;
					default:
						break;
				}
				// press event
				switch( ((Uint8 *)&hats)[i] ) {
					case SDL_HAT_UP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qtrue, 0, NULL );
						break;
					case SDL_HAT_RIGHT:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qtrue, 0, NULL );
						break;
					case SDL_HAT_DOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qtrue, 0, NULL );
						break;
					case SDL_HAT_LEFT:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qtrue, 0, NULL );
						break;
					case SDL_HAT_RIGHTUP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qtrue, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qtrue, 0, NULL );
						break;
					case SDL_HAT_RIGHTDOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qtrue, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qtrue, 0, NULL );
						break;
					case SDL_HAT_LEFTUP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qtrue, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qtrue, 0, NULL );
						break;
					case SDL_HAT_LEFTDOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qtrue, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qtrue, 0, NULL );
						break;
					default:
						break;
				}
			}
		}
	}

	// save hat state
	stick_state.oldhats = hats;

	// finally, look at the axes...
	total = SDL_GetNumJoystickAxes(stick);
	if (total > 0)
	{
		if (in_joystickUseAnalog->integer)
		{
			if (total > MAX_JOYSTICK_AXIS) total = MAX_JOYSTICK_AXIS;
			for (i = 0; i < total; i++)
			{
				Sint16 axis = SDL_GetJoystickAxis(stick, i);
				float f = ( (float) abs(axis) ) / 32767.0f;
				
				if( f < in_joystickThreshold->value ) axis = 0;

				if ( axis != stick_state.oldaaxes[i] )
				{
					Com_QueueEvent( in_eventTime, SE_JOYSTICK_AXIS, i, axis, 0, NULL );
					stick_state.oldaaxes[i] = axis;
				}
			}
		}
		else
		{
			if (total > 16) total = 16;
			for (i = 0; i < total; i++)
			{
				Sint16 axis = SDL_GetJoystickAxis(stick, i);
				float f = ( (float) axis ) / 32767.0f;
				if( f < -in_joystickThreshold->value ) {
					axes |= ( 1 << ( i * 2 ) );
				} else if( f > in_joystickThreshold->value ) {
					axes |= ( 1 << ( ( i * 2 ) + 1 ) );
				}
			}
		}
	}

	/* Time to update axes state based on old vs. new. */
	if (axes != stick_state.oldaxes)
	{
		for( i = 0; i < 16; i++ ) {
			if( ( axes & ( 1 << i ) ) && !( stick_state.oldaxes & ( 1 << i ) ) ) {
				Com_QueueEvent( in_eventTime, SE_KEY, joy_keys[i], qtrue, 0, NULL );
			}

			if( !( axes & ( 1 << i ) ) && ( stick_state.oldaxes & ( 1 << i ) ) ) {
				Com_QueueEvent( in_eventTime, SE_KEY, joy_keys[i], qfalse, 0, NULL );
			}
		}
	}

	/* Save for future generations. */
	stick_state.oldaxes = axes;
}
#endif  // USE_JOYSTICK

#ifdef SDL_INIT_CAMERA
static const char *IN_WebcamPositionName( SDL_CameraPosition pos )
{
	switch ( pos ) {
		case SDL_CAMERA_POSITION_FRONT_FACING: return "front";
		case SDL_CAMERA_POSITION_BACK_FACING: return "back";
		default: return "unknown";
	}
}

static qboolean IN_WebcamEnsureSubsystem( void )
{
	if ( SDL_WasInit( SDL_INIT_CAMERA ) ) {
		return qtrue;
	}
	if ( !SDL_Init( SDL_INIT_CAMERA ) ) {
		Com_Printf( "SDL_Init(SDL_INIT_CAMERA) failed: %s\n", SDL_GetError() );
		return qfalse;
	}
	return qtrue;
}

static void IN_WebcamClose( void )
{
	if ( s_webcam.camera ) {
		SDL_CloseCamera( s_webcam.camera );
	}
	Com_Memset( &s_webcam, 0, sizeof( s_webcam ) );
}

static void IN_WebcamList_f( void )
{
	SDL_CameraID *ids;
	int count = 0;
	int i;

	if ( !IN_WebcamEnsureSubsystem() ) {
		return;
	}

	ids = SDL_GetCameras( &count );
	Com_Printf( "SDL3 webcam devices: %d\n", count );
	for ( i = 0; ids && i < count; i++ ) {
		SDL_CameraSpec **formats = NULL;
		int formatCount = 0;
		int f;
		Com_Printf( "  [%d] id=%u name=%s position=%s\n", i, (unsigned)ids[i],
			SDL_GetCameraName( ids[i] ) ? SDL_GetCameraName( ids[i] ) : "(unknown)",
			IN_WebcamPositionName( SDL_GetCameraPosition( ids[i] ) ) );
		formats = SDL_GetCameraSupportedFormats( ids[i], &formatCount );
		for ( f = 0; formats && f < formatCount && f < 8; f++ ) {
			const SDL_CameraSpec *spec = formats[f];
			Com_Printf( "      %dx%d %s %d/%d fps\n",
				spec->width, spec->height, SDL_GetPixelFormatName( spec->format ),
				spec->framerate_numerator, spec->framerate_denominator );
		}
		if ( formats && formatCount > 8 ) {
			Com_Printf( "      ... %d more format(s)\n", formatCount - 8 );
		}
		if ( formats ) {
			SDL_free( formats );
		}
	}
	if ( ids ) {
		SDL_free( ids );
	}
}

static void IN_WebcamStatus_f( void )
{
	Com_Printf( "SDL3 webcam status:\n" );
	Com_Printf( "  enabled=%d alias=%d poll=%d selected=%d active=%s permission=%d frames=%u\n",
		cl_webcamEnable ? cl_webcamEnable->integer : 0,
		in_webcam ? in_webcam->integer : 0,
		cl_webcamPoll ? cl_webcamPoll->integer : 0,
		cl_webcamDevice ? cl_webcamDevice->integer : 0,
		s_webcam.active ? "yes" : "no", s_webcam.permission, s_webcam.frameCount );
	if ( s_webcam.active ) {
		Com_Printf( "  id=%u format=%s requested=%dx%d@%d actual=%dx%d last=%dx%d timestampNS=%llu\n",
			(unsigned)s_webcam.id,
			s_webcam.hasSpec ? SDL_GetPixelFormatName( s_webcam.spec.format ) : "(pending)",
			cl_webcamWidth ? cl_webcamWidth->integer : 0,
			cl_webcamHeight ? cl_webcamHeight->integer : 0,
			cl_webcamFps ? cl_webcamFps->integer : 0,
			s_webcam.hasSpec ? s_webcam.spec.width : 0,
			s_webcam.hasSpec ? s_webcam.spec.height : 0,
			s_webcam.lastWidth, s_webcam.lastHeight,
			(unsigned long long)s_webcam.lastTimestampNS );
	}
}

static void IN_WebcamStart_f( void )
{
	SDL_CameraID *ids;
	SDL_CameraSpec requested;
	SDL_CameraSpec *specPtr = NULL;
	int count = 0;
	int index;

	if ( ( !in_webcam || !in_webcam->integer ) && ( !cl_webcamEnable || !cl_webcamEnable->integer ) ) {
		Com_Printf( "webcam_start: disabled by in_webcam 0 and cl_webcamEnable 0\n" );
		return;
	}
	if ( !IN_WebcamEnsureSubsystem() ) {
		return;
	}

	ids = SDL_GetCameras( &count );
	if ( !ids || count <= 0 ) {
		Com_Printf( "webcam_start: no SDL3 camera devices found\n" );
		if ( ids ) {
			SDL_free( ids );
		}
		return;
	}

	index = cl_webcamDevice ? cl_webcamDevice->integer : 0;
	if ( index < 0 || index >= count ) {
		index = 0;
	}

	IN_WebcamClose();
	Com_Memset( &requested, 0, sizeof( requested ) );
	if ( cl_webcamWidth && cl_webcamHeight && cl_webcamWidth->integer > 0 && cl_webcamHeight->integer > 0 ) {
		requested.width = cl_webcamWidth->integer;
		requested.height = cl_webcamHeight->integer;
		requested.framerate_numerator = ( cl_webcamFps && cl_webcamFps->integer > 0 ) ? cl_webcamFps->integer : 30;
		requested.framerate_denominator = 1;
		specPtr = &requested;
	}

	s_webcam.camera = SDL_OpenCamera( ids[index], specPtr );
	if ( !s_webcam.camera ) {
		Com_Printf( "webcam_start: SDL_OpenCamera failed for [%d] %s: %s\n",
			index, SDL_GetCameraName( ids[index] ) ? SDL_GetCameraName( ids[index] ) : "(unknown)", SDL_GetError() );
		SDL_free( ids );
		return;
	}

	s_webcam.id = ids[index];
	s_webcam.active = qtrue;
	s_webcam.permission = SDL_GetCameraPermissionState( s_webcam.camera );
	s_webcam.hasSpec = SDL_GetCameraFormat( s_webcam.camera, &s_webcam.spec ) ? qtrue : qfalse;
	Com_Printf( "webcam_start: opened [%d] %s permission=%d\n",
		index, SDL_GetCameraName( ids[index] ) ? SDL_GetCameraName( ids[index] ) : "(unknown)", s_webcam.permission );
	SDL_free( ids );
}

static void IN_WebcamStop_f( void )
{
	IN_WebcamClose();
	Com_Printf( "webcam_stop: closed SDL3 camera\n" );
}

static void IN_WebcamFrame( void )
{
	SDL_Surface *frame;
	Uint64 timestampNS = 0;
	qboolean enabled;

	enabled = ( in_webcam && in_webcam->integer ) || ( cl_webcamEnable && cl_webcamEnable->integer );
	if ( !enabled || !s_webcam.active || !s_webcam.camera || !cl_webcamPoll || !cl_webcamPoll->integer ) {
		return;
	}

	s_webcam.permission = SDL_GetCameraPermissionState( s_webcam.camera );
	frame = SDL_AcquireCameraFrame( s_webcam.camera, &timestampNS );
	if ( !frame ) {
		return;
	}

	s_webcam.frameCount++;
	s_webcam.lastWidth = frame->w;
	s_webcam.lastHeight = frame->h;
	s_webcam.lastFormat = frame->format;
	s_webcam.lastTimestampNS = timestampNS;
	if ( re.WebcamUploadFrame ) {
		SDL_Surface *rgba = frame;

		if ( frame->format != SDL_PIXELFORMAT_RGBA32 ) {
			rgba = SDL_ConvertSurface( frame, SDL_PIXELFORMAT_RGBA32 );
		}
		if ( rgba ) {
			re.WebcamUploadFrame( (const byte *)rgba->pixels, rgba->w, rgba->h );
			if ( rgba != frame ) {
				SDL_DestroySurface( rgba );
			}
		}
	}
	SDL_ReleaseCameraFrame( s_webcam.camera, frame );
}
#endif



#ifdef DEBUG_EVENTS
static const char *eventName( Uint32 event )
{
	static char buf[32];
	Com_sprintf( buf, sizeof( buf ), "EVENT#%u", (unsigned)event );
	return buf;
}
#endif


/*
===============
IN_SyncModifiers
===============
*/
static void IN_SyncModifiers( void ) {
    SDL_Keymod mod = SDL_GetModState();

    keys[K_CTRL].down  = (mod & SDL_KMOD_CTRL)  ? qtrue : qfalse;
    keys[K_SHIFT].down = (mod & SDL_KMOD_SHIFT) ? qtrue : qfalse;
    keys[K_ALT].down   = (mod & SDL_KMOD_ALT)   ? qtrue : qfalse;
}


/*
===============
HandleEvents
===============
*/
//static void IN_ProcessEvents( void )
void HandleEvents( void )
{
	SDL_Event e;
	keyNum_t key = 0;
	static keyNum_t lastKeyDown = 0;

	if ( !SDL_WasInit( SDL_INIT_VIDEO ) )
			return;

	in_eventTime = Sys_Milliseconds();

	IN_SyncModifiers();

	while ( SDL_PollEvent( &e ) )
	{
		switch( e.type )
		{
			case SDL_EVENT_KEY_DOWN:
				if ( e.key.repeat && Key_GetCatcher() == 0 )
					break;
				key = IN_TranslateSDLToQ3Key( e.key.scancode, e.key.key, e.key.mod, qtrue );

				if ( key == K_ENTER && keys[K_ALT].down ) {
					Cvar_SetIntegerValue( "r_fullscreen", glw_state.isFullscreen ? 0 : 1 );
					Cbuf_AddText( "vid_restart\n" );
					break;
				}

				if ( key ) {
					Com_QueueEvent( in_eventTime, SE_KEY, key, qtrue, 0, NULL );

					if ( key == K_BACKSPACE )
						Com_QueueEvent( in_eventTime, SE_CHAR, CTRL('h'), 0, 0, NULL );
					else if ( key == K_ESCAPE )
						Com_QueueEvent( in_eventTime, SE_CHAR, key, 0, 0, NULL );
					else if( keys[K_CTRL].down && key >= 'a' && key <= 'z' )
						Com_QueueEvent( in_eventTime, SE_CHAR, CTRL(key), 0, 0, NULL );
#ifdef MACOS_X
					else if( keys[K_COMMAND].down && key == 'v' )
						Com_QueueEvent( in_eventTime, SE_CHAR, CTRL(key), 0, 0, NULL );
#endif
				}

				lastKeyDown = key;
				break;

			case SDL_EVENT_KEY_UP:
				if( ( key = IN_TranslateSDLToQ3Key( e.key.scancode, e.key.key, e.key.mod, qfalse ) ) )
					Com_QueueEvent( in_eventTime, SE_KEY, key, qfalse, 0, NULL );

				lastKeyDown = 0;
				break;

			case SDL_EVENT_TEXT_INPUT:
				if( lastKeyDown != K_CONSOLE )
				{
					const char *c = e.text.text;
					IN_ClearImeState();

					// Quick and dirty UTF-8 to UTF-32 conversion
					while ( *c )
					{
						int utf32 = 0;

						if( ( *c & 0x80 ) == 0 )
							utf32 = *c++;
						else if( ( *c & 0xE0 ) == 0xC0 ) // 110x xxxx
						{
							utf32 |= ( *c++ & 0x1F ) << 6;
							utf32 |= ( *c++ & 0x3F );
						}
						else if( ( *c & 0xF0 ) == 0xE0 ) // 1110 xxxx
						{
							utf32 |= ( *c++ & 0x0F ) << 12;
							utf32 |= ( *c++ & 0x3F ) << 6;
							utf32 |= ( *c++ & 0x3F );
						}
						else if( ( *c & 0xF8 ) == 0xF0 ) // 1111 0xxx
						{
							utf32 |= ( *c++ & 0x07 ) << 18;
							utf32 |= ( *c++ & 0x3F ) << 12;
							utf32 |= ( *c++ & 0x3F ) << 6;
							utf32 |= ( *c++ & 0x3F );
						}
						else
						{
							Com_DPrintf( "Unrecognised UTF-8 lead byte: 0x%x\n", (unsigned int)*c );
							c++;
						}

						if( utf32 != 0 )
						{
							if ( IN_IsConsoleKey( 0, utf32 ) )
							{
								Com_QueueEvent( in_eventTime, SE_KEY, K_CONSOLE, qtrue, 0, NULL );
								Com_QueueEvent( in_eventTime, SE_KEY, K_CONSOLE, qfalse, 0, NULL );
							}
							else
								Com_QueueEvent( in_eventTime, SE_CHAR, utf32, 0, 0, NULL );
						}
					}
				}
				break;

			case SDL_EVENT_TEXT_EDITING:
				if ( ui_imeComposition ) {
					Cvar_Set( "ui_imeComposition", e.edit.text ? e.edit.text : "" );
				}
				if ( ui_imeCompositionStart ) {
					Cvar_SetValue( "ui_imeCompositionStart", e.edit.start );
				}
				if ( ui_imeCompositionLength ) {
					Cvar_SetValue( "ui_imeCompositionLength", e.edit.length );
				}
				IN_UpdateTextInputArea();
				break;

			case SDL_EVENT_TEXT_EDITING_CANDIDATES:
				if ( ui_imeCandidates ) {
					char buf[MAX_CVAR_VALUE_STRING];
					int i;

					buf[0] = '\0';
					if ( e.edit_candidates.candidates ) {
						for ( i = 0; i < e.edit_candidates.num_candidates; i++ ) {
							if ( i > 0 ) {
								Q_strcat( buf, sizeof( buf ), " | " );
							}
							Q_strcat( buf, sizeof( buf ), e.edit_candidates.candidates[i] ? e.edit_candidates.candidates[i] : "" );
						}
					}
					Cvar_Set( "ui_imeCandidates", buf );
				}
				break;

			case SDL_EVENT_MOUSE_MOTION:
				if( mouseActive || IN_PointerUiMode() )
				{
					int dx, dy;
					if ( mouseActive ) {
						float xrel = e.motion.xrel;
						float yrel = e.motion.yrel;
						s_dbg_raw_xrel = xrel;
						s_dbg_raw_yrel = yrel;
						s_dbg_event_count++;
						/* When relative mode failed, prefer absolute delta vs window
						 * center so warp-to-center fallback still produces look input. */
						if ( !mouseRelativeActive ) {
							const float cx = (float)( glw_state.window_width / 2 );
							const float cy = (float)( glw_state.window_height / 2 );
							xrel = e.motion.x - cx;
							yrel = e.motion.y - cy;
							s_dbg_raw_xrel = xrel;
							s_dbg_raw_yrel = yrel;
						} else if ( xrel == 0.0f && yrel == 0.0f ) {
							break;
						}
						/* Accumulate SDL3 float deltas so sub-pixel HiDPI motion is not lost. */
						mouse_frac_x += xrel;
						mouse_frac_y += yrel;
						dx = (int)mouse_frac_x;
						dy = (int)mouse_frac_y;
						mouse_frac_x -= (float)dx;
						mouse_frac_y -= (float)dy;
						if ( !dx && !dy )
							break;
						s_dbg_post_dx = dx;
						s_dbg_post_dy = dy;
						s_dbg_accum_dx += dx;
						s_dbg_accum_dy += dy;
						if ( in_mouseDebug && in_mouseDebug->integer ) {
							Com_Printf( "mouse raw=%.3f,%.3f post=%d,%d rel=%d grab=%d\n",
								xrel, yrel, dx, dy,
								(int)mouseRelativeActive,
								(int)SDL_GetWindowMouseGrab( SDL_window ) );
						}
					} else {
						/* UI mode, ungrabbed: use absolute position to compute delta. */
						int prev_x = ( last_ui_mouse_x < 0 ) ? 0 : last_ui_mouse_x;
						int prev_y = ( last_ui_mouse_y < 0 ) ? 0 : last_ui_mouse_y;
						last_ui_mouse_x = (int)lroundf( e.motion.x );
						last_ui_mouse_y = (int)lroundf( e.motion.y );
						dx = last_ui_mouse_x - prev_x;
						dy = last_ui_mouse_y - prev_y;
						if ( !dx && !dy )
							break;
					}
					Com_QueueEvent( in_eventTime, SE_MOUSE, dx, dy, 0, NULL );
				}
				break;

			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
				{
					int b;
					switch( e.button.button )
					{
						case SDL_BUTTON_LEFT:   b = K_MOUSE1;     break;
						case SDL_BUTTON_MIDDLE: b = K_MOUSE3;     break;
						case SDL_BUTTON_RIGHT:  b = K_MOUSE2;     break;
						case SDL_BUTTON_X1:     b = K_MOUSE4;     break;
						case SDL_BUTTON_X2:     b = K_MOUSE5;     break;
						default:                b = K_AUX1 + ( e.button.button - SDL_BUTTON_X2 + 1 ) % 16; break;
					}
					Com_QueueEvent( in_eventTime, SE_KEY, b,
						( e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? qtrue : qfalse ), 0, NULL );
				}
				break;

			case SDL_EVENT_MOUSE_WHEEL:
				if( e.wheel.y > 0 )
				{
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELUP, qtrue, 0, NULL );
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELUP, qfalse, 0, NULL );
				}
				else if( e.wheel.y < 0 )
				{
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELDOWN, qtrue, 0, NULL );
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL );
				}
				break;

#ifdef USE_JOYSTICK
			case SDL_EVENT_GAMEPAD_ADDED:
			case SDL_EVENT_GAMEPAD_REMOVED:
				if ( in_joystick->integer )
					IN_InitJoystick();
				break;
#endif

#ifdef SDL_INIT_CAMERA
			case SDL_EVENT_CAMERA_DEVICE_ADDED:
			case SDL_EVENT_CAMERA_DEVICE_REMOVED:
				if ( ( cl_webcamEnable && cl_webcamEnable->integer ) || ( in_webcam && in_webcam->integer ) )
					Com_Printf( "SDL3 webcam device %s: id=%u\n",
						e.type == SDL_EVENT_CAMERA_DEVICE_ADDED ? "added" : "removed",
						(unsigned)e.cdevice.which );
				break;
			case SDL_EVENT_CAMERA_DEVICE_APPROVED:
			case SDL_EVENT_CAMERA_DEVICE_DENIED:
				if ( s_webcam.active && s_webcam.id == e.cdevice.which ) {
					s_webcam.permission = ( e.type == SDL_EVENT_CAMERA_DEVICE_APPROVED ) ? 1 : -1;
				}
				Com_Printf( "SDL3 webcam permission %s: id=%u\n",
					e.type == SDL_EVENT_CAMERA_DEVICE_APPROVED ? "approved" : "denied",
					(unsigned)e.cdevice.which );
				break;
#endif

			case SDL_EVENT_QUIT:
				Cbuf_ExecuteText( EXEC_NOW, "quit Closed window\n" );
				break;

			case SDL_EVENT_WINDOW_MOVED:
#ifdef DEBUG_EVENTS
				Com_Printf( "%4i %s\n", (int)e.window.timestamp, eventName( e.type ) );
#endif
				if ( gw_active && !gw_minimized && !glw_state.isFullscreen ) {
					Cvar_SetIntegerValue( "vid_xpos", e.window.data1 );
					Cvar_SetIntegerValue( "vid_ypos", e.window.data2 );
				}
				break;
			case SDL_EVENT_WINDOW_RESIZED:
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
				IN_UpdateDisplayScaleCvars();
				IN_UpdateTextInputArea();
				/* Keep Vulkan swapchain/FBO in sync with the SDL window.
				 * RESIZED delivers logical size; PIXEL_SIZE_CHANGED delivers pixels.
				 * Always query both live sizes — never compare event data against the
				 * wrong coordinate space (HiDPI caused endless vid_restart). */
				if ( !glw_state.isFullscreen && SDL_window ) {
					int logicalW = 0, logicalH = 0;
					int pixelW = 0, pixelH = 0;
					static int s_lastPixelW, s_lastPixelH;

					SDL_GetWindowSize( SDL_window, &logicalW, &logicalH );
					SDL_GetWindowSizeInPixels( SDL_window, &pixelW, &pixelH );
					if ( logicalW > 0 && logicalH > 0 ) {
						glw_state.window_width = logicalW;
						glw_state.window_height = logicalH;
					}
					if ( pixelW > 0 && pixelH > 0 ) {
						glw_state.pixel_width = pixelW;
						glw_state.pixel_height = pixelH;
						if ( pixelW != s_lastPixelW || pixelH != s_lastPixelH ) {
							s_lastPixelW = pixelW;
							s_lastPixelH = pixelH;
							if ( pixelW != cls.glconfig.vidWidth || pixelH != cls.glconfig.vidHeight ) {
								Cvar_Set( "r_mode", "-1" );
								Cvar_SetIntegerValue( "r_customWidth", pixelW );
								Cvar_SetIntegerValue( "r_customHeight", pixelH );
								Cbuf_AddText( "vid_restart\n" );
							}
						}
					}
				}
				break;
			case SDL_EVENT_WINDOW_HIDDEN:
			case SDL_EVENT_WINDOW_MINIMIZED:
				gw_active = qfalse; gw_minimized = qtrue;
				break;
			case SDL_EVENT_WINDOW_SHOWN:
			case SDL_EVENT_WINDOW_RESTORED:
			case SDL_EVENT_WINDOW_MAXIMIZED:
				gw_minimized = qfalse;
				IN_NotifyWindowRestored();
				if ( re.NotifyWindowRestored ) {
					re.NotifyWindowRestored( "window_restored" );
				}
				break;
			case SDL_EVENT_WINDOW_FOCUS_LOST:
				lastKeyDown = 0; Key_ClearStates(); IN_SyncModifiers();
				gw_active = qfalse;
				mouse_focus = qfalse;
				break;
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
				lastKeyDown = 0; Key_ClearStates(); IN_SyncModifiers();
				gw_active = qtrue;
				gw_minimized = qfalse;
				/* Treat keyboard focus as sufficient for relative mouse; MOUSE_ENTER
				 * may never fire after vid_restart / fullscreen transitions. */
				mouse_focus = qtrue;
				if ( re.SetColorMappings ) {
					re.SetColorMappings();
				}
				IN_NotifyWindowRestored();
				if ( re.NotifyWindowRestored ) {
					re.NotifyWindowRestored( "focus_gained" );
				}
				break;
			case SDL_EVENT_WINDOW_MOUSE_ENTER:
				mouse_focus = qtrue;
				break;
			case SDL_EVENT_WINDOW_MOUSE_LEAVE:
				/* Do not drop relative mouse on leave while the window still has
				 * keyboard focus — Wayland/X11 often emit leave during grab. */
				if ( !gw_active )
					mouse_focus = qfalse;
				break;
		default:
				break;
		}
	}
}


/*
===============
IN_NotifyWindowRestored

Re-assert relative mouse after alt-tab / un-minimize / presentation restore.
Forces ActivateMouse to re-run grab + relative mode even if already "active".
===============
*/
void IN_NotifyWindowRestored( void )
{
	if ( !mouseAvailable ) {
		return;
	}

	IN_GobbleMouseEvents();
	mouse_frac_x = 0.0f;
	mouse_frac_y = 0.0f;

	/* Force the activate path even when mouseActive was sticky across focus loss. */
	mouseActive = qfalse;

	if ( !gw_active || ( in_nograb && in_nograb->integer ) ) {
		return;
	}
	if ( Key_GetCatcher() & ( KEYCATCH_CONSOLE | KEYCATCH_UI ) ) {
		return;
	}

	mouse_focus = qtrue;
	IN_ActivateMouse();

	if ( mouseActive && mouseRelativeWanted && SDL_window &&
		!SDL_GetWindowRelativeMouseMode( SDL_window ) ) {
		IN_SetRelativeMouse( qtrue );
	}

	Com_DPrintf( "[input] window restored: relative=%s grab=%s\n",
		mouseRelativeActive ? "yes" : "no",
		mouseActive ? "yes" : "no" );
}

/*
===============
IN_Minimize

Minimize the game so that user is back at the desktop
===============
*/
static void IN_Minimize( void )
{
	SDL_MinimizeWindow( SDL_window );
}


/*
===============
IN_Frame
===============
*/
void IN_Frame( void )
{
#ifdef USE_JOYSTICK
	IN_JoyMove();
#endif
#ifdef SDL_INIT_CAMERA
	IN_WebcamFrame();
#endif

	IN_UpdateDisplayScaleCvars();
	IN_UpdateTextInputArea();
	if ( !( Key_GetCatcher() & ( KEYCATCH_CONSOLE | KEYCATCH_UI | KEYCATCH_MESSAGE ) ) ) {
		IN_ClearImeState();
	}

	if ( Key_GetCatcher() & ( KEYCATCH_CONSOLE | KEYCATCH_UI ) || CL_RpMenuActive() ) {
		/* Release mouse and show cursor when console, Q3 menu, or HavenRP City Menu is open.
		 * In fullscreen single-monitor, console may keep grab for consistency;
		 * menu always releases so user can click UI elements. */
		if ( !glw_state.isFullscreen || glw_state.monitorCount > 1 ||
		     IN_PointerUiMode() ) {
			IN_DeactivateMouse();
			return;
		}
	}

	if ( !gw_active || in_nograb->integer ) {
		IN_DeactivateMouse();
		return;
	}

	/* Keyboard focus is enough; mouse_focus is advisory after leave events. */
	if ( !mouse_focus ) {
		mouse_focus = qtrue;
	}

	IN_ActivateMouse();

	/* Re-assert relative mode if SDL dropped it (Wayland constraint loss). */
	if ( mouseActive && mouseRelativeWanted && !SDL_GetWindowRelativeMouseMode( SDL_window ) ) {
		IN_SetRelativeMouse( qtrue );
	}

	/* If relative mode is still unavailable, keep the cursor centered so absolute
	 * motion can be converted to deltas without hitting the window edge. Do not
	 * warp when relative mode is live — that injects spurious motion on Wayland. */
	if ( mouseActive && mouseRelativeWanted && !mouseRelativeActive &&
	     !( in_nograb && in_nograb->integer ) ) {
		IN_WarpToWindowCenter();
	}
}


/*
===============
IN_InputStatus_f
===============
*/
static void IN_InputStatus_f( void )
{
	const char *drv = SDL_GetCurrentVideoDriver();
	const int catcher = Key_GetCatcher();
	const qboolean relLive = ( SDL_window && SDL_GetWindowRelativeMouseMode( SDL_window ) ) ? qtrue : qfalse;
	const qboolean grabLive = ( SDL_window && SDL_GetWindowMouseGrab( SDL_window ) ) ? qtrue : qfalse;
	const qboolean cursorVis = SDL_CursorVisible() ? qtrue : qfalse;
	const float sens = Cvar_VariableValue( "sensitivity" );
	const float mPitch = Cvar_VariableValue( "m_pitch" );
	const float mYaw = Cvar_VariableValue( "m_yaw" );
	const int freelook = (int)Cvar_VariableValue( "cl_freelook" );

	Com_Printf( "=== input_status ===\n" );
	Com_Printf( "  backend:          SDL3 / %s\n", drv ? drv : "(none)" );
	Com_Printf( "  window focus:     %s\n", gw_active ? "yes" : "no" );
	Com_Printf( "  mouse focus:      %s\n", mouse_focus ? "yes" : "no" );
	Com_Printf( "  minimized:        %s\n", gw_minimized ? "yes" : "no" );
	Com_Printf( "  restore hook:     IN_NotifyWindowRestored + re.NotifyWindowRestored\n" );
	Com_Printf( "  mouse available:  %s (in_mouse=%d)\n", mouseAvailable ? "yes" : "no",
		in_mouse ? in_mouse->integer : 0 );
	Com_Printf( "  mouse active:     %s\n", mouseActive ? "yes" : "no" );
	Com_Printf( "  relative wanted:  %s\n", mouseRelativeWanted ? "yes" : "no" );
	Com_Printf( "  relative active:  %s (SDL live=%s)\n",
		mouseRelativeActive ? "yes" : "no", relLive ? "yes" : "no" );
	Com_Printf( "  relative fails:   %d\n", s_dbg_relative_fail );
	Com_Printf( "  raw input:        %s (SDL relative mode)\n",
		( in_mouse && in_mouse->integer == 1 ) ? "requested" : "off" );
	Com_Printf( "  grab state:       %s (SDL live=%s)\n",
		( mouseActive && !( in_nograb && in_nograb->integer ) ) ? "wanted" : "released",
		grabLive ? "yes" : "no" );
	Com_Printf( "  cursor visible:   %s\n", cursorVis ? "yes" : "no" );
	Com_Printf( "  UI capture:       %s\n", ( catcher & KEYCATCH_UI ) ? "yes" : "no" );
	Com_Printf( "  console capture:  %s\n", ( catcher & KEYCATCH_CONSOLE ) ? "yes" : "no" );
	Com_Printf( "  cgame capture:    %s\n", ( catcher & KEYCATCH_CGAME ) ? "yes" : "no" );
	Com_Printf( "  in_nograb:        %d\n", in_nograb ? in_nograb->integer : 0 );
	Com_Printf( "  fullscreen:       %s\n", glw_state.isFullscreen ? "yes" : "no" );
	Com_Printf( "  window logical:   %dx%d\n", glw_state.window_width, glw_state.window_height );
	Com_Printf( "  window pixels:    %dx%d\n", glw_state.pixel_width, glw_state.pixel_height );
	Com_Printf( "  latest raw delta: %.3f, %.3f\n", s_dbg_raw_xrel, s_dbg_raw_yrel );
	Com_Printf( "  latest post delta:%d, %d\n", s_dbg_post_dx, s_dbg_post_dy );
	Com_Printf( "  accum delta:      %d, %d (events=%d)\n",
		s_dbg_accum_dx, s_dbg_accum_dy, s_dbg_event_count );
	Com_Printf( "  sensitivity:      %.3f\n", sens );
	Com_Printf( "  m_yaw / m_pitch:  %.4f / %.4f\n", mYaw, mPitch );
	Com_Printf( "  cl_freelook:      %d\n", freelook );
	Com_Printf( "  yaw/pitch scale:  sens*m_yaw=%.5f  sens*m_pitch=%.5f\n",
		sens * mYaw, sens * mPitch );

	/* Reset session accumulators so repeated input_status shows fresh motion. */
	s_dbg_accum_dx = 0;
	s_dbg_accum_dy = 0;
	s_dbg_event_count = 0;
}


/*
===============
IN_Restart
===============
*/
static void IN_Restart( void )
{
#ifdef USE_JOYSTICK
	IN_ShutdownJoystick();
#endif
	IN_Shutdown();
	IN_Init();
}


/*
===============
IN_Init
===============
*/
void IN_Init( void )
{
	if ( !SDL_WasInit( SDL_INIT_VIDEO ) )
	{
		Com_Error( ERR_FATAL, "IN_Init called before SDL_Init( SDL_INIT_VIDEO )" );
		return;
	}

	Com_DPrintf( "\n------- Input Initialization -------\n" );

	in_keyboardDebug = Cvar_Get( "in_keyboardDebug", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( in_keyboardDebug, "Print keyboard debug info." );
	in_forceCharset = Cvar_Get( "in_forceCharset", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( in_forceCharset, "Try to translate non-ASCII chars in keyboard input or force EN/US keyboard layout." );

	// mouse variables
	in_mouse = Cvar_Get( "in_mouse", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( in_mouse, "-1", "1", CV_INTEGER );
	Cvar_SetDescription( in_mouse,
		"Mouse data input source:\n" \
		"  0 - disable mouse input\n" \
		"  1 - di/raw mouse\n" \
		" -1 - win32 mouse" );
	in_mouseDebug = Cvar_Get( "in_mouseDebug", "0", CVAR_TEMP );
	Cvar_CheckRange( in_mouseDebug, "0", "1", CV_INTEGER );
	Cvar_SetDescription( in_mouseDebug, "Print per-motion raw/post mouse deltas and relative/grab state." );

#ifdef USE_JOYSTICK
	in_joystick = Cvar_Get( "in_joystick", "0", CVAR_ARCHIVE|CVAR_LATCH );
	Cvar_SetDescription( in_joystick, "Whether or not joystick support is on." );
	in_joystickThreshold = Cvar_Get( "joy_threshold", "0.15", CVAR_ARCHIVE );
	Cvar_SetDescription( in_joystickThreshold, "Threshold of joystick moving distance." );
	in_gamepadEvents = Cvar_Get( "in_gamepadEvents", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( in_gamepadEvents, "0", "1", CV_INTEGER );
	Cvar_SetDescription( in_gamepadEvents, "Enable SDL3 gamepad hotplug/button/axis events. State polling still runs when joystick support is active." );
	in_gamepadMappingFile = Cvar_Get( "in_gamepadMappingFile", "", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( in_gamepadMappingFile, "Optional SDL3 gamepad mapping database path loaded at joystick init (for example gamecontrollerdb.txt)." );
	in_gamepadRumble = Cvar_Get( "in_gamepadRumble", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( in_gamepadRumble, "0", "1", CV_INTEGER );
	Cvar_SetDescription( in_gamepadRumble, "Allow gamepad_rumble and gamepad_trigger_rumble commands." );

	j_pitch =        Cvar_Get( "j_pitch",        "0.022", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( j_pitch, "Joystick pitch rotation speed/direction." );
	j_yaw =          Cvar_Get( "j_yaw",          "-0.022", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( j_yaw, "Joystick yaw rotation speed/direction." );
	j_forward =      Cvar_Get( "j_forward",      "-0.25", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( j_forward, "Joystick forward movement speed/direction." );
	j_side =         Cvar_Get( "j_side",         "0.25", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( j_side, "Joystick side movement speed/direction." );
	j_up =           Cvar_Get( "j_up",           "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( j_up, "Joystick up movement speed/direction." );

	j_pitch_axis =   Cvar_Get( "j_pitch_axis",   "3", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( j_pitch_axis,   "0", va("%i",MAX_JOYSTICK_AXIS-1), CV_INTEGER );
	Cvar_SetDescription( j_pitch_axis, "Selects which joystick axis controls pitch." );
	j_yaw_axis =     Cvar_Get( "j_yaw_axis",     "2", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( j_yaw_axis,     "0", va("%i",MAX_JOYSTICK_AXIS-1), CV_INTEGER );
	Cvar_SetDescription( j_yaw_axis, "Selects which joystick axis controls yaw." );
	j_forward_axis = Cvar_Get( "j_forward_axis", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( j_forward_axis, "0", va("%i",MAX_JOYSTICK_AXIS-1), CV_INTEGER );
	Cvar_SetDescription( j_forward_axis, "Selects which joystick axis controls forward/back." );
	j_side_axis =    Cvar_Get( "j_side_axis",    "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( j_side_axis,    "0", va("%i",MAX_JOYSTICK_AXIS-1), CV_INTEGER );
	Cvar_SetDescription( j_side_axis, "Selects which joystick axis controls left/right." );
	j_up_axis =      Cvar_Get( "j_up_axis",      "4", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( j_up_axis,      "0", va("%i",MAX_JOYSTICK_AXIS-1), CV_INTEGER );
	Cvar_SetDescription( j_up_axis, "Selects which joystick axis controls up/down." );
#endif

#ifdef SDL_INIT_CAMERA
	in_webcam = Cvar_Get( "in_webcam", "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( in_webcam, "0", "1", CV_INTEGER );
	Cvar_SetDescription( in_webcam, "Upload SDL3 webcam frames to *webcam when enabled. Alias-friendly opt-in for RP HUD preview work." );
	cl_webcamEnable = Cvar_Get( "cl_webcamEnable", "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_webcamEnable, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_webcamEnable, "Allow SDL3 webcam/camera device access. Use webcam_start after enabling." );
	cl_webcamDevice = Cvar_Get( "cl_webcamDevice", "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_webcamDevice, "0", "32", CV_INTEGER );
	Cvar_SetDescription( cl_webcamDevice, "SDL3 webcam device index selected by webcam_start." );
	cl_webcamWidth = Cvar_Get( "cl_webcamWidth", "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_webcamWidth, "0", "7680", CV_INTEGER );
	Cvar_SetDescription( cl_webcamWidth, "Requested webcam width. 0 lets SDL choose the camera default." );
	cl_webcamHeight = Cvar_Get( "cl_webcamHeight", "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_webcamHeight, "0", "4320", CV_INTEGER );
	Cvar_SetDescription( cl_webcamHeight, "Requested webcam height. 0 lets SDL choose the camera default." );
	cl_webcamFps = Cvar_Get( "cl_webcamFps", "30", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_webcamFps, "1", "240", CV_INTEGER );
	Cvar_SetDescription( cl_webcamFps, "Requested webcam frame rate when width/height are set." );
	cl_webcamPoll = Cvar_Get( "cl_webcamPoll", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_webcamPoll, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_webcamPoll, "Poll and immediately release SDL3 webcam frames, updating webcam_status counters." );
#endif

	r_displayScale = Cvar_Get( "r_displayScale", "1", CVAR_TEMP );
	Cvar_SetDescription( r_displayScale, "SDL3-reported window display/content scale factor." );
	ui_displayScale = Cvar_Get( "ui_displayScale", "1", CVAR_TEMP );
	Cvar_SetDescription( ui_displayScale, "Copy of the SDL3 window display/content scale factor for UI/cgame consumption." );
	ui_fileDialogResult = Cvar_Get( "ui_fileDialogResult", "", CVAR_TEMP );
	Cvar_SetDescription( ui_fileDialogResult, "Last path returned by SDL3 filedialog_open/filedialog_save." );
	ui_fileDialogStatus = Cvar_Get( "ui_fileDialogStatus", "", CVAR_TEMP );
	Cvar_SetDescription( ui_fileDialogStatus, "Status for SDL3 file dialogs: pending, selected, cancelled, or error." );
	ui_fileDialogFilter = Cvar_Get( "ui_fileDialogFilter", "-1", CVAR_TEMP );
	Cvar_SetDescription( ui_fileDialogFilter, "Index of the selected SDL3 file dialog filter, or -1 when unavailable." );
	ui_imeComposition = Cvar_Get( "ui_imeComposition", "", CVAR_TEMP );
	Cvar_SetDescription( ui_imeComposition, "Current SDL3 IME composition string." );
	ui_imeCompositionStart = Cvar_Get( "ui_imeCompositionStart", "-1", CVAR_TEMP );
	Cvar_SetDescription( ui_imeCompositionStart, "SDL3 IME composition selection start in UTF-8 characters." );
	ui_imeCompositionLength = Cvar_Get( "ui_imeCompositionLength", "-1", CVAR_TEMP );
	Cvar_SetDescription( ui_imeCompositionLength, "SDL3 IME composition selection length in UTF-8 characters." );
	ui_imeCandidates = Cvar_Get( "ui_imeCandidates", "", CVAR_TEMP );
	Cvar_SetDescription( ui_imeCandidates, "Flattened SDL3 IME candidate list for scripting/UI consumers." );

	// ~ and `, as keys and characters
	cl_consoleKeys = Cvar_Get( "cl_consoleKeys", "~ ` 0x7e 0x60", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_consoleKeys, "Space delimited list of key names or characters that toggle the console." );

	mouseAvailable = ( in_mouse->value != 0 ) ? qtrue : qfalse;

	if ( SDL_window ) SDL_StartTextInput( SDL_window );
	IN_UpdateDisplayScaleCvars();
	IN_UpdateTextInputArea();
	IN_ClearImeState();

	//IN_DeactivateMouse();

#ifdef USE_JOYSTICK
	IN_InitJoystick();
#endif

	Cmd_AddCommand( "minimize", IN_Minimize );
	Cmd_AddCommand( "in_restart", IN_Restart );
	Cmd_AddCommand( "input_status", IN_InputStatus_f );
#ifdef USE_JOYSTICK
	Cmd_AddCommand( "gamepad_status", IN_GamepadStatus_f );
	Cmd_AddCommand( "gamepad_load_mappings", IN_GamepadLoadMappings_f );
	Cmd_AddCommand( "gamepad_rumble", IN_GamepadRumble_f );
	Cmd_AddCommand( "gamepad_trigger_rumble", IN_GamepadTriggerRumble_f );
	Cmd_AddCommand( "gamepad_led", IN_GamepadLed_f );
#endif
	Cmd_AddCommand( "filedialog_open", IN_FileDialogOpen_f );
	Cmd_AddCommand( "filedialog_save", IN_FileDialogSave_f );
#ifdef SDL_INIT_CAMERA
	Cmd_AddCommand( "webcam_list", IN_WebcamList_f );
	Cmd_AddCommand( "webcam_status", IN_WebcamStatus_f );
	Cmd_AddCommand( "webcam_start", IN_WebcamStart_f );
	Cmd_AddCommand( "webcam_stop", IN_WebcamStop_f );
	if ( ( in_webcam && in_webcam->integer ) || cl_webcamEnable->integer ) {
		IN_WebcamStart_f();
	}
#endif

	Com_DPrintf( "------------------------------------\n" );
}


/*
===============
IN_Shutdown
===============
*/
void IN_Shutdown( void )
{
	if ( SDL_window ) SDL_StopTextInput( SDL_window );

	IN_DeactivateMouse();

	mouseAvailable = qfalse;

#ifdef USE_JOYSTICK
	IN_ShutdownJoystick();
#endif
#ifdef SDL_INIT_CAMERA
	IN_WebcamClose();
	if ( SDL_WasInit( SDL_INIT_CAMERA ) ) {
		SDL_QuitSubSystem( SDL_INIT_CAMERA );
	}
#endif

	Cmd_RemoveCommand( "minimize" );
	Cmd_RemoveCommand( "in_restart" );
	Cmd_RemoveCommand( "input_status" );
#ifdef USE_JOYSTICK
	Cmd_RemoveCommand( "gamepad_status" );
	Cmd_RemoveCommand( "gamepad_load_mappings" );
	Cmd_RemoveCommand( "gamepad_rumble" );
	Cmd_RemoveCommand( "gamepad_trigger_rumble" );
	Cmd_RemoveCommand( "gamepad_led" );
#endif
	Cmd_RemoveCommand( "filedialog_open" );
	Cmd_RemoveCommand( "filedialog_save" );
#ifdef SDL_INIT_CAMERA
	Cmd_RemoveCommand( "webcam_list" );
	Cmd_RemoveCommand( "webcam_status" );
	Cmd_RemoveCommand( "webcam_start" );
	Cmd_RemoveCommand( "webcam_stop" );
#endif
}
