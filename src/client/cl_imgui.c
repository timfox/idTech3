#include "client.h"
#include "cl_imgui_debug.h"

#ifdef USE_CIMGUI

#ifdef USE_SDL
#include "../sdl/sdl_glw.h"
#include <SDL.h>
#endif

#include <stdbool.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"

typedef struct
{
	qboolean initialized;
	qboolean backendReady;
	qboolean frameActive;
	ImGuiIO *io;
	cvar_t *enable;
	cvar_t *showDemo;
	cvar_t *showMetrics;
} cl_imgui_state_t;

static cl_imgui_state_t cl_imgui;

static void CL_ImGui_RegisterCvars( void )
{
	if ( cl_imgui.enable )
	{
		return;
	}

	cl_imgui.enable = Cvar_Get( "cl_imgui", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_imgui.enable, "Enable the built-in Dear ImGui developer overlay." );

	cl_imgui.showDemo = Cvar_Get( "cl_imguiDemo", "0", CVAR_TEMP );
	Cvar_SetDescription( cl_imgui.showDemo, "Show the Dear ImGui demo window for testing widgets." );

	cl_imgui.showMetrics = Cvar_Get( "cl_imguiMetrics", "0", CVAR_TEMP );
	Cvar_SetDescription( cl_imgui.showMetrics, "Show the Dear ImGui metrics/debugger window." );
}

static void CL_ImGui_DestroyContext( void );

static qboolean CL_ImGui_EnsureBackend( void )
{
	if ( cl_imgui.backendReady )
	{
		return qtrue;
	}

	if ( !re.ImGuiBackendInit || !re.ImGuiBackendNewFrame || !re.ImGuiBackendRenderDrawData || !re.ImGuiBackendShutdown )
	{
		return qfalse;
	}

	if ( !re.ImGuiBackendInit() )
	{
		return qfalse;
	}

	cl_imgui.backendReady = qtrue;
	return qtrue;
}

static qboolean CL_ImGui_CreateContext( void )
{
	if ( cl_imgui.initialized )
	{
		return qtrue;
	}

	if ( !cl_imgui.enable || !cl_imgui.enable->integer )
	{
		return qfalse;
	}

#ifdef USE_SDL
	if ( !SDL_WasInit( SDL_INIT_VIDEO ) || SDL_window == NULL )
	{
		return qfalse;
	}
#else
	return qfalse;
#endif

	ImGuiContext *ctx = igCreateContext( NULL );
	if ( !ctx )
	{
		return qfalse;
	}

	cl_imgui.io = igGetIO_Nil();
	ImGuiIO *io = cl_imgui.io;
	io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io->ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	io->IniFilename = NULL;

#ifdef USE_SDL
	if ( !ImGui_ImplSDL2_InitForVulkan( SDL_window ) )
	{
		igDestroyContext( ctx );
		cl_imgui.io = NULL;
		return qfalse;
	}
#endif

	if ( !CL_ImGui_EnsureBackend() )
	{
#ifdef USE_SDL
		ImGui_ImplSDL2_Shutdown();
#endif
		igDestroyContext( ctx );
		cl_imgui.io = NULL;
		return qfalse;
	}

	cl_imgui.initialized = qtrue;
	return qtrue;
}

static void CL_ImGui_DestroyContext( void )
{
	if ( !cl_imgui.initialized )
	{
		return;
	}

	if ( cl_imgui.backendReady && re.ImGuiBackendShutdown )
	{
		re.ImGuiBackendShutdown();
	}

	cl_imgui.backendReady = qfalse;

#ifdef USE_SDL
	ImGui_ImplSDL2_Shutdown();
#endif

	igDestroyContext( NULL );
	cl_imgui.io = NULL;
	cl_imgui.initialized = qfalse;
	cl_imgui.frameActive = qfalse;
}

static void CL_ImGui_RenderOverlay( void )
{
	if ( !cl_imgui.frameActive )
	{
		return;
	}

	if ( cl_imgui.showDemo && cl_imgui.showDemo->integer )
	{
		bool open = true;
		igShowDemoWindow( &open );
		if ( !open )
		{
			Cvar_Set( "cl_imguiDemo", "0" );
		}
	}

	if ( cl_imgui.showMetrics && cl_imgui.showMetrics->integer )
	{
		bool open = true;
		igShowMetricsWindow( &open );
		if ( !open )
		{
			Cvar_Set( "cl_imguiMetrics", "0" );
		}
	}

	// Render debug overlays
	CL_ImGui_Debug_RenderAll();

	if ( cls.state <= CA_DISCONNECTED )
	{
		return;
	}

	// Simple overlay (can be disabled if debug overlays are shown)
	extern cvar_t *cl_imgui_debug_performance;
	if ( !cl_imgui_debug_performance || !cl_imgui_debug_performance->integer )
	{
		igSetNextWindowPos( (ImVec2){ 10.0f, 10.0f }, ImGuiCond_Always, (ImVec2){ 0.0f, 0.0f } );
		igSetNextWindowBgAlpha( 0.25f );
		if ( igBegin( "ImGuiOverlay", NULL,
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoMove ) )
		{
			const float frameMs = ( cls.realFrametime > 0 ) ? (float)cls.realFrametime : 1.0f;
			igText( "FPS: %.1f", 1000.0f / frameMs );
			igText( "Frame ms: %.2f", frameMs );
			igText( "Renderer: %s", cls.glconfig.renderer_string );
			igEnd();
		}
	}
}

void CL_ImGui_Init( void )
{
	CL_ImGui_RegisterCvars();
	CL_ImGui_Debug_Init();
}

void CL_ImGui_Shutdown( void )
{
	CL_ImGui_Debug_Shutdown();
	CL_ImGui_DestroyContext();
}

void CL_ImGui_FrameBegin( void )
{
	if ( !cl_imgui.enable || !cl_imgui.enable->integer )
	{
		if ( cl_imgui.initialized )
		{
			CL_ImGui_DestroyContext();
		}
		return;
	}

	if ( !cl_imgui.initialized )
	{
		if ( !CL_ImGui_CreateContext() )
		{
			return;
		}
	}

	if ( !CL_ImGui_EnsureBackend() )
	{
		return;
	}

	if ( !re.ImGuiBackendNewFrame || !re.ImGuiBackendRenderDrawData )
	{
		return;
	}

#ifdef USE_SDL
	ImGui_ImplSDL2_NewFrame();
#endif
	re.ImGuiBackendNewFrame();
	igNewFrame();
	cl_imgui.frameActive = qtrue;
}

void CL_ImGui_Draw( void )
{
	if ( !cl_imgui.frameActive )
	{
		return;
	}

	CL_ImGui_RenderOverlay();
}

void CL_ImGui_FrameEnd( void )
{
	if ( !cl_imgui.frameActive )
	{
		return;
	}
	cl_imgui.frameActive = qfalse;

	igRender();

	if ( re.ImGuiBackendRenderDrawData )
	{
		ImDrawData *drawData = igGetDrawData();
		if ( drawData && drawData->CmdListsCount > 0 )
		{
			re.ImGuiBackendRenderDrawData( drawData );
		}
	}
}

#ifdef USE_SDL
qboolean CL_ImGui_ProcessEvent( const SDL_Event *event )
{
	if ( !cl_imgui.initialized )
	{
		return qfalse;
	}

	return ImGui_ImplSDL2_ProcessEvent( event );
}
#else
qboolean CL_ImGui_ProcessEvent( const void *event )
{
	(void)event;
	return qfalse;
}
#endif

qboolean CL_ImGui_WantCaptureMouse( void )
{
	if ( !cl_imgui.initialized || !cl_imgui.io )
	{
		return qfalse;
	}

	return cl_imgui.io->WantCaptureMouse;
}

qboolean CL_ImGui_WantCaptureKeyboard( void )
{
	if ( !cl_imgui.initialized || !cl_imgui.io )
	{
		return qfalse;
	}

	return cl_imgui.io->WantCaptureKeyboard || cl_imgui.io->WantTextInput;
}

#else

void CL_ImGui_Init( void ) {}
void CL_ImGui_Shutdown( void ) {}
void CL_ImGui_FrameBegin( void ) {}
void CL_ImGui_FrameEnd( void ) {}
void CL_ImGui_Draw( void ) {}
qboolean CL_ImGui_ProcessEvent( const void *event )
{
	(void)event;
	return qfalse;
}
qboolean CL_ImGui_WantCaptureMouse( void )
{
	return qfalse;
}
qboolean CL_ImGui_WantCaptureKeyboard( void )
{
	return qfalse;
}

#endif

