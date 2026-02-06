#if defined(__APPLE__) && !defined(__ANDROID__) && !defined(TARGET_OS_IPHONE)

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import "../../common/qcommon.h"
#import "macos_platform.h"

extern void Com_Init(const char* initString);
extern void Com_Shutdown(void);
int main(int argc, char *argv[]) {
	@autoreleasepool {
		NSApplication* app = [NSApplication sharedApplication];
		[app setActivationPolicy:NSApplicationActivationPolicyRegular];

		if (!Platform_Init()) {
			return 1;
		}

		extern void Sys_Init_Apple(void);
		Sys_Init_Apple();

		const char* cmdline = (argc > 1) ? argv[1] : "";
		Com_Init(cmdline);

		void* window = Platform_CreateWindow("id Tech 3", 1280, 720, qfalse);
		if (!window) {
			Com_Printf("macOS: Failed to create window\n");
			return 1;
		}

		[app run];

		Platform_DestroyWindow(window);
		Sys_Shutdown_Apple();
		Com_Shutdown();

		return 0;
	}
}

#endif
