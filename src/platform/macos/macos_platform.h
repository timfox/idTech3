#ifndef MACOS_PLATFORM_H
#define MACOS_PLATFORM_H

#if defined(__APPLE__) && !defined(__ANDROID__)

#include "../../common/q_shared.h"

#ifdef TARGET_OS_IPHONE
// iOS platform definitions
#define PLATFORM_IOS 1
#define PLATFORM_MOBILE 1
#else
// macOS platform definitions
#define PLATFORM_MACOS 1
#define PLATFORM_DESKTOP 1
#endif

typedef struct {
	qboolean supportsMetal;
	qboolean supportsMetal2;
	qboolean supportsMetal3;
	qboolean supportsRayTracing;
	qboolean supportsArgumentBuffers;
	qboolean supportsIndirectCommandBuffers;
	qboolean supportsVariableRefreshRate;
	qboolean supportsHDR;
} platformCaps_t;

extern platformCaps_t platformCaps;

qboolean Platform_Init(void);
void Platform_Shutdown(void);

#ifdef TARGET_OS_IPHONE
void* Platform_CreateView(int width, int height);
void Platform_DestroyView(void* view);
void Platform_SetViewSize(void* view, int width, int height);
void Platform_GetViewSize(void* view, int* width, int* height);
#else
void* Platform_CreateWindow(const char* title, int width, int height, qboolean fullscreen);
void Platform_DestroyWindow(void* window);
void Platform_SetWindowSize(void* window, int width, int height);
void Platform_GetWindowSize(void* window, int* width, int* height);
void Platform_SetWindowTitle(void* window, const char* title);
qboolean Platform_IsWindowFullscreen(void* window);
void Platform_SetWindowFullscreen(void* window, qboolean fullscreen);
#endif

void Platform_ProcessEvents(void);
qboolean Platform_GetKeyState(int key);
qboolean Platform_GetMouseState(int* x, int* y, int* buttons);

const char* Platform_GetBasePath(void);
const char* Platform_GetUserPath(void);
const char* Platform_GetResourcePath(void);
void Platform_SetHomeDir(const char* homeDir);

void Platform_GetSystemInfo(char* info, int maxlen);
int Platform_GetCPUCount(void);
qboolean Platform_HasRetinaDisplay(void);

#ifdef TARGET_OS_IPHONE
void iOS_ConfigureScreenResolution(void);
void iOS_SetupControllers(void);
void iOS_InitializeEngine(void* view);
void iOS_ShutdownEngine(void);
void iOS_HandleMemoryWarning(void);
#endif

#endif // __APPLE__ && !__ANDROID__

#endif // MACOS_PLATFORM_H
