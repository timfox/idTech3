/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Android platform layer using NativeActivity.
Provides Vulkan surface creation, input handling, and lifecycle management.
===========================================================================
*/

#ifdef __ANDROID__

#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/input.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

#define TAG "idTech3"

static ANativeWindow    *g_window = NULL;
static ANativeActivity  *g_activity = NULL;
static qboolean         g_running = qfalse;
static void             *g_vulkanLib = NULL;

/* ---- Vulkan surface creation ---- */

typedef void *VkInstance;
typedef void *VkSurfaceKHR;
typedef unsigned int VkFlags;
typedef unsigned int VkResult;

typedef struct {
	unsigned int sType;
	const void  *pNext;
	VkFlags     flags;
	ANativeWindow *window;
} VkAndroidSurfaceCreateInfoKHR;

typedef VkResult (*PFN_vkCreateAndroidSurfaceKHR)(
	VkInstance instance,
	const VkAndroidSurfaceCreateInfoKHR *pCreateInfo,
	const void *pAllocator,
	VkSurfaceKHR *pSurface );

void VKimp_Init( void *config ) {
	(void)config;
	if ( !g_vulkanLib ) {
		g_vulkanLib = dlopen( "libvulkan.so", RTLD_NOW | RTLD_LOCAL );
		if ( !g_vulkanLib ) {
			__android_log_print( ANDROID_LOG_ERROR, TAG, "VKimp_Init: failed to load libvulkan.so" );
		}
	}
	__android_log_print( ANDROID_LOG_INFO, TAG, "VKimp_Init: Vulkan library %s",
		g_vulkanLib ? "loaded" : "NOT available" );
}

void VKimp_Shutdown( void ) {
	if ( g_vulkanLib ) {
		dlclose( g_vulkanLib );
		g_vulkanLib = NULL;
	}
}

void *VK_GetInstanceProcAddr( void ) {
	if ( !g_vulkanLib ) return NULL;
	return dlsym( g_vulkanLib, "vkGetInstanceProcAddr" );
}

int VK_CreateSurface( void *instance, void *pSurface ) {
	PFN_vkCreateAndroidSurfaceKHR createFunc;
	VkAndroidSurfaceCreateInfoKHR ci;

	if ( !g_vulkanLib || !g_window || !instance ) return 0;

	createFunc = (PFN_vkCreateAndroidSurfaceKHR)dlsym( g_vulkanLib, "vkCreateAndroidSurfaceKHR" );
	if ( !createFunc ) {
		__android_log_print( ANDROID_LOG_ERROR, TAG, "VK_CreateSurface: vkCreateAndroidSurfaceKHR not found" );
		return 0;
	}

	memset( &ci, 0, sizeof( ci ) );
	ci.sType = 1000008000; /* VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR */
	ci.window = g_window;

	if ( createFunc( (VkInstance)instance, &ci, NULL, (VkSurfaceKHR *)pSurface ) != 0 ) {
		__android_log_print( ANDROID_LOG_ERROR, TAG, "VK_CreateSurface: vkCreateAndroidSurfaceKHR failed" );
		return 0;
	}

	__android_log_print( ANDROID_LOG_INFO, TAG, "VK_CreateSurface: Android Vulkan surface created" );
	return 1;
}

/* ---- NativeActivity callbacks ---- */

static void onNativeWindowCreated( ANativeActivity *activity, ANativeWindow *window ) {
	(void)activity;
	g_window = window;
	__android_log_print( ANDROID_LOG_INFO, TAG, "Window created: %dx%d",
		ANativeWindow_getWidth( window ), ANativeWindow_getHeight( window ) );
}

static void onNativeWindowDestroyed( ANativeActivity *activity, ANativeWindow *window ) {
	(void)activity; (void)window;
	g_window = NULL;
	__android_log_print( ANDROID_LOG_INFO, TAG, "Window destroyed" );
}

static void onNativeWindowResized( ANativeActivity *activity, ANativeWindow *window ) {
	(void)activity;
	__android_log_print( ANDROID_LOG_INFO, TAG, "Window resized: %dx%d",
		ANativeWindow_getWidth( window ), ANativeWindow_getHeight( window ) );
}

static void onDestroy( ANativeActivity *activity ) {
	(void)activity;
	g_running = qfalse;
	__android_log_print( ANDROID_LOG_INFO, TAG, "Activity destroyed" );
}

static void onPause( ANativeActivity *activity ) {
	(void)activity;
	__android_log_print( ANDROID_LOG_INFO, TAG, "Activity paused" );
}

static void onResume( ANativeActivity *activity ) {
	(void)activity;
	__android_log_print( ANDROID_LOG_INFO, TAG, "Activity resumed" );
}

static int32_t onInputEvent( ANativeActivity *activity, AInputEvent *event ) {
	(void)activity;
	int32_t type = AInputEvent_getType( event );
	(void)type;
	return 0;
}

/* ---- Entry point ---- */

void ANativeActivity_onCreate( ANativeActivity *activity, void *savedState, size_t savedStateSize ) {
	(void)savedState;
	(void)savedStateSize;

	g_activity = activity;

	activity->callbacks->onDestroy = onDestroy;
	activity->callbacks->onPause = onPause;
	activity->callbacks->onResume = onResume;
	activity->callbacks->onNativeWindowCreated = onNativeWindowCreated;
	activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
	activity->callbacks->onNativeWindowResized = onNativeWindowResized;
	activity->callbacks->onInputEvent = onInputEvent;

	__android_log_print( ANDROID_LOG_INFO, TAG,
		"ANativeActivity_onCreate: id Tech 3 engine starting" );

	g_running = qtrue;
}

/* ---- Platform stubs still needed ---- */

void GLimp_InitGamma( void *config ) { (void)config; }
void GLimp_SetGamma( unsigned char *r, unsigned char *g, unsigned char *b ) {
	(void)r; (void)g; (void)b;
}

#endif /* __ANDROID__ */
