#ifndef MACOS_PLATFORM_H
#define MACOS_PLATFORM_H

#if defined(__APPLE__) && !defined(__ANDROID__)

#include "../qcommon/q_shared.h"

#ifdef TARGET_OS_IPHONE
// iOS platform definitions
#define PLATFORM_IOS 1
#define PLATFORM_MOBILE 1
#else
// macOS platform definitions
#define PLATFORM_MACOS 1
#define PLATFORM_DESKTOP 1
#endif

// Platform capabilities
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

// Platform initialization
qboolean Platform_Init(void);
void Platform_Shutdown(void);

// Window/View management
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

// Input handling
void Platform_ProcessEvents(void);
qboolean Platform_GetKeyState(int key);
qboolean Platform_GetMouseState(int* x, int* y, int* buttons);

// File system
const char* Platform_GetBasePath(void);
const char* Platform_GetUserPath(void);
const char* Platform_GetResourcePath(void);
void Platform_SetHomeDir(const char* homeDir); // iOS: Override default home directory

// System info
void Platform_GetSystemInfo(char* info, int maxlen);
int Platform_GetCPUCount(void);
qboolean Platform_HasRetinaDisplay(void);

// iOS-specific functions
#ifdef TARGET_OS_IPHONE
void iOS_ConfigureScreenResolution(void); // Configure screen resolution cvars
void iOS_SetupControllers(void); // Setup GameController framework
void iOS_InitializeEngine(void* view); // Initialize engine with view
void iOS_ShutdownEngine(void); // Shutdown engine
void iOS_HandleMemoryWarning(void); // Handle memory warnings
#endif

#endif // __APPLE__ && !__ANDROID__

#endif // MACOS_PLATFORM_H

