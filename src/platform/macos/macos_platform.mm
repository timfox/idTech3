#if defined(__APPLE__) && !defined(__ANDROID__)

#import "macos_platform.h"
#import "../../common/qcommon.h"
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

qboolean Platform_Init(void) {
	Com_Memset(&platformCaps, 0, sizeof(platformCaps));
	
	id<MTLDevice> device = MTLCreateSystemDefaultDevice();
	if (device) {
		platformCaps.supportsMetal = qtrue;
		
		if (@available(macOS 10.13, iOS 11.0, *)) {
			platformCaps.supportsMetal2 = qtrue;
			platformCaps.supportsArgumentBuffers = [device supportsFamily:MTLGPUFamilyApple1] ||
												   [device supportsFamily:MTLGPUFamilyMac1] ||
												   [device supportsFamily:MTLGPUFamilyMac2];
		}
		
		if (@available(macOS 13.0, iOS 16.0, *)) {
			platformCaps.supportsMetal3 = qtrue;
			platformCaps.supportsRayTracing = [device supportsRayTracing];
		}
		
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

void Platform_Shutdown(void) {
	Com_Memset(&platformCaps, 0, sizeof(platformCaps));
}

#ifdef TARGET_OS_IPHONE
void* Platform_CreateView(int width, int height) {
	UIView* view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, width, height)];
	view.backgroundColor = [UIColor blackColor];
	view.contentMode = UIViewContentModeScaleToFill;
	return (__bridge_retained void*)view;
}

void Platform_DestroyView(void* view) {
	if (view) {
		UIView* uiView = (__bridge_transfer UIView*)view;
		uiView = nil;
	}
}

void Platform_SetViewSize(void* view, int width, int height) {
	if (view) {
		UIView* uiView = (__bridge UIView*)view;
		uiView.frame = CGRectMake(0, 0, width, height);
	}
}

void Platform_GetViewSize(void* view, int* width, int* height) {
	if (view && width && height) {
		UIView* uiView = (__bridge UIView*)view;
		CGSize size = uiView.bounds.size;
		*width = (int)size.width;
		*height = (int)size.height;
	}
}

#else
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

void Platform_DestroyWindow(void* window) {
	if (window) {
		NSWindow* nsWindow = (__bridge_transfer NSWindow*)window;
		[nsWindow close];
		nsWindow = nil;
	}
}

void Platform_SetWindowSize(void* window, int width, int height) {
	if (window) {
		NSWindow* nsWindow = (__bridge NSWindow*)window;
		NSRect frame = [nsWindow frame];
		frame.size.width = width;
		frame.size.height = height;
		[nsWindow setFrame:frame display:YES];
	}
}

void Platform_GetWindowSize(void* window, int* width, int* height) {
	if (window && width && height) {
		NSWindow* nsWindow = (__bridge NSWindow*)window;
		NSRect frame = [nsWindow.contentView bounds];
		*width = (int)frame.size.width;
		*height = (int)frame.size.height;
	}
}

void Platform_SetWindowTitle(void* window, const char* title) {
	if (window && title) {
		NSWindow* nsWindow = (__bridge NSWindow*)window;
		[nsWindow setTitle:[NSString stringWithUTF8String:title]];
	}
}

qboolean Platform_IsWindowFullscreen(void* window) {
	if (window) {
		NSWindow* nsWindow = (__bridge NSWindow*)window;
		return ([nsWindow styleMask] & NSWindowStyleMaskFullScreen) ? qtrue : qfalse;
	}
	return qfalse;
}

void Platform_SetWindowFullscreen(void* window, qboolean fullscreen) {
	if (window) {
		NSWindow* nsWindow = (__bridge NSWindow*)window;
		[nsWindow toggleFullScreen:nil];
	}
}
#endif

void Platform_ProcessEvents(void) {
#ifdef TARGET_OS_IPHONE
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
	getcwd(basePath, sizeof(basePath));
	return basePath;
}

static char g_customHomeDir[MAX_OSPATH] = {0};

const char* Platform_GetUserPath(void) {
	if (g_customHomeDir[0]) {
		return g_customHomeDir;
	}
	static char userPath[MAX_OSPATH];
#ifdef TARGET_OS_IPHONE
	NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	if (paths && [paths count] > 0) {
		NSString* path = [paths objectAtIndex:0];
		Q_strncpyz(userPath, [path UTF8String], sizeof(userPath));
		return userPath;
	}
#else
	NSArray* paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
	if (paths && [paths count] > 0) {
		NSString* path = [paths objectAtIndex:0];
		Q_strncpyz(userPath, [path UTF8String], sizeof(userPath));
		return userPath;
	}
#endif
	Q_strncpyz(userPath, Platform_GetBasePath(), sizeof(userPath));
	return userPath;
}

void Platform_SetHomeDir(const char* homeDir) {
	if (homeDir && homeDir[0]) {
		Q_strncpyz(g_customHomeDir, homeDir, sizeof(g_customHomeDir));
		size_t len = strlen(g_customHomeDir);
		if (len > 0 && g_customHomeDir[len - 1] != '/') {
			Q_strcat(g_customHomeDir, sizeof(g_customHomeDir), "/");
		}
	} else {
		g_customHomeDir[0] = '\0';
	}
}

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
	Q_strncpyz(resourcePath, Platform_GetBasePath(), sizeof(resourcePath));
	return resourcePath;
}

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

int Platform_GetCPUCount(void) {
	return (int)[[NSProcessInfo processInfo] processorCount];
}

qboolean Platform_HasRetinaDisplay(void) {
#ifdef TARGET_OS_IPHONE
	return qtrue;
#else
	CGDirectDisplayID displayID = CGMainDisplayID();
	return CGDisplayPixelsWide(displayID) > CGDisplayBounds(displayID).size.width * 1.0f ? qtrue : qfalse;
#endif
}

#endif // __APPLE__ && !__ANDROID__
