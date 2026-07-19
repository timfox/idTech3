/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.
*/

#define _GNU_SOURCE

/*
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

#include "../../client/client.h"
#include "../../renderers/common/tr_public.h"
#include "sdl_glw.h"
#include "sdl_icon.h"

#if defined(_WIN32) && defined(_MSC_VER)
#include <windows.h>
#else
#include <stdlib.h>
#endif
#include <string.h>

#ifdef USE_VULKAN_API
#	include <SDL3/SDL_vulkan.h>
#endif

void GLimp_EndFrame( void );
void GLW_HideFullscreenWindow( void );

typedef enum {
	RSERR_OK,
	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,
	RSERR_FATAL_ERROR,
	RSERR_UNKNOWN
} rserr_t;

glwstate_t glw_state;

SDL_Window *SDL_window = NULL;
#ifdef USE_VULKAN_API
static PFN_vkGetInstanceProcAddr qvkGetInstanceProcAddr;
#endif

cvar_t *r_stereoEnabled;
cvar_t *in_nograb;

/*
===============
GLimp_Shutdown
===============
*/
void GLimp_Shutdown( qboolean unloadDLL )
{
	const char* drv = SDL_GetCurrentVideoDriver();

	IN_Shutdown();

	if ( glw_state.isFullscreen ) {
		if ( drv && strcmp( drv, "x11" ) == 0 ) {
			SDL_WarpMouseGlobal( (float)( glw_state.desktop_width / 2 ), (float)( glw_state.desktop_height / 2 ) );
		} else {
			SDL_ShowCursor();
		}
	}

	if ( SDL_window ) {
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}

	if ( unloadDLL )
		SDL_QuitSubSystem( SDL_INIT_VIDEO );
}


/*
===============
GLimp_Minimize

Minimize the game so that user is back at the desktop
===============
*/
void GLimp_Minimize( void )
{
	SDL_MinimizeWindow( SDL_window );
}


/*
===============
GLimp_LogComment
===============
Writes renderer debug comments to the log file when glw_state.log_fp is set.
Platform function used by the Vulkan renderer.
*/
void GLimp_LogComment( const char *comment )
{
	if ( glw_state.log_fp )
	{
		fprintf( glw_state.log_fp, "%s", comment );
	}
}


static SDL_DisplayID FindNearestDisplay( int *x, int *y, int w, int h )
{
	const int cx = *x + w / 2;
	const int cy = *y + h / 2;
	int i, index, numDisplays;
	SDL_DisplayID *ids;
	SDL_DisplayID best = 0;
	SDL_Rect *list, *m;

	index = -1;

	ids = SDL_GetDisplays( &numDisplays );
	if ( !ids || numDisplays <= 0 )
		return 0;

	glw_state.monitorCount = numDisplays;

	list = Z_Malloc( numDisplays * sizeof( list[0] ) );

	for ( i = 0; i < numDisplays; i++ )
	{
		SDL_GetDisplayBounds( ids[i], list + i );
	}

	for ( i = 0; i < numDisplays; i++ )
	{
		m = list + i;
		if ( cx >= m->x && cx < (m->x + m->w) && cy >= m->y && cy < (m->y + m->h) )
		{
			index = i;
			break;
		}
	}

	if ( index == -1 )
	{
		unsigned long nearest, dist;
		int dx, dy;
		nearest = ~0UL;
		for ( i = 0; i < numDisplays; i++ )
		{
			m = list + i;
			dx = (m->x + m->w/2) - cx;
			dy = (m->y + m->h/2) - cy;
			dist = ( dx * dx ) + ( dy * dy );
			if ( dist < nearest )
			{
				nearest = dist;
				index = i;
			}
		}
	}

	if ( index >= 0 )
	{
		m = list + index;
		if ( *x < m->x )
			*x = m->x;

		if ( *y < m->y )
			*y = m->y;

		best = ids[index];
	}

	SDL_free( ids );
	Z_Free( list );

	return best;
}


static SDL_HitTestResult SDL_HitTestFunc( SDL_Window *win, const SDL_Point *area, void *data )
{
	(void)win;
	(void)area;
	(void)data;

	if ( Key_GetCatcher() & KEYCATCH_CONSOLE && keys[ K_ALT ].down )
		return SDL_HITTEST_DRAGGABLE;

	return SDL_HITTEST_NORMAL;
}


/*
===============
GLimp_SetMode
===============
*/
static int GLW_SetMode( int mode, const char *modeFS, qboolean fullscreen )
{
	glconfig_t *config = glw_state.config;
	int colorBits, depthBits, stencilBits;
	int i;
	const SDL_DisplayMode *desktopMode;
	SDL_DisplayID display = 0;
	int x;
	int y;
	Uint64 flags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN;

	/* Wayland/libdecor needs RESIZABLE for a real window chrome and sane
	 * configure events; without it the window is fixed-size and presentation
	 * can stay black after mode fallbacks. */
	if ( !fullscreen )
		flags |= SDL_WINDOW_RESIZABLE;

	Com_Printf( "Initializing Vulkan display\n" );

	// If a window exists, note its display
	if ( SDL_window != NULL )
	{
		display = SDL_GetDisplayForWindow( SDL_window );
		if ( !display )
		{
			Com_DPrintf( "SDL_GetDisplayForWindow() failed: %s\n", SDL_GetError() );
		}
	}
	else
	{
		x = vid_xpos->integer;
		y = vid_ypos->integer;

		// find out to which display our window belongs to
		// according to previously stored id_xpos and id_ypos coordinates
		display = FindNearestDisplay( &x, &y, 640, 480 );
	}

	desktopMode = display ? SDL_GetDesktopDisplayMode( display ) : NULL;
	if ( desktopMode )
	{
		glw_state.desktop_width = desktopMode->w;
		glw_state.desktop_height = desktopMode->h;
	}
	else
	{
		glw_state.desktop_width = 640;
		glw_state.desktop_height = 480;
	}

	config->isFullscreen = fullscreen;
	glw_state.isFullscreen = fullscreen;

	Com_Printf( "...setting mode %d:", mode );

	if ( !CL_GetModeInfo( &config->vidWidth, &config->vidHeight, &config->windowAspect, mode, modeFS, glw_state.desktop_width, glw_state.desktop_height, fullscreen ) )
	{
		Com_Printf( " invalid mode\n" );
		return RSERR_INVALID_MODE;
	}

	Com_Printf( " %d %d\n", config->vidWidth, config->vidHeight );

	// Destroy existing state if it exists
	if ( SDL_window != NULL )
	{
		SDL_GetWindowPosition( SDL_window, &x, &y );
		Com_DPrintf( "Existing window at %dx%d before being destroyed\n", x, y );
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}

	gw_active = qfalse;
	gw_minimized = qtrue;

	if ( !fullscreen && r_noborder->integer )
	{
		flags |= SDL_WINDOW_BORDERLESS;
	}

	colorBits = r_colorbits->value;

	if ( colorBits == 0 || colorBits > 24 )
		colorBits = 24;

	if ( cl_depthbits->integer == 0 )
	{
		// implicitly assume Z-buffer depth == desktop color depth
		if ( colorBits > 16 )
			depthBits = 24;
		else
			depthBits = 16;
	}
	else
		depthBits = cl_depthbits->integer;

	stencilBits = cl_stencilbits->integer;

	// do not allow stencil if Z-buffer depth likely won't contain it
	if ( depthBits < 24 )
		stencilBits = 0;

	SDL_GL_ResetAttributes();  /* avoid legacy GL visual hints on X11/aarch64 */
	SDL_SetHint( SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0" );

	for ( i = 0; i < 16; i++ )
	{
		int testColorBits, testDepthBits, testStencilBits;
		SDL_PropertiesID props;

		// 0 - default
		// 1 - minus colorBits
		// 2 - minus depthBits
		// 3 - minus stencil
		if ((i % 4) == 0 && i)
		{
			// one pass, reduce
			switch (i / 4)
			{
				case 2 :
					if (colorBits == 24)
						colorBits = 16;
					break;
				case 1 :
					if (depthBits == 24)
						depthBits = 16;
					else if (depthBits == 16)
						depthBits = 8;
					__attribute__((fallthrough));
				case 3 :
					if (stencilBits == 24)
						stencilBits = 16;
					else if (stencilBits == 16)
						stencilBits = 8;
			}
		}

		testColorBits = colorBits;
		testDepthBits = depthBits;
		testStencilBits = stencilBits;

		if ((i % 4) == 3)
		{ // reduce colorBits
			if (testColorBits == 24)
				testColorBits = 16;
		}

		if ((i % 4) == 2)
		{ // reduce depthBits
			if (testDepthBits == 24)
				testDepthBits = 16;
		}

		if ((i % 4) == 1)
		{ // reduce stencilBits
			if (testStencilBits == 8)
				testStencilBits = 0;
		}

		props = SDL_CreateProperties();
		SDL_SetStringProperty( props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, cl_title );
		SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_X_NUMBER, x );
		SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, y );
		SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, config->vidWidth );
		SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, config->vidHeight );
		SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, (Sint64)flags );
		SDL_ClearError();
		SDL_window = SDL_CreateWindowWithProperties( props );
		SDL_DestroyProperties( props );

		if ( SDL_window == NULL )
		{
			const char *sdl_err = SDL_GetError();
			/* Vulkan: same error every iteration; print once to avoid spam */
			if ( i == 0 )
				Com_Printf( "[VK] SDL_CreateWindowWithProperties failed: %s\n", ( sdl_err && sdl_err[0] ) ? sdl_err : "(no SDL error)" );
			continue;
		}

		if ( fullscreen )
		{
#ifdef MACOS_X
			/* Desktop fullscreen: no exclusive mode pointer */
			if ( !SDL_SetWindowFullscreenMode( SDL_window, NULL ) )
			{
				Com_DPrintf( "SDL_SetWindowFullscreenMode(NULL) failed: %s\n", SDL_GetError() );
				SDL_DestroyWindow( SDL_window );
				SDL_window = NULL;
				continue;
			}
			if ( !SDL_SetWindowFullscreen( SDL_window, true ) )
			{
				Com_DPrintf( "SDL_SetWindowFullscreen failed: %s\n", SDL_GetError() );
				SDL_DestroyWindow( SDL_window );
				SDL_window = NULL;
				continue;
			}
#else
			{
				SDL_DisplayMode displayMode;
				const SDL_DisplayMode *got;
				SDL_DisplayID winDisplay = SDL_GetDisplayForWindow( SDL_window );
				const SDL_DisplayMode *desk = winDisplay ? SDL_GetDesktopDisplayMode( winDisplay ) : NULL;

				if ( !desk )
				{
					SDL_DestroyWindow( SDL_window );
					SDL_window = NULL;
					continue;
				}

				displayMode = *desk;
				switch ( testColorBits )
				{
					case 16: displayMode.format = SDL_PIXELFORMAT_RGB565; break;
					case 24: displayMode.format = SDL_PIXELFORMAT_RGB24;  break;
					default: Com_DPrintf( "testColorBits is %d, can't fullscreen\n", testColorBits );
						SDL_DestroyWindow( SDL_window );
						SDL_window = NULL;
						continue;
				}

				displayMode.w = config->vidWidth;
				displayMode.h = config->vidHeight;
				displayMode.refresh_rate = (float)Cvar_VariableIntegerValue( "r_displayRefresh" );

				if ( !SDL_SetWindowFullscreenMode( SDL_window, &displayMode ) )
				{
					Com_DPrintf( "SDL_SetWindowFullscreenMode failed: %s\n", SDL_GetError( ) );
					SDL_DestroyWindow( SDL_window );
					SDL_window = NULL;
					continue;
				}

				if ( !SDL_SetWindowFullscreen( SDL_window, true ) )
				{
					Com_DPrintf( "SDL_SetWindowFullscreen failed: %s\n", SDL_GetError( ) );
					SDL_DestroyWindow( SDL_window );
					SDL_window = NULL;
					continue;
				}

				got = SDL_GetWindowFullscreenMode( SDL_window );
				if ( got )
				{
					config->displayFrequency = (int)got->refresh_rate;
					config->vidWidth = got->w;
					config->vidHeight = got->h;
				}
			}
#endif
		}

		config->colorBits = testColorBits;
		config->depthBits = testDepthBits;
		config->stencilBits = testStencilBits;


		Com_Printf( "Using %d color bits, %d depth, %d stencil display.\n",	config->colorBits, config->depthBits, config->stencilBits );

		break;
	}

	if ( SDL_window )
	{
#ifdef USE_ICON
		SDL_Surface *icon = SDL_CreateSurfaceFrom(
			(int)CLIENT_WINDOW_ICON.width,
			(int)CLIENT_WINDOW_ICON.height,
			SDL_PIXELFORMAT_RGBA32,
			(void *)CLIENT_WINDOW_ICON.pixel_data,
			(int)( CLIENT_WINDOW_ICON.bytes_per_pixel * CLIENT_WINDOW_ICON.width )
		);
		if ( icon )
		{
			SDL_SetWindowIcon( SDL_window, icon );
			SDL_DestroySurface( icon );
		}
#endif
	}
	else
	{
		const char *sdl_err = SDL_GetError();
		Com_Printf( "[VK] Couldn't get a visual: %s\n", ( sdl_err && sdl_err[0] ) ? sdl_err : "(no SDL error string)" );
		Com_Printf( "[VK] SDL video driver: %s\n", SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "(none)" );
#if defined(__arm__) || defined(__aarch64__)
		Com_Printf( S_COLOR_YELLOW "[VK] On ARM, Vulkan may be unavailable. Ensure SDL was built with Vulkan support.\n" );
#endif
		/* SDL built without Vulkan: no point retrying modes/drivers */
		if ( sdl_err && strstr( sdl_err, "Vulkan support" ) != NULL )
			return RSERR_FATAL_ERROR;
		return RSERR_INVALID_MODE;
	}

	if ( !fullscreen && r_noborder->integer )
		SDL_SetWindowHitTest( SDL_window, SDL_HitTestFunc, NULL );

	SDL_GetWindowSizeInPixels( SDL_window, &config->vidWidth, &config->vidHeight );

	// save render dimensions as renderer may change it in advance
	glw_state.window_width = config->vidWidth;
	glw_state.window_height = config->vidHeight;

	SDL_WarpMouseInWindow( SDL_window, (float)( glw_state.window_width / 2 ), (float)( glw_state.window_height / 2 ) );

	return RSERR_OK;
}


/*
===============
GLimp_StartDriverAndSetMode
===============
*/
static rserr_t GLimp_StartDriverAndSetMode( int mode, const char *modeFS, qboolean fullscreen )
{
	rserr_t err;

	if ( fullscreen && in_nograb->integer )
	{
		Com_Printf( "Fullscreen not allowed with \\in_nograb 1\n");
		Cvar_Set( "r_fullscreen", "0" );
		r_fullscreen->modified = qfalse;
		fullscreen = qfalse;
	}

	if ( !SDL_WasInit( SDL_INIT_VIDEO ) )
	{
		const char *driverName;
		const char *vidDriver;

		vidDriver = r_vid_driver ? r_vid_driver->string : "auto";
#if defined(__arm__) || defined(__aarch64__)
		/* Raspberry Pi / ARM: Vulkan with KMSDRM has known issues (SDL#3997); force X11 */
		if ( !vidDriver || !vidDriver[0] || ( Q_stricmp( vidDriver, "auto" ) == 0 ) )
			vidDriver = "x11";
		else if ( Q_stricmp( vidDriver, "kmsdrm" ) == 0 )
		{
			Com_Printf( "[VK] ARM: KMSDRM has Vulkan issues, using X11 instead\n" );
			vidDriver = "x11";
		}
#endif
		SDL_SetHint( SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0" );
		if ( vidDriver && vidDriver[0] && Q_stricmp( vidDriver, "auto" ) != 0 ) {
			SDL_SetHint( SDL_HINT_VIDEO_DRIVER, vidDriver );
#ifndef _WIN32
			{
				static char envBuf[64];
				Com_sprintf( envBuf, sizeof( envBuf ), "SDL_VIDEODRIVER=%s", vidDriver );
				putenv( envBuf );
			}
#endif
		}

		if ( !SDL_Init( SDL_INIT_VIDEO ) )
		{
			Com_Printf( "SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n", SDL_GetError() );
			return RSERR_FATAL_ERROR;
		}

		driverName = SDL_GetCurrentVideoDriver();

		Com_Printf( "SDL using driver \"%s\"\n", driverName );
		Com_Printf( "SDL3 video initialized.\n" );

		/* Must load Vulkan AFTER SDL_Init(VIDEO). Doing it earlier leaves a
		 * stale error ("Video subsystem has not been initialized" /
		 * "Invalid fullscreen display mode") that breaks mode selection. */
		SDL_ClearError();
		if ( !SDL_Vulkan_LoadLibrary( NULL ) )
			Com_Printf( "[VK] SDL Vulkan load check: %s\n", SDL_GetError() );
		Com_Printf( "[VK] SDL video driver: %s\n", driverName ? driverName : "(none)" );
	}

	err = GLW_SetMode( mode, modeFS, fullscreen );

	switch ( err )
	{
		case RSERR_INVALID_FULLSCREEN:
			Com_Printf( "...WARNING: fullscreen unavailable in this mode\n" );
			return err;
		case RSERR_INVALID_MODE:
			Com_Printf( "...WARNING: could not set the given mode (%d)\n", mode );
			return err;
		case RSERR_FATAL_ERROR:
			return err;
		default:
			break;
	}

	return RSERR_OK;
}


/*
===============
VKimp_Init

This routine is responsible for initializing the OS specific portions
of Vulkan
===============
*/
void VKimp_Init( glconfig_t *config )
{
	rserr_t err;

#ifndef _WIN32
	InitSig();
#endif

	Com_DPrintf( "VKimp_Init()\n" );

	in_nograb = Cvar_Get( "in_nograb", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( in_nograb, "Do not capture mouse in game, may be useful during online streaming." );

	r_swapInterval = Cvar_Get( "r_swapInterval", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_stereoEnabled = Cvar_Get( "r_stereoEnabled", "0", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_SetDescription( r_stereoEnabled, "Enable stereo rendering for techniques like shutter glasses." );

	// feedback to renderer configuration
	glw_state.config = config;

	// Create the window and set up the context (with driver retry on ARM)
	{
		int driverRetry;

		err = RSERR_UNKNOWN;
		for ( driverRetry = 0; driverRetry < 2; driverRetry++ )
		{
			err = GLimp_StartDriverAndSetMode( r_mode->integer, r_modeFullscreen->string, r_fullscreen->integer);
			if ( err != RSERR_OK && err != RSERR_FATAL_ERROR )
			{
				Com_Printf( "Setting r_mode %d failed, falling back on r_mode %d\n", r_mode->integer, 3 );

				err = GLimp_StartDriverAndSetMode( 3, "", r_fullscreen->integer);
			}
			if ( err != RSERR_OK && err != RSERR_FATAL_ERROR )
			{
				Com_Printf( "r_mode 3 failed, trying r_mode -1 (640x480)\n" );
				Cvar_Set( "r_customWidth", "640" );
				Cvar_Set( "r_customHeight", "480" );
				err = GLimp_StartDriverAndSetMode( -1, "", r_fullscreen->integer);
				if ( err == RSERR_OK )
					Cvar_Set( "r_mode", "-1" );
			}
			if ( err != RSERR_OK && err != RSERR_FATAL_ERROR )
			{
				Com_Printf( "r_mode -1 (640x480) failed, trying 800x600\n" );
				Cvar_Set( "r_customWidth", "800" );
				Cvar_Set( "r_customHeight", "600" );
				err = GLimp_StartDriverAndSetMode( -1, "", r_fullscreen->integer);
				if ( err == RSERR_OK )
					Cvar_Set( "r_mode", "-1" );
			}
			if ( err != RSERR_OK && err != RSERR_FATAL_ERROR && r_fullscreen->integer )
			{
				Com_Printf( "Fullscreen failed, trying windowed mode\n" );
				Cvar_Set( "r_fullscreen", "0" );
				err = GLimp_StartDriverAndSetMode( 3, "", 0);
			}
			if ( err == RSERR_OK )
				break;

			/* On ARM: if x11 failed, try wayland as last resort */
#if defined(__arm__) || defined(__aarch64__)
			{
				const char *vidDriver = SDL_GetCurrentVideoDriver();
				if ( driverRetry == 0 && vidDriver && strcmp( vidDriver, "x11" ) == 0 )
			{
				Com_Printf( "[VK] x11 failed, trying wayland\n" );
				Cvar_Set( "r_vid_driver", "wayland" );
				SDL_QuitSubSystem( SDL_INIT_VIDEO );
				continue;
			}
			}
#endif
			break;
		}

		if ( err == RSERR_FATAL_ERROR )
		{
#if defined(__arm__) || defined(__aarch64__)
			Com_Printf( S_COLOR_YELLOW "Vulkan failed on ARM. SDL needs Vulkan support.\n" );
			Com_Printf( "  Build SDL with Vulkan: ./scripts/build_sdl_vulkan_rpi.sh\n" );
			Com_Printf( "  Try windowed mode: +set r_fullscreen 0\n" );
#endif
			Com_Error( ERR_FATAL, "VKimp_Init() - could not load Vulkan subsystem: %s", SDL_GetError() );
			return;
		}
		if ( err != RSERR_OK )
		{
#if defined(__arm__) || defined(__aarch64__)
			Com_Printf( S_COLOR_YELLOW "Vulkan failed on ARM. SDL may lack Vulkan support.\n" );
			Com_Printf( "  Build SDL with Vulkan: ./scripts/build_sdl_vulkan_rpi.sh\n" );
			Com_Printf( "  Try windowed mode: +set r_fullscreen 0\n" );
#endif
			Com_Error( ERR_FATAL, "VKimp_Init() - could not load Vulkan subsystem: %s", SDL_GetError() );
			return;
		}
	}

	{
		SDL_FunctionPointer sym = SDL_Vulkan_GetVkGetInstanceProcAddr();
		Com_Memcpy( &qvkGetInstanceProcAddr, &sym, sizeof( qvkGetInstanceProcAddr ) );
	}

	if ( qvkGetInstanceProcAddr == NULL )
	{
		SDL_QuitSubSystem( SDL_INIT_VIDEO );
		Com_Error( ERR_FATAL, "VKimp_Init: qvkGetInstanceProcAddr is NULL" );
	}

	// These values force the UI to disable driver selection
	config->driverType = GLDRV_ICD;
	config->hardwareType = GLHW_GENERIC;

	// This depends on SDL_INIT_VIDEO, hence having it here
	IN_Init();

	HandleEvents();

	Key_ClearStates();
}


/*
===============
VK_GetInstanceProcAddr
===============
*/
void *VK_GetInstanceProcAddr( VkInstance instance, const char *name )
{
	PFN_vkVoidFunction fn;
	void *addr = NULL;

	fn = qvkGetInstanceProcAddr( instance, name );
	Com_Memcpy( &addr, &fn, sizeof( addr ) );
	return addr;
}


/*
===============
VK_CreateSurface
===============
*/
qboolean VK_CreateSurface( VkInstance instance, VkSurfaceKHR *surface )
{
	if ( SDL_Vulkan_CreateSurface( SDL_window, instance, NULL, surface ) )
		return qtrue;
	Com_Printf( "SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError() );
	return qfalse;
}


/*
===============
VKimp_Shutdown
===============
*/
void VKimp_Shutdown( qboolean unloadDLL )
{
	const char* drv = SDL_GetCurrentVideoDriver();

	IN_Shutdown();

	if ( glw_state.isFullscreen ) {
		if ( drv && strcmp( drv, "x11" ) == 0 ) {
			SDL_WarpMouseGlobal( (float)( glw_state.desktop_width / 2 ), (float)( glw_state.desktop_height / 2 ) );
		} else {
			SDL_ShowCursor();
		}
	}

	if ( SDL_window ) {
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}

	if ( unloadDLL )
		SDL_QuitSubSystem( SDL_INIT_VIDEO );
}


/*
===============
GLimp_EndFrame

Legacy no-op: Vulkan presents via vk_present_frame().
===============
*/
void GLimp_EndFrame( void )
{
}


/*
================
GLW_HideFullscreenWindow
================
*/
void GLW_HideFullscreenWindow( void ) {
	if ( SDL_window && glw_state.isFullscreen ) {
		SDL_HideWindow( SDL_window );
	}
}


/*
===============
Sys_GetClipboardData
===============
*/
char *Sys_GetClipboardData( void )
{
#ifdef DEDICATED
	return NULL;
#else
	char *data = NULL;
	char *cliptext;

	if ( ( cliptext = SDL_GetClipboardText() ) != NULL ) {
		if ( cliptext[0] != '\0' ) {
			size_t bufsize = strlen( cliptext ) + 1;

			data = Z_Malloc( bufsize );
			Q_strncpyz( data, cliptext, bufsize );

			// find first listed char and set to '\0'
			strtok( data, "\n\r\b" );
		}
		SDL_free( cliptext );
	}
	return data;
#endif
}

qboolean Sys_SetClipboardText( const char *text )
{
#ifdef DEDICATED
	(void)text;
	return qfalse;
#else
	if ( !text ) {
		text = "";
	}
	return SDL_SetClipboardText( text );
#endif
}


/*
===============
Sys_SetClipboardBitmap
===============
*/
void Sys_SetClipboardBitmap( const byte *bitmap, int length )
{
#if defined(_WIN32) && defined(_MSC_VER)
	HGLOBAL hMem;
	byte *ptr;

	if ( !OpenClipboard( NULL ) )
		return;

	EmptyClipboard();
	hMem = GlobalAlloc( GMEM_MOVEABLE | GMEM_DDESHARE, length );
	if ( hMem != NULL ) {
		ptr = ( byte* )GlobalLock( hMem );
		if ( ptr != NULL ) {
			memcpy( ptr, bitmap, length ); 
		}
		GlobalUnlock( hMem );
		SetClipboardData( CF_DIB, hMem );
	}
	CloseClipboard();
#else
	(void)bitmap;
	(void)length;
#endif
}


void Sys_UpdateWindowTitle( const char *title )
{
	if ( SDL_window && title && *title )
	{
		SDL_SetWindowTitle( SDL_window, title );
	}
}
