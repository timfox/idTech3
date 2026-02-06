#if defined(__APPLE__) && !defined(__ANDROID__)

#include "../../common/q_shared.h"
#include "../../common/qcommon.h"
#include "macos_platform.h"

void Sys_Init_Apple(void) {
	if (!Platform_Init()) {
		Com_Error(ERR_FATAL, "Platform initialization failed");
	}
	const char* basePath = Platform_GetBasePath();
	const char* userPath = Platform_GetUserPath();
	const char* resourcePath = Platform_GetResourcePath();
	Com_Printf("Apple Platform: Base=%s\n", basePath);
	Com_Printf("Apple Platform: User=%s\n", userPath);
	Com_Printf("Apple Platform: Resource=%s\n", resourcePath);
}

void Sys_Shutdown_Apple(void) {
	Platform_Shutdown();
}

void Sys_GetPlatformInfo(char* info, int maxlen) {
	if (info && maxlen > 0) {
		Platform_GetSystemInfo(info, maxlen);
	}
}

#endif
