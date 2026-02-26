/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Stub implementations for Android platform.
Provides minimal no-op implementations of platform functions that the
engine references. These will be replaced with real Android
implementations (NativeActivity, Vulkan surface, OpenSL ES audio, etc.)
as Android support matures.
===========================================================================
*/

#ifdef __ANDROID__

#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#include <android/log.h>
#include <stdlib.h>
#include <unistd.h>

#define TAG "idTech3"

/* ---- Core system functions ---- */

void NORETURN Sys_Quit( void ) {
	exit( 0 );
}

void NORETURN Sys_Error( const char *fmt, ... ) {
	va_list ap;
	char buf[1024];
	va_start( ap, fmt );
	Q_vsnprintf( buf, sizeof( buf ), fmt, ap );
	va_end( ap );
	__android_log_print( ANDROID_LOG_ERROR, TAG, "Sys_Error: %s", buf );
	exit( 1 );
}

void Sys_Print( const char *msg ) {
	__android_log_print( ANDROID_LOG_INFO, TAG, "%s", msg );
}

void Sys_Init( void ) {
	__android_log_print( ANDROID_LOG_INFO, TAG, "Sys_Init (Android stub)" );
}

const char *Sys_DefaultBasePath( void ) {
	return "/sdcard/idtech3";
}

qboolean Sys_LowPhysicalMemory( void ) {
	return qfalse;
}

void Sys_BeginProfiling( void ) {}

void Sys_ShowErrorMessage( const char *msg, const char *title ) {
	(void)title;
	__android_log_print( ANDROID_LOG_ERROR, TAG, "Error: %s", msg );
}

void Sys_SetStatus( const char *format, ... ) { (void)format; }

void Sys_SendKeyEvents( void ) {}

char *Sys_ConsoleInput( void ) { return NULL; }

void Sys_Sleep( int msec ) {
	if ( msec > 0 ) usleep( msec * 1000 );
}

/* ---- Window / display ---- */

void Sys_UpdateWindowTitle( const char *title ) { (void)title; }
char *Sys_GetClipboardData( void ) { return NULL; }
void Sys_SetClipboardBitmap( const byte *bitmap, int length ) {
	(void)bitmap; (void)length;
}

/* ---- Sound (stubs until OpenSL ES / AAudio) ---- */

qboolean SNDDMA_Init( int sampleFrequencyInKHz ) { (void)sampleFrequencyInKHz; return qfalse; }
void SNDDMA_Shutdown( void ) {}
void SNDDMA_BeginPainting( void ) {}
int  SNDDMA_GetDMAPos( void ) { return 0; }
void SNDDMA_Submit( void ) {}

/* ---- OpenGL gamma (not used with Vulkan-only) ---- */

void GLimp_InitGamma( void *config ) { (void)config; }
void GLimp_SetGamma( unsigned char *red, unsigned char *green, unsigned char *blue ) {
	(void)red; (void)green; (void)blue;
}

/* ---- Vulkan surface (stubs until proper Android VkSurface) ---- */

void VKimp_Init( void *config ) { (void)config; }
void VKimp_Shutdown( void ) {}

void *VK_GetInstanceProcAddr( void ) { return NULL; }
int   VK_CreateSurface( void *instance, void *surface ) {
	(void)instance; (void)surface;
	return 0;
}

/* ---- Navigation stubs (Recast not available on Android) ---- */

void Nav_Init( void ) {}
void Nav_Shutdown( void ) {}
int  Nav_BuildFromBSP( const char *mapName, void *params ) { (void)mapName; (void)params; return -1; }
void Nav_UpdateCrowd( int mesh, float dt ) { (void)mesh; (void)dt; }
void Nav_BSP_ClearGeometry( void ) {}
int  Nav_BSP_AddVertex( float x, float y, float z ) { (void)x; (void)y; (void)z; return 0; }
void Nav_BSP_AddTriangle( int v0, int v1, int v2 ) { (void)v0; (void)v1; (void)v2; }

#endif /* __ANDROID__ */
