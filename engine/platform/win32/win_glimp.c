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
/*
** WIN_GLIMP.C
**
** This file contains ALL Win32 specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_LogComment
** GLimp_Shutdown
**
** Note that the GLW_xxx functions are Windows specific GL-subsystem
** related functions that are relevant ONLY to win_glimp.c
*/

#include "../../client/client.h"
#include "resource.h"
#include "win_local.h"
#include "glw_win.h"

// Enable High Performance Graphics while using Integrated Graphics.
Q_EXPORT DWORD NvOptimusEnablement = 0x00000001;		// Nvidia
Q_EXPORT int AmdPowerXpressRequestHighPerformance = 1;	// AMD


typedef enum {
	RSERR_OK,

	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,

	RSERR_UNKNOWN
} rserr_t;

#define TRY_PFD_SUCCESS		0
#define TRY_PFD_FAIL_SOFT	1
#define TRY_PFD_FAIL_HARD	2

#ifndef PFD_SUPPORT_COMPOSITION
#define PFD_SUPPORT_COMPOSITION 0x00008000
#endif

static DEVMODE dm_desktop;
static DEVMODE dm_current;

static rserr_t	GLW_SetMode( int mode, const char *modeFS, int colorbits,
							 qboolean cdsFullscreen, qboolean vulkan );

//
// function declaration
//

#ifdef USE_VULKAN_API
qboolean	QVK_Init( void );
void		QVK_Shutdown( qboolean unloadDLL );
#endif

//
// variable declarations
//
glwstate_t glw_state;

// GLimp-specific cvars

/*
** GLW_StartDriverAndSetMode
*/
static rserr_t GLW_StartDriverAndSetMode( int mode, const char *modeFS, int colorbits,
										   qboolean cdsFullscreen, qboolean vulkan )
{
	rserr_t err;

	err = GLW_SetMode( mode, modeFS, colorbits, cdsFullscreen, vulkan );

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
** GLW_InitVulkanDriver
*/
#ifdef USE_VULKAN_API
static qboolean GLW_InitVulkanDriver( int colorbits )
{
	int depthbits;
	int stencilbits;

	// implicitly assume Z-buffer depth == desktop color depth
	if ( cl_depthbits->integer == 0 ) {
		if ( colorbits > 16 ) {
			depthbits = 24;
		} else {
			depthbits = 16;
		}
	} else {
		depthbits = cl_depthbits->integer;
	}

	// do not allow stencil if Z-buffer depth likely won't contain it
	stencilbits = cl_stencilbits->integer;
	if ( depthbits < 24 ) {
		stencilbits = 0;
	}

	glw_state.config->colorBits = colorbits;
	glw_state.config->depthBits = depthbits;
	glw_state.config->stencilBits = stencilbits;

	return qtrue;
}
#endif


/*
** GLW_CreateWindow
**
** Responsible for creating the Win32 window and initializing the OpenGL/Vulkan drivers.
*/
static qboolean GLW_CreateWindow( int width, int height, int colorbits, qboolean cdsFullscreen, qboolean vulkan )
{
	static qboolean s_classRegistered = qfalse;
	RECT			r;
	int				stylebits;
	int				x, y, w, h;
	int				exstyle;
	qboolean		oldFullscreen;
	qboolean		res = qfalse;

	//
	// register the window class if necessary
	//
	if ( !s_classRegistered )
	{
		WNDCLASS wc;

		memset( &wc, 0, sizeof( wc ) );

		wc.style         = 0;
		wc.lpfnWndProc   = (WNDPROC) MainWndProc;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = 0;
		wc.hInstance     = g_wv.hInstance;
		wc.hIcon         = LoadIcon( g_wv.hInstance, MAKEINTRESOURCE(IDI_ICON1));
		wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
		wc.hbrBackground = (HBRUSH)(LRESULT)COLOR_GRAYTEXT;
		wc.lpszMenuName  = 0;
		wc.lpszClassName = T(CLIENT_WINDOW_TITLE);

		if ( !RegisterClass( &wc ) )
		{
			Com_Error( ERR_FATAL, "%s: could not register window class", __func__ );
			return qfalse;
		}
		s_classRegistered = qtrue;
		// Com_Printf( "...registered window class\n" );
	}

	r.left = vid_xpos->integer;
	r.top = vid_ypos->integer;
	r.right = r.left + width;
	r.bottom = r.top + height;

	UpdateMonitorInfo( &r );
	
	//
	// create the HWND if one does not already exist
	//
	if ( !g_wv.hWnd )
	{
		//
		// compute width and height
		//
		//r.left = 0;
		//r.top = 0;
		//r.right  = width;
		//r.bottom = height;
		
		g_wv.borderless = 0;

		if ( cdsFullscreen )
		{
			exstyle = WINDOW_ESTYLE_FULLSCREEN;
			stylebits = WINDOW_STYLE_FULLSCREEN;
		}
		else
		{
			exstyle = WINDOW_ESTYLE_NORMAL;
			if ( r_noborder->integer ) {
				stylebits = WINDOW_STYLE_NORMAL_NB;
				g_wv.borderless = r_noborder->integer;
			} else {
				stylebits = WINDOW_STYLE_NORMAL;
			}
			AdjustWindowRect( &r, stylebits, FALSE );
		}

		w = r.right - r.left;
		h = r.bottom - r.top;

		// select monitor from window rect
		r.left = vid_xpos->integer;
		r.top = vid_ypos->integer;
		r.right = r.left + w;
		r.bottom = r.top + h;
		UpdateMonitorInfo( &r );

		if ( cdsFullscreen )
		{
			x = glw_state.desktopX;
			y = glw_state.desktopY;
		}
		else
		{
			x = vid_xpos->integer;
			y = vid_ypos->integer;

			// adjust window coordinates if necessary 
			// so that the window is completely on screen
			if ( w < glw_state.desktopWidth && (x + w) > glw_state.desktopWidth + glw_state.desktopX )
				x = ( glw_state.desktopWidth + glw_state.desktopX - w );
			if ( h < glw_state.desktopHeight && (y + h) > glw_state.desktopHeight + glw_state.desktopY )
				y = ( glw_state.desktopHeight + glw_state.desktopY - h );

			if ( x < glw_state.desktopX )
				x = glw_state.desktopX;
			if ( y < glw_state.desktopY )
				y = glw_state.desktopY;
		}

		stylebits &= ~WS_VISIBLE; // show window only after successive OpenGL/Vulkan initialization
			
		oldFullscreen = glw_state.cdsFullscreen;
		glw_state.cdsFullscreen = cdsFullscreen;

		g_wv.hWnd = CreateWindowEx( exstyle, TEXT(CLIENT_WINDOW_TITLE), AtoW(cl_title),
			 stylebits, x, y, w, h, NULL, NULL, g_wv.hInstance,  NULL );

		if ( !g_wv.hWnd )
		{
			glw_state.cdsFullscreen = oldFullscreen;
			Com_Error( ERR_FATAL, "GLW_CreateWindow() - Couldn't create window" );
			return qfalse;
		}

		// we must reflect actual drawable dimensions in glconfig
		GetClientRect( g_wv.hWnd, &r );
		glw_state.config->vidWidth =  r.right - r.left;
		glw_state.config->vidHeight =  r.bottom - r.top;

		Com_Printf( "...created window@%d,%d (%dx%d)\n", x, y, w, h );
	}
	else
	{
		Com_Printf( "...window already present, CreateWindowEx skipped\n" );
	}

	if ( colorbits == 0 )
		colorbits = dm_desktop.dmBitsPerPel;

#ifdef USE_VULKAN_API
	if ( vulkan )
		res = GLW_InitVulkanDriver( colorbits );
#endif

	if ( !res )
	{
		//ShowWindow( g_wv.hWnd, SW_HIDE );
		DestroyWindow( g_wv.hWnd );
		g_wv.hWnd = NULL;
		return qfalse;
	}

	//SetForegroundWindow( g_wv.hWnd );
	//SetFocus( g_wv.hWnd );

	//ShowWindow( g_wv.hWnd, SW_SHOW );
	//UpdateWindow( g_wv.hWnd );

	return qtrue;
}


static void PrintCDSError( int value )
{
	switch ( value )
	{
	case DISP_CHANGE_RESTART:
		Com_Printf( "restart required\n" );
		break;
	case DISP_CHANGE_BADPARAM:
		Com_Printf( "bad param\n" );
		break;
	case DISP_CHANGE_BADFLAGS:
		Com_Printf( "bad flags\n" );
		break;
	case DISP_CHANGE_FAILED:
		Com_Printf( "DISP_CHANGE_FAILED\n" );
		break;
	case DISP_CHANGE_BADMODE:
		Com_Printf( "bad mode\n" );
		break;
	case DISP_CHANGE_NOTUPDATED:
		Com_Printf( "not updated\n" );
		break;
	default:
		Com_Printf( "unknown error %d\n", value );
		break;
	}
}


static void ResetDisplaySettings( qboolean verbose )
{
	if ( verbose )
		Com_Printf( "...restoring display settings\n" );

	if ( glw_state.displayName[0] )
		ChangeDisplaySettingsEx( glw_state.displayName, NULL, NULL, 0, NULL );
	else
		ChangeDisplaySettings( NULL, 0 );
}


static LONG ApplyDisplaySettings( DEVMODE *dm )
{
	DEVMODE curr;
	LONG lResult;
	BOOL bResult;

	Com_Memset( &curr, 0, sizeof( curr ) );
	curr.dmSize = sizeof( DEVMODE );

	// Get current display mode on current monitor
	if ( glw_state.displayName[0] )
		bResult = EnumDisplaySettings( glw_state.displayName, ENUM_CURRENT_SETTINGS, &curr );
	else
		bResult = EnumDisplaySettings( NULL, ENUM_CURRENT_SETTINGS, &curr );

	if ( !bResult )
		return DISP_CHANGE_FAILED;

#ifdef FAST_MODE_SWITCH
	// Check if current resolution is the same as we want to set
	if ( curr.dmDisplayFrequency &&
		curr.dmPelsWidth == dm->dmPelsWidth &&
		curr.dmPelsHeight == dm->dmPelsHeight &&
		(curr.dmBitsPerPel == dm->dmBitsPerPel || dm->dmBitsPerPel == 0 ) &&
		(curr.dmDisplayFrequency == dm->dmDisplayFrequency || dm->dmDisplayFrequency ==0)) 
	{
		memcpy( &dm_current, &curr, sizeof( dm_current ) );
		return DISP_CHANGE_SUCCESSFUL; // simulate success
	}
#endif

	// Uninitialized?
	if ( dm->dmDisplayFrequency == 0 && dm->dmPelsWidth == 0 && 
		dm->dmPelsHeight == 0 && dm->dmBitsPerPel == 0 ) {
		if ( dm_desktop.dmPelsWidth && dm_desktop.dmPelsHeight ) {
			return ApplyDisplaySettings( &dm_desktop );
		}
	}

	// Apply requested mode
	if ( glw_state.displayName[0] )
		lResult = ChangeDisplaySettingsEx( glw_state.displayName, dm, NULL, CDS_FULLSCREEN, NULL );
	else
		lResult = ChangeDisplaySettings( dm, 0 );

	if ( lResult == DISP_CHANGE_SUCCESSFUL )
		memcpy( &dm_current, dm, sizeof( dm_current ) );

	return lResult;
}


void SetGameDisplaySettings( void ) 
{
	ApplyDisplaySettings( &dm_current );
}


void SetDesktopDisplaySettings( void )
{
	ResetDisplaySettings( qfalse );

	memset( &dm_desktop, 0, sizeof( dm_desktop ) );
	dm_desktop.dmSize = sizeof( DEVMODE );

	if ( glw_state.displayName[0] )
		EnumDisplaySettings( glw_state.displayName, ENUM_CURRENT_SETTINGS, &dm_desktop );
	else
		EnumDisplaySettings( NULL, ENUM_CURRENT_SETTINGS, &dm_desktop );
}


void UpdateMonitorInfo( const RECT *target ) 
{
	MONITORINFOEX mInfo;
	DEVMODE	devMode;
	HMONITOR hMon;
	const RECT *Rect;
	int w, h, x ,y;

	glw_state.monitorCount = GetSystemMetrics( SM_CMONITORS );

	if ( target )
		Rect = target;
	else if ( g_wv.winRectValid )
		Rect = &g_wv.winRect;
	else
		Rect = &g_wv.conRect;

	// try to get more correct data
	hMon = MonitorFromRect( Rect, MONITOR_DEFAULTTONEAREST );
	memset( &mInfo, 0, sizeof( mInfo ) );
	mInfo.cbSize = sizeof( MONITORINFOEX );

	memset( &devMode, 0, sizeof( devMode ) );
	devMode.dmSize = sizeof( DEVMODE );

	if ( GetMonitorInfo( hMon, (LPMONITORINFO)&mInfo ) && EnumDisplaySettings( mInfo.szDevice, ENUM_CURRENT_SETTINGS, &devMode ) ) {
		w = mInfo.rcMonitor.right - mInfo.rcMonitor.left;
		h = mInfo.rcMonitor.bottom - mInfo.rcMonitor.top;
		x = mInfo.rcMonitor.left;
		y = mInfo.rcMonitor.top;

		// try to detect DPI scale
		// we can't properly handle it but at least detect monitor resolution 
		// and inform user in console
		if ( devMode.dmPelsWidth > w || devMode.dmPelsHeight > h ) {
			int scaleX, scaleY;
			scaleX = (devMode.dmPelsWidth * 100) / w;
			scaleY = (devMode.dmPelsHeight * 100) / h;
			if ( scaleX == scaleY ) {
				Com_Printf( S_COLOR_YELLOW "...detected DPI scale: %i%%\n", scaleX );
				w = devMode.dmPelsWidth;
				h = devMode.dmPelsHeight;
			}
		}

		if ( glw_state.desktopWidth != w || glw_state.desktopHeight != h || 
			glw_state.desktopX != x || glw_state.desktopY != y || 
			glw_state.hMonitor != hMon ) {
				// track monitor and gamma change
				qboolean gammaSet = glw_state.gammaSet;

				GLW_RestoreGamma();

				glw_state.desktopWidth = w;
				glw_state.desktopHeight = h;
				glw_state.desktopX = x;
				glw_state.desktopY = y;
				glw_state.hMonitor = hMon;
				memcpy( glw_state.displayName, mInfo.szDevice, sizeof( glw_state.displayName ) );

				glw_state.desktopBitsPixel = devMode.dmBitsPerPel;

				Com_Printf( "...current monitor: %ix%i@%i,%i %s\n", 
					w, h, x, y, WtoA( mInfo.szDevice ) );

				if ( gammaSet && re.SetColorMappings ) {
					re.SetColorMappings();
				}
		}

		glw_state.workArea = mInfo.rcWork;

	} else {
		// no information about current monitor, get desktop settings
		HDC hDC = GetDC( GetDesktopWindow() );
		glw_state.desktopX = 0;
		glw_state.desktopY = 0;
		glw_state.desktopWidth = GetDeviceCaps( hDC, HORZRES );
		glw_state.desktopHeight = GetDeviceCaps( hDC, VERTRES );
		ReleaseDC( GetDesktopWindow(), hDC );

		glw_state.displayName[0] = '\0';

		SystemParametersInfo( SPI_GETWORKAREA, 0, &glw_state.workArea, 0 );
	}
}


/*
** GLW_SetMode
*/
static rserr_t GLW_SetMode( int mode, const char *modeFS, int colorbits, qboolean cdsFullscreen, qboolean vulkan )
{
	//HDC hDC;
	RECT r;
	const char *win_fs[] = { "W", "FS" };
	glconfig_t *config = glw_state.config;
	int		cdsRet;
	DEVMODE dm;

	r.left = vid_xpos->integer;
	r.top = vid_ypos->integer;
	r.right = r.left + 320;
	r.bottom = r.top + 240;

	UpdateMonitorInfo( &r );

	if ( dm_desktop.dmSize == 0 )
	{
		SetDesktopDisplaySettings();
	}

	//
	// print out informational messages
	//
	Com_Printf( "...setting mode %d:", mode );
	if ( !CL_GetModeInfo( &config->vidWidth, &config->vidHeight, &config->windowAspect,
		mode, modeFS, glw_state.desktopWidth, glw_state.desktopHeight, cdsFullscreen ) )
	{
		Com_Printf( " invalid mode\n" );
		return RSERR_INVALID_MODE;
	}
	Com_Printf( " %d %d %s\n", config->vidWidth, config->vidHeight, win_fs[ cdsFullscreen ] );

	//
	// verify desktop bit depth
	//
	if ( glw_state.desktopBitsPixel < 15 || glw_state.desktopBitsPixel == 24 )
	{
		if ( colorbits == 0 || ( !cdsFullscreen && colorbits >= 15 ) )
		{
			if ( MessageBox( NULL,
						T("It is highly unlikely that a correct\n") \
						T("windowed display can be initialized with\n") \
						T("the current desktop display depth.  Select\n") \
						T("'OK' to try anyway.  Press 'Cancel' if you\n") \
						T("have a 3Dfx Voodoo, Voodoo-2, or Voodoo Rush\n") \
						T("3D accelerator installed, or if you otherwise\n") \
						T("wish to quit."),	T("Low Desktop Color Depth"),
						MB_OKCANCEL | MB_ICONEXCLAMATION ) != IDOK )
			{
				return RSERR_INVALID_MODE;
			}
		}
	}

	// do a CDS if needed
	if ( cdsFullscreen )
	{
		memset( &dm, 0, sizeof( dm ) );
		
		dm.dmSize = sizeof( dm );
		
		dm.dmPelsWidth  = config->vidWidth;
		dm.dmPelsHeight = config->vidHeight;
		dm.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT;

		if ( Cvar_VariableIntegerValue( "r_displayRefresh" ) )
		{
			dm.dmDisplayFrequency = Cvar_VariableIntegerValue( "r_displayRefresh" );
			dm.dmFields |= DM_DISPLAYFREQUENCY;
		}
		else // try to set at least desktop refresh rate?
		if ( (dm_desktop.dmDisplayFrequency 
				&& dm.dmPelsWidth <= dm_desktop.dmPelsWidth 
				&& dm.dmPelsHeight <= dm_desktop.dmPelsWidth) 
				|| (dm_current.dmDisplayFrequency 
				&& dm.dmPelsWidth <= dm_current.dmPelsWidth 
				&& dm.dmPelsHeight <= dm_current.dmPelsWidth)) {
			//dm.dmDisplayFrequency = dm_desktop.dmDisplayFrequency;
			//dm.dmFields |= DM_DISPLAYFREQUENCY;
			//Com_Printf("...using display refresh rate: %iHz\n", 
			//	dm_desktop.dmDisplayFrequency );
		}
		
		// try to change color depth if possible
		if ( colorbits != 0 )
		{
			dm.dmBitsPerPel = colorbits;
			dm.dmFields |= DM_BITSPERPEL;
			Com_Printf( "...using colorsbits of %d\n", colorbits );
		}
		else
		{
			Com_Printf( "...using desktop display depth of %d\n", glw_state.desktopBitsPixel );
		}

		//
		// if we're already in fullscreen then just create the window
		//
		if ( glw_state.cdsFullscreen )
		{
			Com_Printf( "...already fullscreen, avoiding redundant CDS\n" );

			if ( !GLW_CreateWindow( config->vidWidth, config->vidHeight, colorbits, qtrue, vulkan ) )
			{
				ResetDisplaySettings( qtrue );
				glw_state.cdsFullscreen = qfalse;
				return RSERR_INVALID_MODE;
			}
		}
		//
		// need to call CDS
		//
		else
		{
			Com_Printf( "...calling CDS: " );
			
			// try setting the exact mode requested, because some drivers don't report
			// the low res modes in EnumDisplaySettings, but still work
			if ( ( cdsRet = ApplyDisplaySettings( &dm ) ) == DISP_CHANGE_SUCCESSFUL )
			{
				Com_Printf( "ok\n" );

				if ( !GLW_CreateWindow( config->vidWidth, config->vidHeight, colorbits, qtrue, vulkan ) )
				{
					ResetDisplaySettings( qtrue );
					glw_state.cdsFullscreen = qfalse;
					return RSERR_INVALID_MODE;
				}
			}
			else
			{
				//
				// the exact mode failed, so scan EnumDisplaySettings for the next largest mode
				//
				DEVMODE		devmode;
				int			modeNum;

				Com_Printf( "failed, " );
				
				PrintCDSError( cdsRet );
			
				Com_Printf( "...trying next higher resolution:" );
				
				// we could do a better matching job here...
				for ( modeNum = 0 ; ; modeNum++ ) {
					BOOL bResult;

					Com_Memset( &devmode, 0, sizeof( devmode ) );
					devmode.dmSize = sizeof( DEVMODE );

					if ( glw_state.displayName[0] )
						bResult = EnumDisplaySettings( glw_state.displayName, modeNum, &devmode );
					else
						bResult = EnumDisplaySettings( NULL, modeNum, &devmode );

					if ( !bResult ) {
						modeNum = -1;
						break;
					}
					if ( devmode.dmPelsWidth >= config->vidWidth 
						&& devmode.dmPelsHeight >= config->vidHeight
						&& devmode.dmBitsPerPel >= 15 ) {
						break;
					}
				}

				if ( modeNum != -1 && ( cdsRet = ApplyDisplaySettings( &devmode ) ) == DISP_CHANGE_SUCCESSFUL )
				{
					Com_Printf( " ok\n" );
					if ( !GLW_CreateWindow( config->vidWidth, config->vidHeight, colorbits, qtrue, vulkan) )
					{
						ResetDisplaySettings( qtrue );
						glw_state.cdsFullscreen = qfalse;
						return RSERR_INVALID_MODE;
					}
				}
				else
				{
					Com_Printf( " failed, " );
					
					PrintCDSError( cdsRet );
					
					ResetDisplaySettings( qtrue );
					glw_state.cdsFullscreen = qfalse;
					glw_state.config->isFullscreen = qfalse;
					if ( !GLW_CreateWindow( config->vidWidth, config->vidHeight, colorbits, qfalse, vulkan ) )
					{
						return RSERR_INVALID_MODE;
					}
					return RSERR_INVALID_FULLSCREEN;
				}
			}
		}
	}
	else // !cdsFullscreen
	{
		if ( glw_state.cdsFullscreen )
		{
			ResetDisplaySettings( qtrue );
			glw_state.cdsFullscreen = qfalse;
		}

		if ( !GLW_CreateWindow( config->vidWidth, config->vidHeight, colorbits, qfalse, vulkan ) )
		{
			return RSERR_INVALID_MODE;
		}
	}

	//
	// success, now check display frequency, although this won't be valid on Voodoo(2)
	//
	memset( &dm, 0, sizeof( dm ) );
	dm.dmSize = sizeof( dm );
	if ( EnumDisplaySettings( glw_state.displayName, ENUM_CURRENT_SETTINGS, &dm ) ) 
	{
		glw_state.config->displayFrequency = dm.dmDisplayFrequency;
	}

	// NOTE: this is overridden later on standalone 3Dfx drivers
	glw_state.config->isFullscreen = cdsFullscreen;
	//glw_state.config->colorBits = dm.dmBitsPerPel;

	return RSERR_OK;
}




/*
** GLimp_LogComment
**
** Writes renderer debug comments to the log file when glw_state.log_fp is set.
** Platform function used by both OpenGL and Vulkan renderers.
*/
void GLimp_LogComment( const char *comment )
{
	if ( glw_state.log_fp )
	{
		fprintf( glw_state.log_fp, "%s", comment );
	}
}


#ifdef USE_VULKAN_API
static qboolean GLW_LoadVulkan( void )
{
	//
	// load the driver and bind our function pointers to it
	//
	if ( QVK_Init() )
	{
		qboolean cdsFullscreen = (r_fullscreen->integer != 0);

		// create the window and set up the context
		if ( GLW_StartDriverAndSetMode( r_mode->integer, r_modeFullscreen->string, r_colorbits->integer, cdsFullscreen, qtrue ) == RSERR_OK )
			return qtrue;
	}

	QVK_Shutdown( qtrue );

	return qfalse;
}


static qboolean GLW_StartVulkan( void )
{
	//
	// load and initialize Vulkan driver
	//
	if ( !GLW_LoadVulkan() ) {
		Com_Error( ERR_FATAL, "GLW_StartVulkan() - could not load Vulkan subsystem\n" );
		return qfalse;
	}

	return qtrue;
}


/*
** VKimp_Init
**
** This is the platform specific Vulkan initialization function.  It
** is responsible for loading Vulkan, initializing it, setting
** extensions, creating a window of the appropriate size, doing
** fullscreen manipulations, etc.  Its overall responsibility is
** to make sure that a functional Vulkan subsystem is operating
** when it returns to the ref.
*/
void VKimp_Init( glconfig_t *config )
{
	Com_Printf( "Initializing Vulkan subsystem\n" );

	// feedback to renderer configuration
	glw_state.config = config;

	// load appropriate DLL and initialize subsystem
	if ( !GLW_StartVulkan() )
		return;

	config->driverType = GLDRV_ICD;
	config->hardwareType = GLHW_GENERIC;

	// show main window after all initializations
	ShowWindow( g_wv.hWnd, SW_SHOW );

	IN_Init();

	HandleEvents();

	Key_ClearStates();
}


/*
** VKimp_Shutdown
**
** This routine does all OS specific shutdown procedures for the Vulkan
** subsystem.
*/
void VKimp_Shutdown( qboolean unloadDLL )
{
	IN_Shutdown();

	Com_Printf( "Shutting down Vulkan subsystem\n" );

	// restore gamma
	GLW_RestoreGamma();

	// destroy window
	if ( g_wv.hWnd )
	{
		Com_Printf( "...destroying window\n" );
		//ShowWindow( g_wv.hWnd, SW_HIDE );
		DestroyWindow( g_wv.hWnd );
		g_wv.hWnd = NULL;
	}

	// reset display settings
	if ( glw_state.cdsFullscreen )
	{
		ResetDisplaySettings( qtrue );
		glw_state.cdsFullscreen = qfalse;
	}

	// shutdown QVK subsystem
	QVK_Shutdown( unloadDLL );
}
#endif // USE_VULKAN_API
