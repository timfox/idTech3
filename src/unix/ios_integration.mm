#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE

#import <UIKit/UIKit.h>
#import <Foundation/Foundation.h>
#import "../common/qcommon.h"
#import "macos_platform.h"
// Metal renderer will be initialized separately

// iOS-specific integration functions

static void* g_gameView = NULL;
static qboolean g_engineInitialized = qfalse;

/*
================
iOS_InitializeEngine
================
*/
void iOS_InitializeEngine(void* view) {
	if (g_engineInitialized) {
		return;
	}
	
	g_gameView = view;
	
	// Initialize platform
	if (!Platform_Init()) {
		Com_Error(ERR_FATAL, "iOS: Platform initialization failed");
	}
	
	// Set up file system paths
	const char* basePath = Platform_GetBasePath();
	const char* userPath = Platform_GetUserPath();
	const char* resourcePath = Platform_GetResourcePath();
	
	// Initialize engine
	char cmdline[1024];
	Com_sprintf(cmdline, sizeof(cmdline), "+set fs_basepath \"%s\" +set fs_homepath \"%s\"", basePath, userPath);
	Com_Init(cmdline);
	
	// Metal renderer will be initialized by the renderer system
	// when CL_InitRef is called
	
	g_engineInitialized = qtrue;
	Com_Printf("iOS: Engine initialized\n");
}

/*
================
iOS_ShutdownEngine
================
*/
void iOS_ShutdownEngine(void) {
	if (!g_engineInitialized) {
		return;
	}
	
	Com_Shutdown();
	Platform_Shutdown();
	g_engineInitialized = qfalse;
	Com_Printf("iOS: Engine shut down\n");
}

/*
================
iOS_HandleSizeChange
================
*/
void iOS_HandleSizeChange(float width, float height) {
	if (g_gameView) {
		Platform_SetViewSize(g_gameView, (int)width, (int)height);
		Metal_Resize((int)width, (int)height);
	}
}

/*
================
iOS_UpdateFrame
================
*/
void iOS_UpdateFrame(void) {
	if (!g_engineInitialized) {
		return;
	}
	
	// Process platform events
	Platform_ProcessEvents();
	
	// Run engine frame
	Com_Frame();
}

/*
================
iOS_HandleMemoryWarning
================
*/
void iOS_HandleMemoryWarning(void) {
	Com_Printf("iOS: Memory warning received\n");
	// Engine will handle memory cleanup automatically
}

#endif // TARGET_OS_IPHONE

