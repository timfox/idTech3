#ifndef IOS_PLATFORM_H
#define IOS_PLATFORM_H

#ifdef __APPLE__
#if defined(TARGET_OS_IPHONE)

#include "../../common/q_shared.h"

void Sys_Init_iOS(void);
void Sys_Shutdown_iOS(void);
void Platform_SetupIOS(void* view);

#endif
#endif

#endif // IOS_PLATFORM_H
