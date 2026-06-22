/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

JNI/NativeActivity surface state for coordination with the Vulkan renderer.
===========================================================================
*/

#ifndef ANDROID_SURFACE_GLUE_H
#define ANDROID_SURFACE_GLUE_H

#include "../../renderers/common/tr_types.h"

#ifdef __ANDROID__

qboolean Android_NativeWindowReady( void );
void Android_NativeWindowSize( int *width, int *height );

/* Game thread: process surface teardown/recreate requested from NativeActivity callbacks. */
void Android_SurfaceThread_ProcessPending( void );

#else

static inline qboolean Android_NativeWindowReady( void ) {
	return qfalse;
}
static inline void Android_NativeWindowSize( int *width, int *height ) {
	(void)width;
	(void)height;
}
static inline void Android_SurfaceThread_ProcessPending( void ) {}

#endif

#endif
