#include "client.h"

#ifdef USE_CIMGUI
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

static qboolean cl_imgui_initialized = qfalse;
static ImGuiContext *cl_imgui_context = NULL;

void CL_ImGui_Init( void )
{
	if ( cl_imgui_initialized )
	{
		return;
	}

	cl_imgui_context = igCreateContext( NULL );
	cl_imgui_initialized = ( cl_imgui_context != NULL );
}

void CL_ImGui_Shutdown( void )
{
	if ( !cl_imgui_initialized )
	{
		return;
	}

	igDestroyContext( cl_imgui_context );
	cl_imgui_context = NULL;
	cl_imgui_initialized = qfalse;
}

qboolean CL_ImGui_IsReady( void )
{
	return cl_imgui_initialized;
}
#else
void CL_ImGui_Init( void ) {}
void CL_ImGui_Shutdown( void ) {}
qboolean CL_ImGui_IsReady( void )
{
	return qfalse;
}
#endif

