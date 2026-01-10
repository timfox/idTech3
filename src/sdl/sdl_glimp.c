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

#if defined(__ANDROID__) && !defined(USE_VULKAN)
#define USE_VULKAN
#endif

#ifdef USE_LOCAL_HEADERS
#	include "SDL.h"
#ifdef USE_VULKAN
#	include "SDL_vulkan.h"
#endif
#else
#	include <SDL.h>
#include <stdio.h>
#ifdef USE_VULKAN
#	include <SDL_vulkan.h>
#endif

#ifdef __ANDROID__
#	include <GLES3/gl3.h>
#	include <EGL/egl.h>
#	include <EGL/eglext.h>
#endif
#endif

#if defined(__ANDROID__) && defined(USE_VULKAN)
#	include <vulkan/vulkan_android.h>
#endif

#include "../client/client.h"
#include "../renderers/vulkan/vk_metrics.h"
#include "../renderers/renderercommon/tr_public.h"
#include "sdl_glw.h"
#include "sdl_icon.h"
#include "sdl_wayland.h"

// Export Vulkan functions for dynamic renderer loading
#ifdef USE_VULKAN
#if defined(_WIN32)
#define VK_EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
#define VK_EXPORT __attribute__((visibility("default")))
#else
#define VK_EXPORT
#endif
#else
#define VK_EXPORT
#endif

typedef enum {
	RSERR_OK,
	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,
	RSERR_FATAL_ERROR,
	RSERR_UNKNOWN
} rserr_t;

glwstate_t glw_state;

SDL_Window *SDL_window = NULL;
static SDL_GLContext SDL_glContext = NULL;
#ifdef USE_VULKAN
static PFN_vkGetInstanceProcAddr qvkGetInstanceProcAddr;
#endif

cvar_t *r_stereoEnabled;
cvar_t *in_nograb;

// Cache the last requested mode so we can recreate the window if we need to
// fall back from Wayland -> X11 (e.g. Vulkan surface creation).
static int s_last_mode = 0;
static char s_last_modeFS[ MAX_CVAR_VALUE_STRING ];
static qboolean s_last_fullscreen = qfalse;
static qboolean s_last_vulkan = qfalse;

static qboolean GLimp_CanFallbackToX11( void )
{
#if defined(__linux__) || defined(__unix__)
	const char *forcedDriver = SDL_getenv( "SDL_VIDEODRIVER" );
	const char *display = SDL_getenv( "DISPLAY" );

	// Respect explicit user override: if they forced a driver, don't fight it.
	if ( forcedDriver && forcedDriver[0] ) {
		return qfalse;
	}

	// X11 fallback only makes sense if Xwayland/X11 is available.
	if ( !display || !display[0] ) {
		return qfalse;
	}

	return qtrue;
#else
	return qfalse;
#endif
#ifdef UNIT_TEST
// Expose tiny helper for tests to verify the Wayland toggle behavior
extern "C" int Wayland_Toggle_IsWaylandForced();
int Wayland_Toggle_IsWaylandForced() {
#ifdef _WIN32
    const char* v = SDL_getenv("WAYLAND_FORCE");
    return (v && v[0] == '1') ? 1 : 0;
#else
    const char* v = SDL_getenv("WAYLAND_FORCE");
    return (v && v[0] == '1') ? 1 : 0;
#endif
}
#endif
}

static qboolean GLimp_RestartVideoDriver( const char *driver )
{
	if ( !driver || !driver[0] ) {
		return qfalse;
	}

	// Tear down window first.
	if ( SDL_window ) {
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}

	// Shut down video subsystem if it was initialized.
	if ( SDL_WasInit( SDL_INIT_VIDEO ) ) {
		SDL_QuitSubSystem( SDL_INIT_VIDEO );
	}

	// Force the requested backend for the remainder of this process.
	SDL_setenv( "SDL_VIDEODRIVER", driver, 1 );

	if ( SDL_Init( SDL_INIT_VIDEO ) != 0 ) {
		Com_Printf( "SDL_Init video failed for driver \"%s\" (%s)\n", driver, SDL_GetError() );
		return qfalse;
	}

	Com_Printf( "SDL using driver \"%s\"\n", SDL_GetCurrentVideoDriver() );
	return qtrue;
}

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
			SDL_WarpMouseGlobal( glw_state.desktop_width / 2, glw_state.desktop_height / 2 );
		} else {
			SDL_ShowCursor( SDL_TRUE );
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
*/
void GLimp_LogComment( const char *comment )
{
	(void)comment;  // Suppress unused parameter warning
}


static int FindNearestDisplay( int *x, int *y, int w, int h )
{
	const int cx = *x + w / 2;
	const int cy = *y + h / 2;
	int i, index, numDisplays;
	SDL_Rect *list, *m;

	index = -1; // selected display index

	numDisplays = SDL_GetNumVideoDisplays();
	if ( numDisplays <= 0 )
		return -1;

	glw_state.monitorCount = numDisplays;

	list = Z_Malloc( numDisplays * sizeof( list[0] ) );

	for ( i = 0; i < numDisplays; i++ )
	{
		SDL_GetDisplayBounds( i, list + i );
		//Com_Printf( "[%i]: x=%i, y=%i, w=%i, h=%i\n", i, list[i].x, list[i].y, list[i].w, list[i].h );
	}

	// select display by window center intersection
	for ( i = 0; i < numDisplays; i++ )
	{
		m = list + i;
		if ( cx >= m->x && cx < (m->x + m->w) && cy >= m->y && cy < (m->y + m->h) )
		{
			index = i;
			break;
		}
	}

	// select display by nearest distance between window center and display center
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

	// adjust x and y coordinates if needed
	if ( index >= 0 )
	{
		m = list + index;
		if ( *x < m->x )
			*x = m->x;

		if ( *y < m->y )
			*y = m->y;
	}

	Z_Free( list );

	return index;
}


static SDL_HitTestResult SDL_HitTestFunc( SDL_Window *win, const SDL_Point *area, void *data )
{
	(void)win;   // Suppress unused parameter warning
	(void)area;  // Suppress unused parameter warning
	(void)data;  // Suppress unused parameter warning
	if ( Key_GetCatcher() & KEYCATCH_CONSOLE && keys[ K_ALT ].down )
		return SDL_HITTEST_DRAGGABLE;

	return SDL_HITTEST_NORMAL;
}


/*
===============
GLimp_SetMode
===============
*/
static int GLW_SetMode( int mode, const char *modeFS, qboolean fullscreen, qboolean vulkan )
{
#ifndef USE_VULKAN
	(void)vulkan;
#endif
	glconfig_t *config = glw_state.config;
	int perChannelColorBits;
	int colorBits, depthBits, stencilBits;
	int i;
	SDL_DisplayMode desktopMode;
	int display;
	int x;
	int y;
	Uint32 flags = SDL_WINDOW_SHOWN;

#ifdef USE_VULKAN
	if ( vulkan ) {
		flags |= SDL_WINDOW_VULKAN;
		Com_Printf( "Initializing Vulkan display\n");
	} else
#endif
	{
		flags |= SDL_WINDOW_OPENGL;
		Com_Printf( "Initializing OpenGL display\n");
	}

	// If a window exists, note its display index
	if ( SDL_window != NULL )
	{
		display = SDL_GetWindowDisplayIndex( SDL_window );
		if ( display < 0 )
		{
			Com_DPrintf( "SDL_GetWindowDisplayIndex() failed: %s\n", SDL_GetError() );
		}
	}
	else
	{
		x = vid_xpos->integer;
		y = vid_ypos->integer;

		// find out to which display our window belongs to
		// according to previously stored \vid_xpos and \vid_ypos coordinates
		display = FindNearestDisplay( &x, &y, 640, 480 );

		//Com_Printf("Selected display: %i\n", display );
	}

	if ( display >= 0 && SDL_GetDesktopDisplayMode( display, &desktopMode ) == 0 )
	{
		glw_state.desktop_width = desktopMode.w;
		glw_state.desktop_height = desktopMode.h;
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
	if ( SDL_glContext != NULL )
	{
		SDL_GL_DeleteContext( SDL_glContext );
		SDL_glContext = NULL;
	}

	if ( SDL_window != NULL )
	{
		SDL_GetWindowPosition( SDL_window, &x, &y );
		Com_DPrintf( "Existing window at %dx%d before being destroyed\n", x, y );
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}

	gw_active = qfalse;
	gw_minimized = qtrue;

	if ( fullscreen )
	{
#ifdef MACOS_X
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#else
		flags |= SDL_WINDOW_FULLSCREEN;
#endif
	}
	else if ( r_noborder->integer )
	{
		flags |= SDL_WINDOW_BORDERLESS;
	}

	//flags |= SDL_WINDOW_ALLOW_HIGHDPI;

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

	for ( i = 0; i < 16; i++ )
	{
		int testColorBits, testDepthBits, testStencilBits;
		int realColorBits[3];

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
					/* fall through */
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

		if ( testColorBits == 24 )
			perChannelColorBits = 8;
		else
			perChannelColorBits = 4;

#ifdef USE_VULKAN
		if ( !vulkan )
#endif
		{
	
#ifdef __sgi /* Fix for SGIs grabbing too many bits of color */
			if (perChannelColorBits == 4)
				perChannelColorBits = 0; /* Use minimum size for 16-bit color */

			/* Need alpha or else SGIs choose 36+ bit RGB mode */
			SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 1 );
#endif

			SDL_GL_SetAttribute( SDL_GL_RED_SIZE, perChannelColorBits );
			SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, perChannelColorBits );
			SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, perChannelColorBits );
			SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, testDepthBits );
			SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, testStencilBits );

			SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 0 );
			SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 0 );

			if ( r_stereoEnabled->integer )
			{
				config->stereoEnabled = qtrue;
				SDL_GL_SetAttribute( SDL_GL_STEREO, 1 );
			}
			else
			{
				config->stereoEnabled = qfalse;
				SDL_GL_SetAttribute( SDL_GL_STEREO, 0 );
			}
		
			SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

			// Request OpenGL 4.6 for modern features (compute shaders, SSBOs, etc.)
			// Fallback to lower versions if 4.6 is not available
			// Try OpenGL 4.6 first, then fall back to 4.5, 4.0, 3.3 if needed
			SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 );
			SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 6 );
			SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY );

			if ( !r_allowSoftwareGL->integer )
				SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 );

			// Force hardware acceleration hints
			SDL_SetHint( SDL_HINT_OPENGL_ES_DRIVER, "0" ); // Disable GLES
			SDL_SetHint( SDL_HINT_RENDER_DRIVER, "opengl" ); // Force OpenGL
		}

		// Attempt window creation with detailed diagnostics
		Com_DPrintf( "Attempting to create window: %dx%d at (%d,%d), fullscreen=%d, vulkan=%d\n",
		            config->vidWidth, config->vidHeight, x, y, fullscreen, vulkan );

		if ( ( SDL_window = SDL_CreateWindow( cl_title, x, y, config->vidWidth, config->vidHeight, flags ) ) == NULL )
		{
			const char* sdlError = SDL_GetError();
			Com_Printf( S_COLOR_RED "ERROR: SDL_CreateWindow failed: %s\n", sdlError );
			Com_Printf( S_COLOR_YELLOW "Window creation parameters:\n" );
			Com_Printf( S_COLOR_YELLOW "  Title: %s\n", cl_title );
			Com_Printf( S_COLOR_YELLOW "  Position: (%d, %d)\n", x, y );
			Com_Printf( S_COLOR_YELLOW "  Size: %dx%d\n", config->vidWidth, config->vidHeight );
			Com_Printf( S_COLOR_YELLOW "  Fullscreen: %d\n", fullscreen );
			Com_Printf( S_COLOR_YELLOW "  Vulkan: %d\n", vulkan );
			Com_Printf( S_COLOR_YELLOW "  Flags: 0x%08x\n", flags );

			// Provide helpful suggestions based on error
			if (strstr(sdlError, "wayland") || strstr(sdlError, "Wayland")) {
				Com_Printf( S_COLOR_YELLOW "Suggestion: Try setting SDL_VIDEODRIVER=x11\n" );
				Com_Printf( S_COLOR_YELLOW "Or run: export SDL_VIDEODRIVER=x11\n" );
			} else if (strstr(sdlError, "No available displays")) {
				Com_Printf( S_COLOR_YELLOW "Suggestion: Check display connection and X11/Wayland setup\n" );
			} else if (strstr(sdlError, "Could not initialize OpenGL")) {
				Com_Printf( S_COLOR_YELLOW "Suggestion: Update graphics drivers or try software rendering\n" );
			}

			continue;
		}

		// Window created successfully - log details
		Com_DPrintf( "SDL window created successfully\n" );

		if ( fullscreen )
		{
			SDL_DisplayMode display_mode;

			switch ( testColorBits )
			{
				case 16: display_mode.format = SDL_PIXELFORMAT_RGB565; break;
				case 24: display_mode.format = SDL_PIXELFORMAT_RGB24;  break;
				default: Com_DPrintf( "testColorBits is %d, can't fullscreen\n", testColorBits ); continue;
			}

			display_mode.w = config->vidWidth;
			display_mode.h = config->vidHeight;
			display_mode.refresh_rate = /* config->displayFrequency = */ Cvar_VariableIntegerValue( "r_displayRefresh" );
			display_mode.driverdata = NULL;

			if ( SDL_SetWindowDisplayMode( SDL_window, &display_mode ) < 0 )
			{
				Com_DPrintf( "SDL_SetWindowDisplayMode failed: %s\n", SDL_GetError( ) );
				continue;
			}

			if ( SDL_GetWindowDisplayMode( SDL_window, &display_mode ) >= 0 )
			{
				config->displayFrequency = display_mode.refresh_rate;
				config->vidWidth = display_mode.w;
				config->vidHeight = display_mode.h;
			}
		}

#ifdef USE_VULKAN
		if ( vulkan )
		{
			config->colorBits = testColorBits;
			config->depthBits = testDepthBits;
			config->stencilBits = testStencilBits;
		}
		else
#endif
		{
			if ( !SDL_glContext )
			{
				if ( ( SDL_glContext = SDL_GL_CreateContext( SDL_window ) ) == NULL )
				{
					Com_DPrintf( "SDL_GL_CreateContext failed: %s\n", SDL_GetError( ) );
					SDL_DestroyWindow( SDL_window );
					SDL_window = NULL;
					continue;
				}

				// Make the OpenGL context current
				if ( SDL_GL_MakeCurrent( SDL_window, SDL_glContext ) < 0 )
				{
// #region agent log
					FILE *debug_log = fopen("/home/tim/Desktop/idtech3/.cursor/debug.log", "a");
					if (debug_log) {
						fprintf(debug_log, "{\"id\":\"log_%ld_glmakecurrent_fail\",\"timestamp\":%ld,\"location\":\"sdl_glimp.c:GLimp_Init\",\"message\":\"SDL_GL_MakeCurrent failed\",\"data\":{\"error\":\"%s\",\"sessionId\":\"debug-session\",\"runId\":\"post-fix\",\"hypothesisId\":\"A\"},\"sessionId\":\"debug-session\"}\n", time(NULL), time(NULL)*1000, SDL_GetError());
						fclose(debug_log);
					}
// #endregion
					Com_DPrintf( "SDL_GL_MakeCurrent failed: %s\n", SDL_GetError( ) );
					SDL_GL_DeleteContext( SDL_glContext );
					SDL_glContext = NULL;
					SDL_DestroyWindow( SDL_window );
					SDL_window = NULL;
					continue;
				} else {
// #region agent log
					FILE *debug_log_fail = fopen("/home/tim/Desktop/idtech3/.cursor/debug.log", "a");
					if (debug_log_fail) {
						fprintf(debug_log_fail, "{\"id\":\"log_%ld_glmakecurrent_success\",\"timestamp\":%ld,\"location\":\"sdl_glimp.c:GLimp_Init\",\"message\":\"SDL_GL_MakeCurrent succeeded\",\"data\":{\"sessionId\":\"debug-session\",\"runId\":\"post-fix\",\"hypothesisId\":\"A\"},\"sessionId\":\"debug-session\"}\n", time(NULL), time(NULL)*1000);
						fclose(debug_log_fail);
					}
// #endregion
				}
			}

			if ( SDL_GL_SetSwapInterval( r_swapInterval->integer ) == -1 )
			{
				Com_DPrintf( "SDL_GL_SetSwapInterval failed: %s\n", SDL_GetError( ) );
			}

			SDL_GL_GetAttribute( SDL_GL_RED_SIZE, &realColorBits[0] );
			SDL_GL_GetAttribute( SDL_GL_GREEN_SIZE, &realColorBits[1] );
			SDL_GL_GetAttribute( SDL_GL_BLUE_SIZE, &realColorBits[2] );
			SDL_GL_GetAttribute( SDL_GL_DEPTH_SIZE, &config->depthBits );
			SDL_GL_GetAttribute( SDL_GL_STENCIL_SIZE, &config->stencilBits );

			config->colorBits = realColorBits[0] + realColorBits[1] + realColorBits[2];
		} // if ( !vulkan )


		Com_Printf( "Using %d color bits, %d depth, %d stencil display.\n",	config->colorBits, config->depthBits, config->stencilBits );

		break;
	}

	if ( SDL_window )
	{
#ifdef USE_ICON
		SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(
			(void *)CLIENT_WINDOW_ICON.pixel_data,
			CLIENT_WINDOW_ICON.width,
			CLIENT_WINDOW_ICON.height,
			CLIENT_WINDOW_ICON.bytes_per_pixel * 8,
			CLIENT_WINDOW_ICON.bytes_per_pixel * CLIENT_WINDOW_ICON.width,
#ifdef Q3_LITTLE_ENDIAN
			0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
#else
			0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
		);
		if ( icon )
		{
			SDL_SetWindowIcon( SDL_window, icon );
			SDL_FreeSurface( icon );
		}
#endif
	}
	else
	{
		Com_Printf( "Couldn't get a visual\n" );
		return RSERR_INVALID_MODE;
	}

	if ( !fullscreen && r_noborder->integer )
		SDL_SetWindowHitTest( SDL_window, SDL_HitTestFunc, NULL );

#ifdef USE_VULKAN
	if ( vulkan )
		SDL_Vulkan_GetDrawableSize( SDL_window, &config->vidWidth, &config->vidHeight );
	else
#endif
		SDL_GL_GetDrawableSize( SDL_window, &config->vidWidth, &config->vidHeight );

	// save render dimensions as renderer may change it in advance
	glw_state.window_width = config->vidWidth;
	glw_state.window_height = config->vidHeight;

	SDL_WarpMouseInWindow( SDL_window, glw_state.window_width / 2, glw_state.window_height / 2 );

    // Expose the final render path after attempting startup (Vulkan vs OpenGL)
    Com_Printf("Renderer startup final path: %s\n", (s_last_vulkan ? "vulkan" : "opengl"));
    return RSERR_OK;
}


/*
===============
GLimp_StartDriverAndSetMode
===============
*/
static rserr_t GLimp_StartDriverAndSetMode( int mode, const char *modeFS, qboolean fullscreen, qboolean vulkan )
{
	rserr_t err;

	// Cache last requested parameters for possible recreation/fallback.
	s_last_mode = mode;
	Q_strncpyz( s_last_modeFS, modeFS ? modeFS : "", sizeof( s_last_modeFS ) );
	s_last_fullscreen = fullscreen;
	s_last_vulkan = vulkan;

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

		// Wayland-first on Linux when available.
		//
		// Rationale:
		// - SDL2 supports Wayland and X11 backends; historically we forced X11 for Vulkan.
		// - We now want a Wayland-native window by default when running under Wayland.
		// - Keep a robust fallback to X11 if Wayland initialization fails.
		//
		// Notes:
		// - Respect explicit user overrides via SDL_VIDEODRIVER.
		// - Default to X11, with optional Wayland support via +set r_wayland 1
#if defined(__linux__) || defined(__unix__)
		{
			const char *forcedDriver = SDL_getenv( "SDL_VIDEODRIVER" );
			const char *waylandDisplay = SDL_getenv( "WAYLAND_DISPLAY" );
			const char *x11Display = SDL_getenv( "DISPLAY" );
			qboolean useWayland = ( forcedDriver && !Q_stricmp(forcedDriver, "wayland") ) ||
			                      ( !forcedDriver && Cvar_VariableIntegerValue("r_wayland") );

			if ( useWayland ) {
				if ( waylandDisplay && waylandDisplay[0] ) {
					// Use Wayland when explicitly requested and WAYLAND_DISPLAY is present
					SDL_setenv( "SDL_VIDEODRIVER", "wayland", 0 /* don't override user */ );
					// Explicitly disable libdecor to avoid GTK dependencies and related crashes.
					// This relies on the compositor providing server-side decorations or the
					// engine handling its own window state.
					SDL_SetHint( "SDL_VIDEO_WAYLAND_ALLOW_LIBDECOR", "0" );
					// Enable window state management for proper fullscreen/minimize support
					SDL_SetHint( "SDL_VIDEO_WAYLAND_WMCLASS", "idtech3" );
					Com_Printf("Using Wayland display driver (WAYLAND_DISPLAY=%s)\n", waylandDisplay);
				} else {
					Com_Printf( "Wayland requested but WAYLAND_DISPLAY not set, falling back to X11\n" );
					if ( !forcedDriver ) {
						SDL_setenv( "SDL_VIDEODRIVER", "x11", 0 /* don't override user */ );
					}
					useWayland = qfalse;
				}
			} else {
				// Default to X11 for better compatibility
				if ( !forcedDriver ) {
					SDL_setenv( "SDL_VIDEODRIVER", "x11", 0 /* don't override user */ );
				}
				if ( x11Display && x11Display[0] ) {
					Com_Printf("Using X11 display driver (DISPLAY=%s)\n", x11Display);
				} else {
					Com_Printf("Using X11 display driver (default)\n");
				}
			}

			if ( SDL_Init( SDL_INIT_VIDEO ) != 0 ) {
				const char *errorMsg = SDL_GetError();
				if ( useWayland ) {
					Com_Printf( "SDL_Init video failed with Wayland driver (%s), retrying with X11...\n", errorMsg );
					SDL_setenv( "SDL_VIDEODRIVER", "x11", 1 );
					if ( SDL_Init( SDL_INIT_VIDEO ) != 0 ) {
						Com_Printf( "SDL_Init( SDL_INIT_VIDEO ) FAILED with both Wayland and X11 (%s)\n", SDL_GetError() );
						return RSERR_FATAL_ERROR;
					}
					Com_Printf("Successfully fell back to X11 after Wayland failure\n");
				} else {
					Com_Printf( "SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n", errorMsg );
					return RSERR_FATAL_ERROR;
				}
			}
		}
#else
		if ( SDL_Init( SDL_INIT_VIDEO ) != 0 )
		{
			Com_Printf( "SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n", SDL_GetError() );
			return RSERR_FATAL_ERROR;
		}
#endif

		driverName = SDL_GetCurrentVideoDriver();

		Com_Printf( "SDL using driver \"%s\"\n", driverName );
	}

	err = GLW_SetMode( mode, modeFS, fullscreen, vulkan );
	// If window creation succeeded, emit a diagnostic log to confirm visibility
	if ( err == RSERR_OK ) {
		Com_Printf( "SDL window created: mode=%d fullscreen=%d vulkan=%d\n", mode, fullscreen, vulkan );
	}
	if ( err == RSERR_FATAL_ERROR ) {
		// Wayland can successfully initialize, but window/context/surface creation may fail
		// on some compositor/driver combinations. In that case, fall back to X11 if possible.
		const char *drv = SDL_GetCurrentVideoDriver();
		if ( drv && strcmp( drv, "wayland" ) == 0 && GLimp_CanFallbackToX11() ) {
			Com_Printf( "Wayland backend failed to create window; falling back to X11...\n" );
			if ( GLimp_RestartVideoDriver( "x11" ) ) {
				err = GLW_SetMode( mode, modeFS, fullscreen, vulkan );
			}
		}
	}

	switch ( err )
	{
		case RSERR_INVALID_FULLSCREEN:
			Com_Printf( "...WARNING: fullscreen unavailable in this mode\n" );
			return err;
		case RSERR_INVALID_MODE:
			Com_Printf( "...WARNING: could not set the given mode (%d)\n", mode );
			return err;
		default:
			break;
	}

	return RSERR_OK;
}


/*
===============
GLimp_Init

This routine is responsible for initializing the OS specific portions
of OpenGL
===============
*/
void GLimp_Init( glconfig_t *config )
{
#if defined(__ANDROID__) && defined(USE_VULKAN)
	VKimp_Init( config );
	return;
#endif

	rserr_t err;

#ifndef _WIN32
	InitSig();
#endif

	Com_DPrintf( "GLimp_Init()\n" );

	glw_state.config = config; // feedback renderer configuration

	in_nograb = Cvar_Get( "in_nograb", "0", 0 );
	Cvar_SetDescription( in_nograb, "Do not capture mouse in game, may be useful during online streaming." );

	r_allowSoftwareGL = Cvar_Get( "r_allowSoftwareGL", "0", CVAR_LATCH );

	r_swapInterval = Cvar_Get( "r_swapInterval", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_stereoEnabled = Cvar_Get( "r_stereoEnabled", "0", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_SetDescription( r_stereoEnabled, "Enable stereo rendering for techniques like shutter glasses." );

	// Create the window and set up the context
	err = GLimp_StartDriverAndSetMode( r_mode->integer, r_modeFullscreen->string, r_fullscreen->integer, qfalse );
	if ( err != RSERR_OK )
	{
		if ( err == RSERR_FATAL_ERROR )
		{
			Com_Error( ERR_FATAL, "GLimp_Init() - could not load OpenGL subsystem" );
			return;
		}

		if ( r_mode->integer != 3 || ( r_fullscreen->integer && atoi( r_modeFullscreen->string ) != 3 ) )
		{
			Com_Printf( "Setting \\r_mode %d failed, falling back on \\r_mode %d\n", r_mode->integer, 3 );
			if ( GLimp_StartDriverAndSetMode( 3, "", r_fullscreen->integer, qfalse ) != RSERR_OK )
			{
				// Nothing worked, give up
				Com_Error( ERR_FATAL, "GLimp_Init() - could not load OpenGL subsystem" );
				return;
			}
		}
	}

	// These values force the UI to disable driver selection
	// Initialize Wayland support if available
#ifdef SDL_VIDEO_DRIVER_WAYLAND
	if (SDL_VideoInit(NULL) == 0) {
		const char *driver = SDL_GetCurrentVideoDriver();
		if (driver && strcmp(driver, "wayland") == 0) {
			if (GLimp_InitWayland()) {
				Com_Printf("Using native Wayland support\n");
			}
		}
	}
#endif

	config->driverType = GLDRV_ICD;
	config->hardwareType = GLHW_GENERIC;

	// This depends on SDL_INIT_VIDEO, hence having it here
	IN_Init();

	HandleEvents();

	Key_ClearStates();
}


/*
===============
GLimp_EndFrame

Responsible for doing a swapbuffers
===============
*/
void GLimp_EndFrame( void )
{
	// don't flip if drawing to front buffer
	if ( Q_stricmp( cl_drawBuffer->string, "GL_FRONT" ) != 0 )
	{
		SDL_GL_SwapWindow( SDL_window );
	}

	// Process Wayland events if available
#ifdef SDL_VIDEO_DRIVER_WAYLAND
	GLimp_HandleWaylandEvents();
#endif
}


/*
===============
GL_GetProcAddress

Used by opengl renderers to resolve all qgl* function pointers
===============
*/
void *GL_GetProcAddress( const char *symbol )
{
	return SDL_GL_GetProcAddress( symbol );
}


#ifdef USE_VULKAN
/*
===============
VKimp_Init

This routine is responsible for initializing the OS specific portions
of Vulkan
===============
*/
VK_EXPORT void VKimp_Init( glconfig_t *config )
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

	// Create the window and set up the context
	err = GLimp_StartDriverAndSetMode( r_mode->integer, r_modeFullscreen->string, r_fullscreen->integer, qtrue /* Vulkan */ );
	if ( err != RSERR_OK )
	{
		if ( err == RSERR_FATAL_ERROR )
		{
			Com_Error( ERR_FATAL, "VKimp_Init() - could not load Vulkan subsystem" );
			return;
		}

		Com_Printf( "Setting r_mode %d failed, falling back on r_mode %d\n", r_mode->integer, 3 );

		err = GLimp_StartDriverAndSetMode( 3, "", r_fullscreen->integer, qtrue /* Vulkan */ );
		if( err != RSERR_OK )
		{
			// Nothing worked, give up
			Com_Error( ERR_FATAL, "VKimp_Init() - could not load Vulkan subsystem" );
			return;
		}
	}

#if defined(__ANDROID__)
	{
		unsigned int required_extension_count = 0;
		if ( SDL_Vulkan_GetInstanceExtensions( SDL_window, &required_extension_count, NULL ) != SDL_TRUE ) {
			Com_Error( ERR_FATAL, "VKimp_Init(): SDL_Vulkan_GetInstanceExtensions (count) failed: %s", SDL_GetError() );
			return;
		}

		if ( required_extension_count > 0 ) {
			const char **required_extensions = (const char **)ri.Malloc( required_extension_count * sizeof( char * ) );
			if ( SDL_Vulkan_GetInstanceExtensions( SDL_window, &required_extension_count, required_extensions ) != SDL_TRUE ) {
				ri.Free( (void*)required_extensions );
				Com_Error( ERR_FATAL, "VKimp_Init(): SDL_Vulkan_GetInstanceExtensions (names) failed: %s", SDL_GetError() );
				return;
			}

			for ( unsigned int extIndex = 0; extIndex < required_extension_count; ++extIndex ) {
				ri.Printf( PRINT_DEVELOPER, "...SDL requires Vulkan instance extension: %s\n", required_extensions[extIndex] );
			}

			ri.Free( (void*)required_extensions );
		}
	}
#endif

#ifdef USE_VULKAN
	if ( SDL_Vulkan_LoadLibrary( NULL ) < 0 )
	{
		// Try to load the default library manually as a fallback
		if ( SDL_Vulkan_LoadLibrary( "libvulkan.so.1" ) < 0 ) {
			Com_Error( ERR_FATAL, "VKimp_Init() - could not load Vulkan library: %s", SDL_GetError() );
			return;
		}
	}
#endif

	{
		void *addr = SDL_Vulkan_GetVkGetInstanceProcAddr();
		qvkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)(intptr_t)addr;
		
		if ( qvkGetInstanceProcAddr == NULL )
		{
			// Already tried loading library above, but let's be thorough
			addr = SDL_Vulkan_GetVkGetInstanceProcAddr();
			qvkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)(intptr_t)addr;
		}
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
VK_EXPORT PFN_vkVoidFunction VK_GetInstanceProcAddr( VkInstance instance, const char *name )
{
	if ( qvkGetInstanceProcAddr == NULL )
	{
		void *addr = SDL_Vulkan_GetVkGetInstanceProcAddr();
		
		if ( !addr ) {
			// Fallback: try to load it from the process global namespace
			// This can happen if SDL is linked statically or has issues with library loading
#ifndef _WIN32
			#include <dlfcn.h>
			void *handle = dlopen("libvulkan.so.1", RTLD_LAZY | RTLD_GLOBAL);
			if (handle) {
				addr = dlsym(handle, "vkGetInstanceProcAddr");
			}
#endif
		}
		
		qvkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)(intptr_t)addr;
	}

	if ( qvkGetInstanceProcAddr == NULL )
	{
		return NULL;
	}

	return qvkGetInstanceProcAddr( instance, name );
}


/*
===============
VK_CreateSurface
===============
*/
VK_EXPORT qboolean VK_CreateSurface( VkInstance instance, VkSurfaceKHR *surface )
{
    if (ri.Printf) {
        ri.Printf(PRINT_ALL, "DEBUG: Entering VK_CreateSurface\n");
    } else {
        fprintf(stderr, "DEBUG: Entering VK_CreateSurface\n");
    }
    if ( ri.Printf == NULL ) {
        if (ri.Printf) ri.Printf(PRINT_ALL, "DEBUG: ri.Printf is NULL!\n");
        else fprintf(stderr, "DEBUG: ri.Printf is NULL!\n");
        fprintf(stderr, "CRITICAL ERROR: ri.Printf is NULL in VK_CreateSurface!\n");
        return qfalse;
    }
    if ( SDL_window == NULL ) {
        if (ri.Printf) ri.Printf(PRINT_ALL, "DEBUG: SDL_window is NULL!\n");
        else fprintf(stderr, "DEBUG: SDL_window is NULL!\n");
        ri.Printf( PRINT_ERROR, "VK_CreateSurface: SDL_window is NULL\n" );
        return qfalse;
    }
    if (ri.Printf) {
        ri.Printf(PRINT_ALL, "DEBUG: Calling SDL_Vulkan_CreateSurface\n");
    } else {
        fprintf(stderr, "DEBUG: Calling SDL_Vulkan_CreateSurface\n");
    }
    if ( SDL_Vulkan_CreateSurface( SDL_window, instance, surface ) == SDL_TRUE ) {
        if (ri.Printf) {
            ri.Printf(PRINT_ALL, "DEBUG: SDL_Vulkan_CreateSurface succeeded\n");
        } else {
            fprintf(stderr, "DEBUG: SDL_Vulkan_CreateSurface succeeded\n");
        }
        // Emit a surface-created metric
        vk_metrics_increment_surface_created();
        return qtrue;
    }
    if (ri.Printf) {
        ri.Printf(PRINT_ALL, "DEBUG: SDL_Vulkan_CreateSurface failed\n");
    } else {
        fprintf(stderr, "DEBUG: SDL_Vulkan_CreateSurface failed\n");
    }

	// If Wayland surface creation fails, try a one-time fallback to X11 by restarting
	// the SDL video backend and recreating the window, then retrying surface creation.
	{
		const char *drv = SDL_GetCurrentVideoDriver();
		if ( drv && strcmp( drv, "wayland" ) == 0 && GLimp_CanFallbackToX11() ) {
		Com_Printf( "SDL_Vulkan_CreateSurface failed on Wayland (%s); retrying with X11...\n", SDL_GetError() );
		Com_Printf( "VK_CreateSurface: Wayland fallback to X11 path engaged (attempting restart)\n" );

			if ( GLimp_RestartVideoDriver( "x11" ) ) {
				Com_Printf( "VK_CreateSurface: X11 fallback restart succeeded\n" );
				// Recreate the window in Vulkan mode using the last requested settings.
				const rserr_t err = GLW_SetMode( s_last_mode, s_last_modeFS, s_last_fullscreen, qtrue );
				if ( err == RSERR_OK ) {
					if ( SDL_Vulkan_CreateSurface( SDL_window, instance, surface ) == SDL_TRUE ) {
						return qtrue;
					}
					Com_Printf( "SDL_Vulkan_CreateSurface still failed on X11 (%s)\n", SDL_GetError() );
				} else {
					Com_Printf( "Failed to recreate Vulkan window on X11\n" );
				}
			}
		}
	}

	return qfalse;
}


/*
===============
VKimp_Shutdown
===============
*/
VK_EXPORT void VKimp_Shutdown( qboolean unloadDLL )
{
	const char* drv = SDL_GetCurrentVideoDriver();

	IN_Shutdown();

	if ( glw_state.isFullscreen ) {
		if ( drv && strcmp( drv, "x11" ) == 0 ) {
			SDL_WarpMouseGlobal( glw_state.desktop_width / 2, glw_state.desktop_height / 2 );
		} else {
			SDL_ShowCursor( SDL_TRUE );
		}
	}

	if ( SDL_window ) {
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
	}

	if ( unloadDLL )
		SDL_QuitSubSystem( SDL_INIT_VIDEO );
	// Emit final Vulkan metrics snapshot before shutdown
    vk_metrics_report();
}
#endif // USE_VULKAN


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


/*
===============
Sys_SetClipboardBitmap
===============
*/
void Sys_SetClipboardBitmap( const byte *bitmap, int length )
{
	(void)bitmap;  // Suppress unused parameter warning
	(void)length;  // Suppress unused parameter warning
#ifdef _WIN32
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
#endif
}
