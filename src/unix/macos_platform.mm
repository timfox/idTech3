#if defined(__APPLE__) && !defined(__ANDROID__)

#import "macos_platform.h"
#import "../common/qcommon.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#ifdef TARGET_OS_IPHONE
#import <GameController/GameController.h>
#endif

#ifdef TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

platformCaps_t platformCaps;

/*
================
Platform_Init
================
*/
qboolean Platform_Init(void) {
	Com_Memset(&platformCaps, 0, sizeof(platformCaps));
	
	// Check Metal support
	id<MTLDevice> device = MTLCreateSystemDefaultDevice();
	if (device) {
		platformCaps.supportsMetal = qtrue;
		
		// Check Metal 2.0+ features
		if (@available(macOS 10.13, iOS 11.0, *)) {
			platformCaps.supportsMetal2 = qtrue;
			platformCaps.supportsArgumentBuffers = [device supportsFamily:MTLGPUFamilyApple1] ||
												   [device supportsFamily:MTLGPUFamilyMac1] ||
												   [device supportsFamily:MTLGPUFamilyMac2];
		}
		
		// Check Metal 3.0+ features
		if (@available(macOS 13.0, iOS 16.0, *)) {
			platformCaps.supportsMetal3 = qtrue;
			platformCaps.supportsRayTracing = [device supportsRayTracing];
		}
		
		// Check for variable refresh rate (ProMotion on iOS, ProMotion on Mac)
		#ifdef TARGET_OS_IPHONE
		if (@available(iOS 15.0, *)) {
			platformCaps.supportsVariableRefreshRate = qtrue;
		}
		#else
		if (@available(macOS 12.0, *)) {
			platformCaps.supportsVariableRefreshRate = qtrue;
		}
		#endif
	}
	
	Com_Printf("Platform: Initialized\n");
	Com_Printf("Platform: Metal=%d Metal2=%d Metal3=%d\n",
		platformCaps.supportsMetal,
		platformCaps.supportsMetal2,
		platformCaps.supportsMetal3);
	
	return qtrue;
}

/*
================
Platform_Shutdown
================
*/
void Platform_Shutdown(void) {
	Com_Memset(&platformCaps, 0, sizeof(platformCaps));
}

#ifdef TARGET_OS_IPHONE

/*
================
Platform_CreateView
================
*/
void* Platform_CreateView(int width, int height) {
	UIView* view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, width, height)];
	view.backgroundColor = [UIColor blackColor];
	view.contentMode = UIViewContentModeScaleToFill;
	return (__bridge_retained void*)view;
}

/*
================
Platform_DestroyView
================
*/
void Platform_DestroyView(void* view) {
	if (view) {
		UIView* uiView = (__bridge_transfer UIView*)view;
		uiView = nil;
	}
}

/*
================
Platform_SetViewSize
================
*/
void Platform_SetViewSize(void* view, int width, int height) {
	if (view) {
		UIView* uiView = (__bridge UIView*)view;
		uiView.frame = CGRectMake(0, 0, width, height);
	}
}

/*
================
Platform_GetViewSize
================
*/
void Platform_GetViewSize(void* view, int* width, int* height) {
	if (view && width && height) {
		UIView* uiView = (__bridge UIView*)view;
		CGSize size = uiView.bounds.size;
		*width = (int)size.width;
		*height = (int)size.height;
	}
}

#else // macOS

/*
================
Platform_CreateWindow
================
*/
void* Platform_CreateWindow(const char* title, int width, int height, qboolean fullscreen) {
	NSWindow* window = [[NSWindow alloc]
		initWithContentRect:NSMakeRect(0, 0, width, height)
		styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
		backing:NSBackingStoreBuffered
		defer:NO];
	
	if (title) {
		[window setTitle:[NSString stringWithUTF8String:title]];
	}
	
	[window center];
	[window makeKeyAndOrderFront:nil];
	[window setAcceptsMouseMovedEvents:YES];
	
	return (__bridge_retained void*)window;
}

/*
================
Platform_DestroyWindow
================
*/
void Platform_DestroyWindow(void* window) {
	if (window) {
		NSWindow* nsWindow = (__bridge_transfer NSWindow*)window;
		[nsWindow close];
		nsWindow = nil;
	}
}

/*
================
Platform_SetWindowSize
================
*/
void Platform_SetWindowSize(void* window, int width, int height) {
	if (window) {
		NSWindow* nsWindow = (__bridge NSWindow*)window;
		NSRect frame = [nsWindow frame];
		frame.size.width = width;
		frame.size.height = height;
		[nsWindow setFrame:frame display:YES];
	}
}

/*
================
Platform_GetWindowSize
================
*/
void Platform_GetWindowSize(void* window, int* width, int* height) {
	if (window && width && height) {
		NSWindow* nsWindow = (__bridge NSWindow*)window;
		NSRect frame = [nsWindow.contentView bounds];
		*width = (int)frame.size.width;
		*height = (int)frame.size.height;
	}
}

/*
================
Platform_SetWindowTitle
================
*/
void Platform_SetWindowTitle(void* window, const char* title) {
	if (window && title) {
		NSWindow* nsWindow = (__bridge NSWindow*)window;
		[nsWindow setTitle:[NSString stringWithUTF8String:title]];
	}
}

/*
================
Platform_IsWindowFullscreen
================
*/
qboolean Platform_IsWindowFullscreen(void* window) {
	if (window) {
		NSWindow* nsWindow = (__bridge NSWindow*)window;
		return ([nsWindow styleMask] & NSWindowStyleMaskFullScreen) ? qtrue : qfalse;
	}
	return qfalse;
}

/*
================
Platform_SetWindowFullscreen
================
*/
void Platform_SetWindowFullscreen(void* window, qboolean fullscreen) {
	if (window) {
		NSWindow* nsWindow = (__bridge NSWindow*)window;
		[nsWindow toggleFullScreen:nil];
	}
}

#endif // TARGET_OS_IPHONE

/*
================
Platform_ProcessEvents
================
*/
void Platform_ProcessEvents(void) {
#ifdef TARGET_OS_IPHONE
	// iOS uses run loop, events are processed automatically
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, true);
#else
	@autoreleasepool {
		NSEvent* event;
		do {
			event = [NSApp nextEventMatchingMask:NSEventMaskAny
										untilDate:[NSDate distantPast]
										   inMode:NSDefaultRunLoopMode
										  dequeue:YES];
			if (event) {
				[NSApp sendEvent:event];
			}
		} while (event);
	}
#endif
}

/*
================
Platform_GetBasePath
================
*/
const char* Platform_GetBasePath(void) {
	static char basePath[MAX_OSPATH];
	
	NSBundle* bundle = [NSBundle mainBundle];
	if (bundle) {
		NSString* path = [bundle bundlePath];
		if (path) {
			Q_strncpyz(basePath, [path UTF8String], sizeof(basePath));
			return basePath;
		}
	}
	
	// Fallback to current directory
	getcwd(basePath, sizeof(basePath));
	return basePath;
}

/*
================
Platform_GetUserPath
================
*/
static char g_customHomeDir[MAX_OSPATH] = {0};

const char* Platform_GetUserPath(void) {
	// If custom home dir was set (iOS), use it
	if (g_customHomeDir[0]) {
		return g_customHomeDir;
	}
	
	static char userPath[MAX_OSPATH];
	
#ifdef TARGET_OS_IPHONE
	// iOS: Use Documents directory by default
	NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	if (paths && [paths count] > 0) {
		NSString* path = [paths objectAtIndex:0];
		Q_strncpyz(userPath, [path UTF8String], sizeof(userPath));
		return userPath;
	}
#else
	// macOS: Use Application Support directory
	NSArray* paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
	if (paths && [paths count] > 0) {
		NSString* path = [paths objectAtIndex:0];
		Q_strncpyz(userPath, [path UTF8String], sizeof(userPath));
		return userPath;
	}
#endif
	
	// Fallback
	Q_strncpyz(userPath, Platform_GetBasePath(), sizeof(userPath));
	return userPath;
}

/*
================
Platform_SetHomeDir
================
*/
void Platform_SetHomeDir(const char* homeDir) {
	if (homeDir && homeDir[0]) {
		Q_strncpyz(g_customHomeDir, homeDir, sizeof(g_customHomeDir));
		// Ensure trailing slash
		size_t len = strlen(g_customHomeDir);
		if (len > 0 && g_customHomeDir[len - 1] != '/') {
			Q_strcat(g_customHomeDir, sizeof(g_customHomeDir), "/");
		}
	} else {
		g_customHomeDir[0] = '\0';
	}
}

/*
================
Platform_GetResourcePath
================
*/
const char* Platform_GetResourcePath(void) {
	static char resourcePath[MAX_OSPATH];
	
	NSBundle* bundle = [NSBundle mainBundle];
	if (bundle) {
		NSString* path = [bundle resourcePath];
		if (path) {
			Q_strncpyz(resourcePath, [path UTF8String], sizeof(resourcePath));
			return resourcePath;
		}
	}
	
	// Fallback
	Q_strncpyz(resourcePath, Platform_GetBasePath(), sizeof(resourcePath));
	return resourcePath;
}

/*
================
Platform_GetSystemInfo
================
*/
void Platform_GetSystemInfo(char* info, int maxlen) {
	if (!info || maxlen <= 0) {
		return;
	}
	
	NSMutableString* sysInfo = [NSMutableString string];
	
#ifdef TARGET_OS_IPHONE
	UIDevice* device = [UIDevice currentDevice];
	if (device) {
		[sysInfo appendFormat:@"iOS %@", [device systemVersion]];
		[sysInfo appendFormat:@" - %@", [device model]];
	}
#else
	NSProcessInfo* procInfo = [NSProcessInfo processInfo];
	if (procInfo) {
		[sysInfo appendFormat:@"macOS %@", procInfo.operatingSystemVersionString];
	}
#endif
	
	if ([sysInfo length] > 0) {
		Q_strncpyz(info, [sysInfo UTF8String], maxlen);
	} else {
		Q_strncpyz(info, "Apple Platform", maxlen);
	}
}

/*
================
Platform_GetCPUCount
================
*/
int Platform_GetCPUCount(void) {
	return (int)[[NSProcessInfo processInfo] processorCount];
}

/*
================
Platform_HasRetinaDisplay
================
*/
qboolean Platform_HasRetinaDisplay(void) {
#ifdef TARGET_OS_IPHONE
	return qtrue; // All modern iOS devices have Retina displays
#else
	NSScreen* mainScreen = [NSScreen mainScreen];
	if (mainScreen) {
		return ([mainScreen backingScaleFactor] > 1.0) ? qtrue : qfalse;
	}
	return qfalse;
#endif
}

#ifdef TARGET_OS_IPHONE

/*
================
iOS_ConfigureScreenResolution
================
Configure screen resolution cvars for iOS, accounting for Retina display scale
Note: This should be called after Com_Init() so cvars are available
*/
void iOS_ConfigureScreenResolution(void) {
	UIScreen* screen = [UIScreen mainScreen];
	if (!screen) {
		return;
	}
	
	CGRect bounds = screen.bounds;
	CGFloat scale = screen.scale;
	
	// Calculate actual pixel dimensions
	int width = (int)(bounds.size.width * scale);
	int height = (int)(bounds.size.height * scale);
	
	// Set cvars (these need to be set before renderer initialization)
	// Note: Cvar functions are declared in qcommon.h
	extern void Cvar_Set(const char* var_name, const char* value);
	extern void Cvar_SetValue(const char* var_name, float value);
	
	// Set custom resolution mode
	Cvar_Set("r_mode", "-1");
	Cvar_SetValue("r_customwidth", width);
	Cvar_SetValue("r_customheight", height);
	Cvar_Set("r_useHiDPI", "1");
	Cvar_Set("r_fullscreen", "1");
	
	Com_Printf("iOS: Configured screen resolution: %dx%d (scale: %.1f)\n", width, height, scale);
}

/*
================
iOS_SetupControllers
================
Setup GameController framework for iOS
Note: This should be called after Com_Init() so cvars are available
*/
void iOS_SetupControllers(void) {
	// Enable joystick support
	extern void Cvar_Set(const char* var_name, const char* value);
	Cvar_Set("in_joystick", "1");
	Cvar_Set("in_joystickUseAnalog", "1");
	
	// Listen for controller connections
	[[NSNotificationCenter defaultCenter]
		addObserverForName:GCControllerDidConnectNotification
		object:nil
		queue:[NSOperationQueue mainQueue]
		usingBlock:^(NSNotification* note) {
			GCController* controller = note.object;
			if (controller && controller.vendorName) {
				Com_Printf("iOS: Game controller connected: %s\n", [controller.vendorName UTF8String]);
			} else {
				Com_Printf("iOS: Game controller connected\n");
			}
		}];
	
	[[NSNotificationCenter defaultCenter]
		addObserverForName:GCControllerDidDisconnectNotification
		object:nil
		queue:[NSOperationQueue mainQueue]
		usingBlock:^(NSNotification* note) {
			Com_Printf("iOS: Game controller disconnected\n");
		}];
	
	Com_Printf("iOS: GameController framework initialized\n");
}

/*
================
iOS_InitializeEngine
================
Initialize engine with iOS-specific setup
*/
void iOS_InitializeEngine(void* view) {
	if (!view) {
		Com_Error(ERR_FATAL, "iOS: No view provided for engine initialization");
		return;
	}
	
	// Set up file system paths BEFORE engine initialization
	NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	if (paths && [paths count] > 0) {
		NSString* documentsDir = [paths objectAtIndex:0];
		Platform_SetHomeDir([documentsDir UTF8String]);
		// Use printf here since Com_Printf may not be available yet
		printf("iOS: Set home directory to: %s\n", [documentsDir UTF8String]);
	}
	
	// Initialize engine first (this sets up cvars and other systems)
	extern void Com_Init(const char* commandLine);
	Com_Init("");
	
	// Now configure screen resolution (cvars are now available)
	iOS_ConfigureScreenResolution();
	
	// Setup controllers (cvars are now available)
	iOS_SetupControllers();
	
	// Initialize Metal renderer with view
	extern qboolean Metal_InitWindow(void* windowOrView);
	if (!Metal_InitWindow(view)) {
		Com_Error(ERR_FATAL, "iOS: Failed to initialize Metal renderer");
		return;
	}
	
	Com_Printf("iOS: Engine initialized successfully\n");
}

/*
================
iOS_ShutdownEngine
================
Shutdown engine and cleanup
*/
void iOS_ShutdownEngine(void) {
	extern void Metal_ShutdownWindow(void);
	Metal_ShutdownWindow();
	
	extern void Com_Shutdown(void);
	Com_Shutdown();
	
	Com_Printf("iOS: Engine shutdown complete\n");
}

/*
================
iOS_HandleMemoryWarning
================
Handle iOS memory warnings
*/
void iOS_HandleMemoryWarning(void) {
	Com_Printf("iOS: Memory warning received\n");
	
	// Clear any caches, reduce quality settings, etc.
	extern void Cvar_Set(const char* var_name, const char* value);
	Cvar_Set("r_picmip", "2"); // Reduce texture quality
	Cvar_Set("r_subdivisions", "4"); // Reduce model detail
	
	Com_Printf("iOS: Reduced quality settings due to memory warning\n");
}

#endif // TARGET_OS_IPHONE

#endif // __APPLE__ && !__ANDROID__

