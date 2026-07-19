/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "client.h"
#include "cl_ref.h"
#include "cl_district.h"
#include "cl_vector_font.h"
#include "../../renderers/common/renderer_backend.h"
#ifdef USE_ARC_BLANC
#include "../../world/arc_blanc/arc_blanc.h"
#endif

#include <limits.h>
#include <stdarg.h>

#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
static cvar_t *cl_renderer;
#endif

static void CL_InitGLimp_Cvars( void );
static void CL_InitRef( void );

#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
/*
=================
CL_SanitizeRendererName

Strip surrounding quotes and leading/trailing whitespace from renderer cvar values.
=================
*/
static void CL_SanitizeRendererName( const char *raw, char *out, size_t outSize )
{
	size_t start = 0;
	size_t end;
	size_t len;

	if ( !out || outSize == 0 ) {
		return;
	}
	out[0] = '\0';
	if ( !raw ) {
		return;
	}

	len = strlen( raw );
	while ( start < len && ( raw[start] == ' ' || raw[start] == '\t' ) ) {
		start++;
	}
	end = len;
	while ( end > start && ( raw[end - 1] == ' ' || raw[end - 1] == '\t' ) ) {
		end--;
	}

	if ( end - start >= 2 ) {
		const char q0 = raw[start];
		const char q1 = raw[end - 1];
		if ( ( q0 == '\"' && q1 == '\"' ) || ( q0 == '\'' && q1 == '\'' ) ) {
			start++;
			end--;
		}
	}

	len = end - start;
	if ( len >= outSize ) {
		len = outSize - 1;
	}
	if ( len > 0 ) {
		memcpy( out, raw + start, len );
	}
	out[len] = '\0';
}
#endif

static void ( *re_RenderScene )( const refdef_t *fd );

static void CL_RenderSceneWithDistricts( const refdef_t *fd ) {
	if ( !CL_StockBaseq3Mode() ) {
		CL_District_AddRefEntitiesToScene();
	}
	if ( re_RenderScene ) {
		re_RenderScene( fd );
	}
}
static void CL_Vid_Restart_f( void );
static void CL_ReloadTtf_f( void );
static void CL_ModeList_f( void );
#if defined(USE_IMGUI) && defined(USE_VULKAN_API)
static void CL_ToggleImgui_f( void );
#endif

// common cvars for GLimp modules
cvar_t	*vid_xpos;			// X coordinate of window position
cvar_t	*vid_ypos;			// Y coordinate of window position
cvar_t	*r_noborder;

cvar_t *r_allowSoftwareGL;	// don't abort out if the pixelformat claims software
cvar_t *r_swapInterval;
cvar_t *r_glDriver;
cvar_t *r_displayRefresh;
cvar_t *r_fullscreen;
cvar_t *r_mode;
cvar_t *r_vid_driver;
cvar_t *r_modeFullscreen;
cvar_t *r_customwidth;
cvar_t *r_customheight;
cvar_t *r_customPixelAspect;

cvar_t *r_colorbits;
// these also shared with renderers:
cvar_t *cl_stencilbits;
cvar_t *cl_depthbits;
cvar_t *cl_drawBuffer;

// Structure containing functions exported from refresh DLL
refexport_t	re;
#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
static void	*rendererLib;
#endif

/*
=================
CL_ReloadTtf_f

Rebuild FreeType HUD/console atlases after r_fontAtlasSize / r_fontDpi / r_fontHint / r_fontMipmap
(or r_font / r_fontSize) changes without a full client restart. Safer than relying
on renderer registration cache alone; use vid_restart if anything looks stale.
=================
*/
static void CL_ReloadTtf_f( void ) {
	if ( !re.RegisterFont ) {
		Com_Printf( S_COLOR_YELLOW "reloadTtf: renderer not loaded.\n" );
		return;
	}
	if ( re.ClearTrueTypeFontCache ) {
		re.ClearTrueTypeFontCache();
	} else {
		Com_Printf( S_COLOR_YELLOW "reloadTtf: renderer API too old (missing ClearTrueTypeFontCache); try vid_restart keep_window\n" );
	}
	CL_RegisterBuiltInTrueTypeFonts();
	VectorFont_Reload();
	Con_CheckResize();
	Com_Printf( "reloadTtf: re-registered built-in TrueType fonts\n" );
}


/*
=================
CL_Vid_Restart

Restart the video subsystem

we also have to reload the UI and CGame because the renderer
doesn't know what graphics to reload
=================
*/
void CL_Ref_VidRestart( refShutdownCode_t shutdownCode ) {

	// Settings may have changed so stop recording now
	if ( CL_VideoRecording() )
		CL_CloseAVI( qfalse );

	if ( clc.demorecording )
		CL_StopRecord_f();

	// clear and mute all sounds until next registration
	S_DisableSounds();

	// shutdown VMs
	CL_ShutdownVMs();

	// shutdown the renderer and clear the renderer interface
	CL_Ref_Shutdown( shutdownCode ); // REF_KEEP_CONTEXT, REF_KEEP_WINDOW, REF_DESTROY_WINDOW

	// client is no longer pure until new checksums are sent
	CL_ResetPureClientAtServer();

	// clear pak references
	FS_ClearPakReferences( FS_UI_REF | FS_CGAME_REF );

	// reinitialize the filesystem if the game directory or checksum has changed
	if ( !clc.demoplaying ) // -EC-
		FS_ConditionalRestart( clc.checksumFeed, qfalse );

	cls.soundRegistered = qfalse;

	// unpause so the cgame definitely gets a snapshot and renders a frame
	Cvar_Set( "cl_paused", "0" );

	CL_ClearMemory();

	// startup all the client stuff
	CL_StartHunkUsers();

	// start the cgame if connected
	if ( ( cls.state > CA_CONNECTED && cls.state != CA_CINEMATIC ) || cls.startCgame ) {
		cls.cgameStarted = qtrue;
		CL_InitCGame();
		// send pure checksums
		CL_SendPureChecksums();
	}

	cls.startCgame = qfalse;
}


/*
=================
CL_Vid_Restart_f

Wrapper for CL_Vid_Restart
=================
*/
static void CL_Vid_Restart_f( void ) {

	if ( Q_stricmp( Cmd_Argv( 1 ), "keep_window" ) == 0 || Q_stricmp( Cmd_Argv( 1 ), "fast" ) == 0 ) {
		// fast path: keep window
		CL_Ref_VidRestart( REF_KEEP_WINDOW );
	} else {
		if ( cls.lastVidRestart ) {
			if ( abs( cls.lastVidRestart - Sys_Milliseconds() ) < 500 ) {
				// hack for OSP mod: do not allow vid restart right after cgame init
				return;
			}
		}
		CL_Ref_VidRestart( REF_DESTROY_WINDOW );
	}
}

/*
================
CL_RefPrintf
================
*/
static void FORMAT_PRINTF(2, 3) QDECL CL_RefPrintf( printParm_t level, const char *fmt, ... ) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	switch ( level ) {
		default: Com_Printf( "%s", msg ); break;
		case PRINT_DEVELOPER: Com_DPrintf( "%s", msg ); break;
		case PRINT_WARNING: Com_Printf( S_COLOR_WARNING "%s", msg ); break;
		case PRINT_ERROR: Com_Printf( S_COLOR_ERROR "%s", msg ); break;
	}
}


/*
============
CL_ShutdownRef
============
*/
void CL_Ref_Shutdown( refShutdownCode_t code ) {

#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
	if ( cl_renderer->modified ) {
		code = REF_UNLOAD_DLL;
	}
#endif

	// clear and mute all sounds until next registration
	// S_DisableSounds();

	if ( code >= REF_DESTROY_WINDOW ) { // +REF_UNLOAD_DLL
		// shutdown sound system before renderer
		// because it may depend from window handle
		S_Shutdown();
	}

	SCR_Done();

	cls.builtInTtfActive = qfalse;
	Com_Memset( &cls.builtInHudFont, 0, sizeof( cls.builtInHudFont ) );
	Com_Memset( &cls.builtInConsoleFont, 0, sizeof( cls.builtInConsoleFont ) );
	cls.builtInHudRefLinePx = 0;
	cls.builtInConsoleRefLinePx = 0;

	if ( re.Shutdown ) {
		re.Shutdown( code );
	}

#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
	if ( rendererLib ) {
		Sys_UnloadLibrary( rendererLib );
		rendererLib = NULL;
	}
#endif

	Com_Memset( &re, 0, sizeof( re ) );

	cls.rendererStarted = qfalse;
}


/*
============
CL_InitRenderer
============
*/
void CL_Ref_InitRenderer( void ) {

	// fixup renderer -EC-
	if ( !re.BeginRegistration ) {
		CL_InitRef();
	}

	// this sets up the renderer and calls R_Init
	re.BeginRegistration( &cls.glconfig );

	// load character sets
	cls.charSetShader = re.RegisterShader( "gfx/2d/bigchars" );
	cls.whiteShader = re.RegisterShader( "white" );
	cls.consoleShader = re.RegisterShader( "console" );

	Con_CheckResize();

	g_console_field_width = ((cls.glconfig.vidWidth / SCR_ConsoleCharWidth())) - 2;
	g_consoleField.widthInChars = g_console_field_width;

	// for 640x480 virtualized screen
	cls.biasY = 0;
	cls.biasX = 0;
	if ( cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640 ) {
		// wide screen, scale by height
		cls.scale = cls.glconfig.vidHeight * (1.0/480.0);
		cls.biasX = 0.5 * ( cls.glconfig.vidWidth - ( cls.glconfig.vidHeight * (640.0/480.0) ) );
	} else {
		// no wide screen, scale by width
		cls.scale = cls.glconfig.vidWidth * (1.0/640.0);
		cls.biasY = 0.5 * ( cls.glconfig.vidHeight - ( cls.glconfig.vidWidth * (480.0/640) ) );
	}

	SCR_Init();

	CL_RegisterBuiltInTrueTypeFonts();
}
/*
============
CL_RefMalloc
============
*/
static void *CL_RefMalloc( int size ) {
	return Z_TagMalloc( size, TAG_RENDERER );
}


/*
============
CL_RefFreeAll
============
*/
static void CL_RefFreeAll( void ) {
	Z_FreeTags( TAG_RENDERER );
}


/*
============
CL_ScaledMilliseconds
============
*/
int CL_ScaledMilliseconds( void ) {
	return Sys_Milliseconds()*com_timescale->value;
}


/*
============
CL_IsMinimized
============
*/
static qboolean CL_IsMininized( void ) {
	return gw_minimized;
}

static qboolean CL_HasFocus( void ) {
	return gw_active;
}

static int CL_GetState( void ) {
	return cls.state;
}


/*
============
CL_SetScaling

Sets console chars height
============
*/
static void CL_SetScaling( float factor, int captureWidth, int captureHeight ) {

	if ( cls.con_factor != factor ) {
		// rescale console
		con_scale->modified = qtrue;
	}

	cls.con_factor = factor;

	// set custom capture resolution
	cls.captureWidth = captureWidth;
	cls.captureHeight = captureHeight;
}


/*
============
CL_InitRef
============
*/
static void CL_InitRef( void ) {
	refimport_t	rimp;
	refexport_t	*ret;
#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
	GetRefAPI_t		getRefAPI;
	char			dllName[ MAX_OSPATH ], *ospath;
#endif

	CL_InitGLimp_Cvars();

	Com_Printf( "----- Initializing Renderer ----\n" );

#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
	/* "renderer" is an alias for cl_renderer. Vulkan is the only shipping backend. */
	char sanitizedRenderer[64];
	const char *rendererName;
	rendererBackendId_t backendId;

	CL_SanitizeRendererName( cl_renderer->string, sanitizedRenderer, sizeof( sanitizedRenderer ) );
	rendererName = sanitizedRenderer;
	{
		const char *alt = Cvar_VariableString( "renderer" );
		if ( alt && alt[0] ) {
			CL_SanitizeRendererName( alt, sanitizedRenderer, sizeof( sanitizedRenderer ) );
			rendererName = sanitizedRenderer;
		}
	}
	if ( sanitizedRenderer[0] && Q_stricmp( cl_renderer->string, sanitizedRenderer ) ) {
		Cvar_Set( "cl_renderer", sanitizedRenderer );
	}
	if ( sanitizedRenderer[0] && Q_stricmp( Cvar_VariableString( "renderer" ), sanitizedRenderer ) ) {
		Cvar_Set( "renderer", sanitizedRenderer );
	}
	backendId = R_BackendIdFromName( rendererName );
	if ( backendId == RENDERER_BACKEND_WEBGPU_WASM ) {
		Com_Printf( S_COLOR_YELLOW
			"Renderer '%s' is a Wasm/WebGPU roadmap target (not a native plugin); using Vulkan. See docs/WEBGPU_ROADMAP.md\n",
			rendererName );
		Cvar_Set( "cl_renderer", "vulkan" );
		Cvar_Set( "renderer", "vulkan" );
		rendererName = "vulkan";
		backendId = RENDERER_BACKEND_VULKAN;
	} else if ( !R_BackendIsDlopenPlugin( backendId ) ) {
		Com_Printf( S_COLOR_YELLOW "Renderer '%s' is not recognized; using Vulkan.\n", rendererName );
		Cvar_Set( "cl_renderer", "vulkan" );
		Cvar_Set( "renderer", "vulkan" );
		rendererName = "vulkan";
		backendId = RENDERER_BACKEND_VULKAN;
	} else if ( !R_BackendIsShipping( backendId ) ) {
		Com_Printf( "Loading roadmap renderer '%s' (%s scaffold; not shippable).\n",
			rendererName, R_BackendDisplayName( backendId ) );
	}

#if defined (__linux__) && defined(__i386__)
#define REND_ARCH_STRING "x86"
#else
#define REND_ARCH_STRING ""
#endif

	{
		const char *clean = rendererName;
		if ( REND_ARCH_STRING[0] != '\0' ) {
			Com_sprintf( dllName, sizeof( dllName ), RENDERER_PREFIX "_%s_" REND_ARCH_STRING DLL_EXT, clean );
		} else {
			Com_sprintf( dllName, sizeof( dllName ), RENDERER_PREFIX "_%s" DLL_EXT, clean );
		}
	}
	ospath = FS_BuildOSPath( Sys_DefaultBasePath(), dllName, NULL );
	Sys_ClearLoadLibraryStickyError();
	rendererLib = Sys_LoadLibrary( ospath );
	if ( !rendererLib )
	{
		if ( backendId != RENDERER_BACKEND_VULKAN ) {
			Com_Printf( S_COLOR_YELLOW
				"Failed to load roadmap renderer from %s: %s — falling back to Vulkan.\n",
				ospath, Sys_GetLoadLibraryError() );
		} else {
			Com_Printf( S_COLOR_YELLOW "Failed to load renderer from %s: %s\n",
				ospath, Sys_GetLoadLibraryError() );
		}
		Cvar_Set( "cl_renderer", "vulkan" );
		Cvar_Set( "renderer", "vulkan" );
		if ( REND_ARCH_STRING[0] != '\0' ) {
			Com_sprintf( dllName, sizeof( dllName ), RENDERER_PREFIX "_vulkan_" REND_ARCH_STRING DLL_EXT );
		} else {
			Com_sprintf( dllName, sizeof( dllName ), RENDERER_PREFIX "_vulkan" DLL_EXT );
		}
		ospath = FS_BuildOSPath( Sys_DefaultBasePath(), dllName, NULL );
		Sys_ClearLoadLibraryStickyError();
		rendererLib = Sys_LoadLibrary( ospath );
		if ( !rendererLib )
		{
			Com_Error( ERR_FATAL, "Failed to load renderer %s: %s", dllName, Sys_GetLoadLibraryError() );
		}
	}

	{
		void *sym = Sys_LoadFunction( rendererLib, "GetRefAPI" );
		Com_Memcpy( &getRefAPI, &sym, sizeof( getRefAPI ) );
	}
	if( !getRefAPI )
	{
#ifdef _WIN32
		Com_Error( ERR_FATAL, "Can't load symbol GetRefAPI from renderer DLL (check exports / arch match): %s",
			Sys_GetLoadLibraryError() );
#else
		Com_Error( ERR_FATAL, "Can't load symbol GetRefAPI" );
#endif
		return;
	}

	cl_renderer->modified = qfalse;
#endif

	Com_Memset( &rimp, 0, sizeof( rimp ) );

	rimp.Cmd_AddCommand = Cmd_AddCommand;
	rimp.Cmd_RemoveCommand = Cmd_RemoveCommand;
	rimp.Cmd_Argc = Cmd_Argc;
	rimp.Cmd_Argv = Cmd_Argv;
	rimp.Cmd_ExecuteText = Cbuf_ExecuteText;
	rimp.Printf = CL_RefPrintf;
	rimp.Error = Com_Error;
	rimp.Milliseconds = CL_ScaledMilliseconds;
	rimp.Microseconds = Sys_Microseconds;
	rimp.Malloc = CL_RefMalloc;
	rimp.FreeAll = CL_RefFreeAll;
	rimp.Free = Z_Free;
#ifdef HUNK_DEBUG
	rimp.Hunk_AllocDebug = Hunk_AllocDebug;
#else
	rimp.Hunk_Alloc = Hunk_Alloc;
#endif
	rimp.Hunk_AllocateTempMemory = Hunk_AllocateTempMemory;
	rimp.Hunk_FreeTempMemory = Hunk_FreeTempMemory;

	rimp.CM_ClusterPVS = CM_ClusterPVS;
	rimp.CM_DrawDebugSurface = CM_DrawDebugSurface;

	rimp.FS_ReadFile = FS_ReadFile;
	rimp.FS_FreeFile = FS_FreeFile;
	rimp.FS_WriteFile = FS_WriteFile;
	rimp.FS_FreeFileList = FS_FreeFileList;
	rimp.FS_ListFiles = FS_ListFiles;
	//rimp.FS_FileIsInPAK = FS_FileIsInPAK;
	rimp.FS_FileExists = FS_FileExists;
	rimp.FS_BuildOSPath = FS_BuildOSPath;

	rimp.Cvar_Get = Cvar_Get;
	rimp.Cvar_Set = Cvar_Set;
	rimp.Cvar_SetValue = Cvar_SetValue;
	rimp.Cvar_CheckRange = Cvar_CheckRange;
	rimp.Cvar_SetDescription = Cvar_SetDescription;
	rimp.Cvar_VariableStringBuffer = Cvar_VariableStringBuffer;
	rimp.Cvar_VariableString = Cvar_VariableString;
	rimp.Cvar_VariableIntegerValue = Cvar_VariableIntegerValue;

	rimp.Cvar_SetGroup = Cvar_SetGroup;
	rimp.Cvar_CheckGroup = Cvar_CheckGroup;
	rimp.Cvar_ResetGroup = Cvar_ResetGroup;

	// cinematic stuff

	rimp.CIN_UploadCinematic = CIN_UploadCinematic;
	rimp.CIN_PlayCinematic = CIN_PlayCinematic;
	rimp.CIN_RunCinematic = CIN_RunCinematic;

	rimp.CL_WriteAVIVideoFrame = CL_WriteAVIVideoFrame;
	rimp.CL_SaveJPGToBuffer = CL_SaveJPGToBuffer;
	rimp.CL_SaveJPG = CL_SaveJPG;
	rimp.CL_LoadJPG = CL_LoadJPG;

	rimp.CL_IsMinimized = CL_IsMininized;
	rimp.CL_GetState = CL_GetState;
	rimp.CL_SetScaling = CL_SetScaling;

	rimp.Sys_SetClipboardBitmap = Sys_SetClipboardBitmap;
	rimp.Sys_LowPhysicalMemory = Sys_LowPhysicalMemory;
	rimp.Com_RealTime = Com_RealTime;

	rimp.GLimp_InitGamma = GLimp_InitGamma;
	rimp.GLimp_SetGamma = GLimp_SetGamma;

	// Vulkan API
#ifdef USE_VULKAN_API
	rimp.VKimp_Init = VKimp_Init;
	rimp.VKimp_Shutdown = VKimp_Shutdown;
	rimp.VK_GetInstanceProcAddr = VK_GetInstanceProcAddr;
	rimp.VK_CreateSurface = VK_CreateSurface;
#endif

#ifdef USE_ARC_BLANC
	rimp.ArcBlancSampleHeight = ArcBlanc_SampleHeight;
#else
	rimp.ArcBlancSampleHeight = NULL;
#endif
	rimp.CL_HasFocus = CL_HasFocus;
#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
	ret = getRefAPI( REF_API_VERSION, &rimp );
#else
	ret = GetRefAPI( REF_API_VERSION, &rimp );
#endif

	Com_Printf( "-------------------------------\n");

	if ( !ret ) {
		Com_Error (ERR_FATAL, "Couldn't initialize refresh" );
	}

	re = *ret;
	re_RenderScene = re.RenderScene;
	re.RenderScene = CL_RenderSceneWithDistricts;

#ifdef USE_ARC_BLANC
	ArcBlanc_SetGpuStepFn( re.ArcBlancGpuOceanStep );
#endif

	// unpause so the cgame definitely gets a snapshot and renders a frame
	Cvar_Set( "cl_paused", "0" );
}
/*
** CL_GetModeInfo
*/
typedef struct vidmode_s
{
	const char	*description;
	int			width, height;
	float		pixelAspect;		// pixel width / height
} vidmode_t;

static const vidmode_t cl_vidModes[] =
{
	{ "Mode  0: 320x240",			320,	240,	1 },
	{ "Mode  1: 400x300",			400,	300,	1 },
	{ "Mode  2: 512x384",			512,	384,	1 },
	{ "Mode  3: 640x480",			640,	480,	1 },
	{ "Mode  4: 800x600",			800,	600,	1 },
	{ "Mode  5: 960x720",			960,	720,	1 },
	{ "Mode  6: 1024x768",			1024,	768,	1 },
	{ "Mode  7: 1152x864",			1152,	864,	1 },
	{ "Mode  8: 1280x1024 (5:4)",	1280,	1024,	1 },
	{ "Mode  9: 1600x1200",			1600,	1200,	1 },
	{ "Mode 10: 2048x1536",			2048,	1536,	1 },
	{ "Mode 11: 856x480 (wide)",	856,	480,	1 },
	// extra modes:
	{ "Mode 12: 1280x960",			1280,	960,	1 },
	{ "Mode 13: 1280x720",			1280,	720,	1 },
	{ "Mode 14: 1280x800 (16:10)",	1280,	800,	1 },
	{ "Mode 15: 1366x768",			1366,	768,	1 },
	{ "Mode 16: 1440x900 (16:10)",	1440,	900,	1 },
	{ "Mode 17: 1600x900",			1600,	900,	1 },
	{ "Mode 18: 1680x1050 (16:10)",	1680,	1050,	1 },
	{ "Mode 19: 1920x1080",			1920,	1080,	1 },
	{ "Mode 20: 1920x1200 (16:10)",	1920,	1200,	1 },
	{ "Mode 21: 2560x1080 (21:9)",	2560,	1080,	1 },
	{ "Mode 22: 3440x1440 (21:9)",	3440,	1440,	1 },
	{ "Mode 23: 3840x2160",			3840,	2160,	1 },
	{ "Mode 24: 4096x2160 (4K)",	4096,	2160,	1 }
};
static const int s_numVidModes = ARRAY_LEN( cl_vidModes );

qboolean CL_GetModeInfo( int *width, int *height, float *windowAspect, int mode, const char *modeFS, int dw, int dh, qboolean fullscreen )
{
	const	vidmode_t *vm;
	float	pixelAspect;

	// set dedicated fullscreen mode
	if ( fullscreen && *modeFS )
		mode = atoi( modeFS );

	if ( mode < -2 )
		return qfalse;

	if ( mode >= s_numVidModes )
		return qfalse;

	// fix unknown desktop resolution
	if ( mode == -2 && (dw == 0 || dh == 0) )
		mode = 3;

	if ( mode == -2 ) { // desktop resolution
		*width = dw;
		*height = dh;
		pixelAspect = r_customPixelAspect->value;
	} else if ( mode == -1 ) { // custom resolution
		*width = r_customwidth->integer;
		*height = r_customheight->integer;
		pixelAspect = r_customPixelAspect->value;
	} else { // predefined resolution
		vm = &cl_vidModes[ mode ];
		*width  = vm->width;
		*height = vm->height;
		pixelAspect = vm->pixelAspect;
	}

	*windowAspect = (float)*width / ( *height * pixelAspect );

	return qtrue;
}


/*
** CL_ModeList_f
*/
static void CL_ModeList_f( void )
{
	int i;

	Com_Printf( "\n" );
	for ( i = 0; i < s_numVidModes; i++ )
	{
		Com_Printf( "%s\n", cl_vidModes[ i ].description );
	}
	Com_Printf( "\n" );
}


#if defined(USE_IMGUI) && defined(USE_VULKAN_API)
/*
Toggle Vulkan ImGui debug inspector (\\r_imgui).
*/
static void CL_ToggleImgui_f( void )
{
	cvar_t *cv;
	int on;

	if ( com_dedicated && com_dedicated->integer ) {
		return;
	}
	if ( CL_StockBaseq3Mode() ) {
		Com_Printf( "ImGui inspector disabled in stock baseq3 mode\n" );
		return;
	}

	cv = Cvar_Get( "r_imgui", "1", CVAR_ARCHIVE_ND );
	on = cv->integer ? 0 : 1;
	Cvar_SetValue( "r_imgui", (float)on );
	Com_Printf( "ImGui inspector %s (r_imgui=%d)\n", on ? "enabled" : "disabled", on );
}
#endif


static void CL_InitGLimp_Cvars( void )
{
	// shared with GLimp
	r_allowSoftwareGL = Cvar_Get( "r_allowSoftwareGL", "0", CVAR_LATCH );
	Cvar_SetDescription( r_allowSoftwareGL, "Legacy cvar (ignored). OpenGL renderer removed; Vulkan only." );
	r_swapInterval = Cvar_Get( "r_swapInterval", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( r_swapInterval, "V-blanks to wait before swapping buffers.\n 0: No V-Sync\n 1: Synced to the monitor's refresh rate." );
	r_glDriver = Cvar_Get( "r_glDriver", OPENGL_DRIVER_NAME, CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( r_glDriver, "Legacy cvar (ignored). OpenGL renderer removed; Vulkan only." );

	r_displayRefresh = Cvar_Get( "r_displayRefresh", "0", CVAR_LATCH );
	Cvar_CheckRange( r_displayRefresh, "0", "500", CV_INTEGER );
	Cvar_SetDescription( r_displayRefresh, "Override monitor refresh rate in fullscreen mode:\n   0 - use current monitor refresh rate\n > 0 - use custom refresh rate" );

	vid_xpos = Cvar_Get( "vid_xpos", "3", CVAR_ARCHIVE );
	Cvar_CheckRange( vid_xpos, NULL, NULL, CV_INTEGER );
	Cvar_SetDescription( vid_xpos, "Saves/sets window X-coordinate when windowed, requires \\vid_restart." );
	vid_ypos = Cvar_Get( "vid_ypos", "22", CVAR_ARCHIVE );
	Cvar_CheckRange( vid_ypos, NULL, NULL, CV_INTEGER );
	Cvar_SetDescription( vid_ypos, "Saves/sets window Y-coordinate when windowed, requires \\vid_restart." );

	r_noborder = Cvar_Get( "r_noborder", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( r_noborder, "0", "1", CV_INTEGER );
	Cvar_SetDescription( r_noborder, "Setting to 1 will remove window borders and title bar in windowed mode, hold ALT to drag & drop it with opened console." );

#if defined(__arm__) || defined(__aarch64__)
	/* ARM/RPi: x11 default avoids Vulkan KMSDRM issues (SDL#3997) */
	r_vid_driver = Cvar_Get( "r_vid_driver", "x11", CVAR_ARCHIVE_ND | CVAR_LATCH );
#else
	r_vid_driver = Cvar_Get( "r_vid_driver", "auto", CVAR_ARCHIVE_ND | CVAR_LATCH );
#endif
	Cvar_SetDescription( r_vid_driver, "SDL video driver: auto, x11, wayland, kmsdrm. On ARM/Raspberry Pi with Vulkan, use x11 if you get 'Couldn't get a visual'. Requires vid_restart." );

#if defined(__arm__) || defined(__aarch64__)
	/* RPi5: r_mode -2 (desktop) often fails; -1 with 640x480 is more reliable */
	r_mode = Cvar_Get( "r_mode", "-1", CVAR_ARCHIVE | CVAR_LATCH );
	r_customwidth = Cvar_Get( "r_customWidth", "640", CVAR_ARCHIVE | CVAR_LATCH );
	r_customheight = Cvar_Get( "r_customHeight", "480", CVAR_ARCHIVE | CVAR_LATCH );
#else
	r_mode = Cvar_Get( "r_mode", "-2", CVAR_ARCHIVE | CVAR_LATCH );
	r_customwidth = Cvar_Get( "r_customWidth", "1600", CVAR_ARCHIVE | CVAR_LATCH );
	r_customheight = Cvar_Get( "r_customHeight", "1024", CVAR_ARCHIVE | CVAR_LATCH );
#endif
	Cvar_CheckRange( r_mode, "-2", va( "%i", s_numVidModes-1 ), CV_INTEGER );
	Cvar_SetDescription( r_mode, "Set video mode:\n -2 - use current desktop resolution\n -1 - use \\r_customWidth and \\r_customHeight\n  0..N - enter \\modelist for details" );
#ifdef _DEBUG
	r_modeFullscreen = Cvar_Get( "r_modeFullscreen", "", CVAR_ARCHIVE | CVAR_LATCH );
#else
	r_modeFullscreen = Cvar_Get( "r_modeFullscreen", "-2", CVAR_ARCHIVE | CVAR_LATCH );
#endif
	Cvar_SetDescription( r_modeFullscreen, "Dedicated fullscreen mode, set to \"\" to use \\r_mode in all cases." );
	r_fullscreen = Cvar_Get( "r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_SetDescription( r_fullscreen, "Fullscreen mode. Set to 0 for windowed mode." );
	r_customPixelAspect = Cvar_Get( "r_customPixelAspect", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( r_customPixelAspect, "Enables custom aspect of the screen, with \\r_mode -1." );
	Cvar_CheckRange( r_customwidth, "4", NULL, CV_INTEGER );
	Cvar_SetDescription( r_customwidth, "Custom width to use with \\r_mode -1." );
	Cvar_CheckRange( r_customheight, "4", NULL, CV_INTEGER );
	Cvar_SetDescription( r_customheight, "Custom height to use with \\r_mode -1." );

	r_colorbits = Cvar_Get( "r_colorbits", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( r_colorbits, "0", "32", CV_INTEGER );
	Cvar_SetDescription( r_colorbits, "Sets color bit depth, set to 0 to use desktop settings." );

	// shared with renderer:
	cl_stencilbits = Cvar_Get( "r_stencilbits", "8", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( cl_stencilbits, "0", "8", CV_INTEGER );
	Cvar_SetDescription( cl_stencilbits, "Stencil buffer size, required to be 8 for stencil shadows." );
	cl_depthbits = Cvar_Get( "r_depthbits", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( cl_depthbits, "0", "32", CV_INTEGER );
	Cvar_SetDescription( cl_depthbits, "Sets precision of Z-buffer." );

	cl_drawBuffer = Cvar_Get( "r_drawBuffer", "GL_BACK", CVAR_CHEAT );
	Cvar_SetDescription( cl_drawBuffer, "Specifies buffer to draw from: GL_FRONT or GL_BACK." );
#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
	cl_renderer = Cvar_Get( "cl_renderer", XSTRING( RENDERER_DEFAULT ), CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_SetDescription( cl_renderer,
		"Renderer backend: vulkan (shipping), metal/dxr (roadmap scaffolds when built), webgpu (Wasm only). Requires \\vid_restart." );
	{
		char sanitizedRenderer[64];
		CL_SanitizeRendererName( cl_renderer->string, sanitizedRenderer, sizeof( sanitizedRenderer ) );
		if ( sanitizedRenderer[0] && Q_stricmp( cl_renderer->string, sanitizedRenderer ) ) {
			Cvar_Set( "cl_renderer", sanitizedRenderer );
		}
		if ( R_BackendIdFromName( sanitizedRenderer ) == RENDERER_BACKEND_WEBGPU_WASM ) {
			Cvar_Set( "cl_renderer", "vulkan" );
			Cvar_Set( "renderer", "vulkan" );
		}
	}
#endif
}

void CL_Ref_Init( void )
{
	CL_InitGLimp_Cvars();

	Cmd_AddCommand( "vid_restart", CL_Vid_Restart_f );
	Cmd_AddCommand( "reloadTtf", CL_ReloadTtf_f );
	Cmd_AddCommand( "modelist", CL_ModeList_f );
#if defined(USE_IMGUI) && defined(USE_VULKAN_API)
	Cmd_AddCommand( "toggle_imgui", CL_ToggleImgui_f );
#endif
}

void CL_Ref_ShutdownCommands( void )
{
	Cmd_RemoveCommand( "vid_restart" );
	Cmd_RemoveCommand( "reloadTtf" );
	Cmd_RemoveCommand( "modelist" );
#if defined(USE_IMGUI) && defined(USE_VULKAN_API)
	Cmd_RemoveCommand( "toggle_imgui" );
#endif
}
