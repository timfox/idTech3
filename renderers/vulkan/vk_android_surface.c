/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Android: recycle VkSurfaceKHR when ANativeWindow is destroyed/recreated.
Coordinates with src/platform/android/android_main.c via pthread cond.
===========================================================================
*/

#include "vk_android_surface.h"

#ifdef __ANDROID__

#include "tr_local.h"
#include "vk.h"
#include "vk_instance.h"
#include "platform/android/android_surface_glue.h"
#include "qcommon/qcommon.h"

void vk_Android_OnNativeWindowGoingAway( void )
{
	if ( vk.device == VK_NULL_HANDLE || qvkDestroySurfaceKHR == NULL ) {
		return;
	}

	vk_teardown_presentation_targets();

	if ( vk_surface != VK_NULL_HANDLE ) {
		qvkDestroySurfaceKHR( vk_instance, vk_surface, NULL );
		vk_surface = VK_NULL_HANDLE;
	}

	gw_minimized = qtrue;
}

void vk_Android_OnNativeWindowReady( void )
{
	if ( vk_instance == VK_NULL_HANDLE || vk.device == VK_NULL_HANDLE ) {
		return;
	}

	if ( vk_surface != VK_NULL_HANDLE ) {
		/* Should not happen; recover by destroying stale surface */
		qvkDestroySurfaceKHR( vk_instance, vk_surface, NULL );
		vk_surface = VK_NULL_HANDLE;
	}

	if ( !ri.VK_CreateSurface( vk_instance, &vk_surface ) ) {
		ri.Printf( PRINT_ERROR, "Vulkan: failed to recreate Android surface\n" );
		return;
	}

	{
		int w = 0, h = 0;
		Android_NativeWindowSize( &w, &h );
		if ( w > 0 && h > 0 ) {
			glConfig.vidWidth = w;
			glConfig.vidHeight = h;
			gls.windowWidth = w;
			gls.windowHeight = h;
			gls.captureWidth = w;
			gls.captureHeight = h;
			ri.CL_SetScaling( 1.0f, w, h );
		}
	}

	vk_restore_presentation_targets();

	gw_minimized = qfalse;

	if ( Com_LogVerbosity() >= 1 ) {
		ri.Printf( PRINT_ALL, "Android: Vulkan surface restored (%d x %d)\n",
			glConfig.vidWidth, glConfig.vidHeight );
	}
}

#else /* !__ANDROID__ */

void vk_Android_OnNativeWindowGoingAway( void ) {}
void vk_Android_OnNativeWindowReady( void ) {}

#endif
