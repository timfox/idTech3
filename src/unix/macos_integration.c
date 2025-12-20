#if defined(__APPLE__) && !defined(__ANDROID__)

#include "../common/q_shared.h"
#include "../common/qcommon.h"
#include "macos_platform.h"

// Integration with existing engine initialization

/*
================
Sys_Init_Apple
================
*/
void Sys_Init_Apple(void) {
	// Initialize platform abstraction layer
	if (!Platform_Init()) {
		Com_Error(ERR_FATAL, "Platform initialization failed");
	}
	
	// Set up file system paths
	const char* basePath = Platform_GetBasePath();
	const char* userPath = Platform_GetUserPath();
	const char* resourcePath = Platform_GetResourcePath();
	
	Com_Printf("Apple Platform: Base=%s\n", basePath);
	Com_Printf("Apple Platform: User=%s\n", userPath);
	Com_Printf("Apple Platform: Resource=%s\n", resourcePath);
	
	// File system paths will be set by FS_InitFilesystem()
	// which reads from cvars set here or command line
}

/*
================
Sys_Shutdown_Apple
================
*/
void Sys_Shutdown_Apple(void) {
	Platform_Shutdown();
}

/*
================
Sys_GetPlatformInfo
================
*/
void Sys_GetPlatformInfo(char* info, int maxlen) {
	if (info && maxlen > 0) {
		Platform_GetSystemInfo(info, maxlen);
	}
}

#endif // __APPLE__ && !__ANDROID__

