#ifdef __APPLE__
#if defined(TARGET_OS_IPHONE)

#import "ios_platform.h"
#import "../../common/qcommon.h"

void Sys_Init_iOS(void) {
	Com_Printf("iOS Platform: Stubs initialized\n");
}

void Sys_Shutdown_iOS(void) {
	Com_Printf("iOS Platform: Shutdown\n");
}

void Platform_SetupIOS(void* view) {
	(void)view;
}

#endif
#endif
