#if defined(__APPLE__) && !defined(__ANDROID__) && !defined(TARGET_OS_IPHONE)

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import "../qcommon/qcommon.h"
#import "macos_platform.h"

// macOS-specific main function
int main(int argc, char *argv[]) {
	@autoreleasepool {
		// Initialize NSApplication
		NSApplication* app = [NSApplication sharedApplication];
		[app setActivationPolicy:NSApplicationActivationPolicyRegular];
		
	// Initialize platform
	if (!Platform_Init()) {
		return 1;
	}
	
	// Set up file system paths before engine init
	extern void Sys_Init_Apple(void);
	Sys_Init_Apple();
	
	// Initialize engine
	Com_Init(argc > 1 ? argv[1] : "");
	
	// Create main window after engine init (renderer will attach)
	void* window = Platform_CreateWindow("id Tech 3", 1024, 768, qfalse);
	if (!window) {
		Com_Printf("macOS: Failed to create window\n");
		return 1;
	}
	
	// Initialize Metal renderer with window
	extern qboolean Metal_InitWindow(void* windowOrView);
	if (!Metal_InitWindow(window)) {
		Com_Printf("macOS: Failed to initialize Metal renderer\n");
		return 1;
	}
	
	// Run main loop
	[app run];
	
	// Cleanup
	extern void Sys_Shutdown_Apple(void);
	Metal_ShutdownWindow();
	Platform_DestroyWindow(window);
	Sys_Shutdown_Apple();
	Com_Shutdown();
	
	return 0;
	}
}

#endif // __APPLE__ && !__ANDROID__ && !TARGET_OS_IPHONE

