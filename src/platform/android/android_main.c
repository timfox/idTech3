/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Android platform layer — NativeActivity lifecycle, game thread,
Vulkan surface, input, file system, and JNI bridge.
===========================================================================
*/

#ifdef __ANDROID__

#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/input.h>
#include <android/asset_manager.h>
#include <android/configuration.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include <time.h>
#include <jni.h>

#define TAG "idTech3"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

/* ---- Global state ---- */

static ANativeWindow    *g_window = NULL;
static ANativeActivity  *g_activity = NULL;
static AAssetManager    *g_assetManager = NULL;
static volatile int     g_running = 0;
static volatile int     g_paused = 0;
static volatile int     g_windowReady = 0;
static pthread_t        g_gameThread;
static void             *g_vulkanLib = NULL;
static char             g_dataPath[MAX_OSPATH] = "/sdcard/idtech3";
static char             g_homePath[MAX_OSPATH] = "";
static int              g_windowWidth = 1280;
static int              g_windowHeight = 720;
static int              g_engineInitialized = 0;

/* ---- Sys functions ---- */

int Sys_Milliseconds( void ) {
	struct timespec ts;
	static long base = 0;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	long ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
	if ( !base ) base = ms;
	return (int)( ms - base );
}

int64_t Sys_Microseconds( void ) {
	struct timespec ts;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

void NORETURN Sys_Quit( void ) {
	g_running = 0;
	LOGI( "Sys_Quit" );
	exit( 0 );
}

void NORETURN Sys_Error( const char *fmt, ... ) {
	va_list ap;
	char buf[1024];
	va_start( ap, fmt );
	Q_vsnprintf( buf, sizeof( buf ), fmt, ap );
	va_end( ap );
	LOGE( "Sys_Error: %s", buf );
	g_running = 0;
	exit( 1 );
}

void Sys_Print( const char *msg ) {
	LOGI( "%s", msg );
}

void Sys_Init( void ) {
	LOGI( "Sys_Init (Android)" );
}

const char *Sys_DefaultBasePath( void ) {
	return g_dataPath;
}

const char *Sys_DefaultHomePath( void ) {
	return g_homePath[0] ? g_homePath : g_dataPath;
}

qboolean Sys_LowPhysicalMemory( void ) { return qfalse; }
void Sys_BeginProfiling( void ) {}
void Sys_ShowErrorMessage( const char *msg, const char *title ) {
	(void)title;
	LOGE( "Error: %s", msg );
}
void Sys_SetStatus( const char *format, ... ) { (void)format; }
char *Sys_ConsoleInput( void ) { return NULL; }
void Sys_Sleep( int msec ) { if ( msec > 0 ) usleep( (unsigned)msec * 1000 ); }
void Sys_UpdateWindowTitle( const char *title ) { (void)title; }
char *Sys_GetClipboardData( void ) { return NULL; }
void Sys_SetClipboardBitmap( const byte *bitmap, int length ) { (void)bitmap; (void)length; }

void Sys_SendKeyEvents( void ) {
	/* Events are queued from the input callback on the main thread;
	   Com_Frame processes them via the event queue. */
}

/* ---- Vulkan surface ---- */

typedef void *VkInstance;
typedef void *VkSurfaceKHR;
typedef unsigned int VkFlags;
typedef unsigned int VkResult;

typedef struct {
	unsigned int sType;
	const void *pNext;
	VkFlags flags;
	ANativeWindow *window;
} VkAndroidSurfaceCreateInfoKHR;

typedef VkResult (*PFN_vkCreateAndroidSurfaceKHR)(
	VkInstance, const VkAndroidSurfaceCreateInfoKHR *, const void *, VkSurfaceKHR * );

void VKimp_Init( void *config ) {
	if ( !g_vulkanLib ) {
		g_vulkanLib = dlopen( "libvulkan.so", RTLD_NOW | RTLD_LOCAL );
	}

	if ( config && g_window ) {
		/* Set glconfig dimensions from the Android window */
		typedef struct { int vidWidth; int vidHeight; } minConfig_t;
		minConfig_t *gc = (minConfig_t *)config;
		gc->vidWidth = g_windowWidth;
		gc->vidHeight = g_windowHeight;
	}

	LOGI( "VKimp_Init: Vulkan %s, window %dx%d",
		g_vulkanLib ? "loaded" : "FAILED", g_windowWidth, g_windowHeight );
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
		LOGE( "VK_CreateSurface: vkCreateAndroidSurfaceKHR not found" );
		return 0;
	}

	memset( &ci, 0, sizeof( ci ) );
	ci.sType = 1000008000; /* VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR */
	ci.window = g_window;

	if ( createFunc( (VkInstance)instance, &ci, NULL, (VkSurfaceKHR *)pSurface ) != 0 ) {
		LOGE( "VK_CreateSurface: failed" );
		return 0;
	}

	LOGI( "VK_CreateSurface: success" );
	return 1;
}

/* ---- OpenGL stubs (Vulkan-only on Android) ---- */

void GLimp_InitGamma( void *config ) { (void)config; }
void GLimp_SetGamma( unsigned char *r, unsigned char *g, unsigned char *b ) {
	(void)r; (void)g; (void)b;
}

/* ---- Sound stubs (until OpenSL ES / Oboe) ---- */

qboolean SNDDMA_Init( int freq ) { (void)freq; return qfalse; }
void SNDDMA_Shutdown( void ) {}
void SNDDMA_BeginPainting( void ) {}
int  SNDDMA_GetDMAPos( void ) { return 0; }
void SNDDMA_Submit( void ) {}

/* ---- Navigation stubs ---- */

void Nav_Init( void ) {}
void Nav_Shutdown( void ) {}
int  Nav_BuildFromBSP( const char *m, void *p ) { (void)m; (void)p; return -1; }
void Nav_UpdateCrowd( int h, float dt ) { (void)h; (void)dt; }
void Nav_BSP_ClearGeometry( void ) {}
int  Nav_BSP_AddVertex( float x, float y, float z ) { (void)x; (void)y; (void)z; return 0; }
void Nav_BSP_AddTriangle( int a, int b, int c ) { (void)a; (void)b; (void)c; }

/* ---- Input handling ---- */

static int32_t onInputEvent( ANativeActivity *activity, AInputEvent *event ) {
	(void)activity;
	int32_t type = AInputEvent_getType( event );
	int32_t source = AInputEvent_getSource( event );

	if ( type == AINPUT_EVENT_TYPE_MOTION ) {
		int32_t action = AMotionEvent_getAction( event ) & AMOTION_EVENT_ACTION_MASK;
		float x = AMotionEvent_getX( event, 0 );
		float y = AMotionEvent_getY( event, 0 );

		/* Gamepad joystick */
		if ( source & AINPUT_SOURCE_JOYSTICK ) {
			float lx = AMotionEvent_getAxisValue( event, AMOTION_EVENT_AXIS_X, 0 );
			float ly = AMotionEvent_getAxisValue( event, AMOTION_EVENT_AXIS_Y, 0 );
			float rx = AMotionEvent_getAxisValue( event, AMOTION_EVENT_AXIS_Z, 0 );
			float ry = AMotionEvent_getAxisValue( event, AMOTION_EVENT_AXIS_RZ, 0 );
			Com_QueueEvent( Sys_Milliseconds(), SE_JOYSTICK_AXIS, 0, (int)(lx * 127), 0, NULL );
			Com_QueueEvent( Sys_Milliseconds(), SE_JOYSTICK_AXIS, 1, (int)(ly * 127), 0, NULL );
			Com_QueueEvent( Sys_Milliseconds(), SE_JOYSTICK_AXIS, 2, (int)(rx * 127), 0, NULL );
			Com_QueueEvent( Sys_Milliseconds(), SE_JOYSTICK_AXIS, 3, (int)(ry * 127), 0, NULL );
			return 1;
		}

		/* Touch input → mouse */
		if ( action == AMOTION_EVENT_ACTION_DOWN ) {
			Com_QueueEvent( Sys_Milliseconds(), SE_KEY, K_MOUSE1, qtrue, 0, NULL );
		} else if ( action == AMOTION_EVENT_ACTION_UP ) {
			Com_QueueEvent( Sys_Milliseconds(), SE_KEY, K_MOUSE1, qfalse, 0, NULL );
		}
		Com_QueueEvent( Sys_Milliseconds(), SE_MOUSE, (int)x, (int)y, 0, NULL );
		return 1;
	}

	if ( type == AINPUT_EVENT_TYPE_KEY ) {
		int32_t keyCode = AKeyEvent_getKeyCode( event );
		int32_t action = AKeyEvent_getAction( event );
		qboolean down = ( action == AKEY_EVENT_ACTION_DOWN ) ? qtrue : qfalse;
		int q3key = 0;

		switch ( keyCode ) {
			case AKEYCODE_BACK:         q3key = K_ESCAPE; break;
			case AKEYCODE_MENU:         q3key = K_ESCAPE; break;
			case AKEYCODE_VOLUME_UP:    q3key = K_UPARROW; break;
			case AKEYCODE_VOLUME_DOWN:  q3key = K_DOWNARROW; break;
			case AKEYCODE_DPAD_UP:      q3key = K_UPARROW; break;
			case AKEYCODE_DPAD_DOWN:    q3key = K_DOWNARROW; break;
			case AKEYCODE_DPAD_LEFT:    q3key = K_LEFTARROW; break;
			case AKEYCODE_DPAD_RIGHT:   q3key = K_RIGHTARROW; break;
			case AKEYCODE_DPAD_CENTER:  q3key = K_ENTER; break;
			case AKEYCODE_ENTER:        q3key = K_ENTER; break;
			case AKEYCODE_SPACE:        q3key = K_SPACE; break;
			case AKEYCODE_TAB:          q3key = K_TAB; break;
			case AKEYCODE_BUTTON_A:     q3key = K_JOY1; break;
			case AKEYCODE_BUTTON_B:     q3key = K_JOY2; break;
			case AKEYCODE_BUTTON_X:     q3key = K_JOY3; break;
			case AKEYCODE_BUTTON_Y:     q3key = K_JOY4; break;
			case AKEYCODE_BUTTON_L1:    q3key = K_JOY5; break;
			case AKEYCODE_BUTTON_R1:    q3key = K_JOY6; break;
			case AKEYCODE_BUTTON_START: q3key = K_ENTER; break;
			default:
				if ( keyCode >= AKEYCODE_A && keyCode <= AKEYCODE_Z ) {
					q3key = 'a' + ( keyCode - AKEYCODE_A );
				} else if ( keyCode >= AKEYCODE_0 && keyCode <= AKEYCODE_9 ) {
					q3key = '0' + ( keyCode - AKEYCODE_0 );
				}
				break;
		}

		if ( q3key ) {
			Com_QueueEvent( Sys_Milliseconds(), SE_KEY, q3key, down, 0, NULL );
			return 1;
		}
	}

	return 0;
}

/* ---- Game thread ---- */

static void *gameThreadFunc( void *arg ) {
	(void)arg;
	LOGI( "Game thread started" );

	/* Wait for window */
	while ( g_running && !g_windowReady ) {
		usleep( 10000 );
	}

	if ( !g_running ) {
		LOGI( "Game thread: exiting (no window)" );
		return NULL;
	}

	/* Initialize engine */
	char cmdLine[256];
	Com_sprintf( cmdLine, sizeof( cmdLine ),
		"+set fs_basepath \"%s\" +set fs_homepath \"%s\" +set r_mode -1 +set r_customwidth %d +set r_customheight %d",
		g_dataPath, g_homePath[0] ? g_homePath : g_dataPath,
		g_windowWidth, g_windowHeight );

	Com_Init( cmdLine );
	g_engineInitialized = 1;
	LOGI( "Engine initialized" );

	/* Main loop */
	int lastTime = Sys_Milliseconds();
	while ( g_running ) {
		if ( g_paused ) {
			usleep( 50000 );
			lastTime = Sys_Milliseconds();
			continue;
		}

		int newTime = Sys_Milliseconds();
		int msec = newTime - lastTime;
		lastTime = newTime;

		if ( msec < 1 ) msec = 1;
		if ( msec > 200 ) msec = 200;

		Com_Frame( msec );
	}

	LOGI( "Game thread: shutting down" );
	Com_Shutdown();
	return NULL;
}

/* ---- NativeActivity callbacks ---- */

static void onNativeWindowCreated( ANativeActivity *activity, ANativeWindow *window ) {
	(void)activity;
	g_window = window;
	g_windowWidth = ANativeWindow_getWidth( window );
	g_windowHeight = ANativeWindow_getHeight( window );
	g_windowReady = 1;
	LOGI( "Window created: %dx%d", g_windowWidth, g_windowHeight );
}

static void onNativeWindowDestroyed( ANativeActivity *activity, ANativeWindow *window ) {
	(void)activity; (void)window;
	g_windowReady = 0;
	g_window = NULL;
	LOGI( "Window destroyed" );
}

static void onNativeWindowResized( ANativeActivity *activity, ANativeWindow *window ) {
	(void)activity;
	g_windowWidth = ANativeWindow_getWidth( window );
	g_windowHeight = ANativeWindow_getHeight( window );
	LOGI( "Window resized: %dx%d", g_windowWidth, g_windowHeight );
}

static void onDestroy( ANativeActivity *activity ) {
	(void)activity;
	g_running = 0;
	pthread_join( g_gameThread, NULL );
	LOGI( "Activity destroyed" );
}

static void onPause( ANativeActivity *activity ) {
	(void)activity;
	g_paused = 1;
	LOGI( "Paused" );
}

static void onResume( ANativeActivity *activity ) {
	(void)activity;
	g_paused = 0;
	LOGI( "Resumed" );
}

/* ---- JNI bridge ---- */

JNIEXPORT void JNICALL Java_com_gopex_idtech3_GameActivity_nativeSetDataPath(
	JNIEnv *env, jobject obj, jstring path )
{
	(void)obj;
	const char *p = (*env)->GetStringUTFChars( env, path, NULL );
	if ( p ) {
		Q_strncpyz( g_dataPath, p, sizeof( g_dataPath ) );
		(*env)->ReleaseStringUTFChars( env, path, p );
		LOGI( "Data path set: %s", g_dataPath );
	}
}

JNIEXPORT void JNICALL Java_com_gopex_idtech3_GameActivity_nativeSetHomePath(
	JNIEnv *env, jobject obj, jstring path )
{
	(void)obj;
	const char *p = (*env)->GetStringUTFChars( env, path, NULL );
	if ( p ) {
		Q_strncpyz( g_homePath, p, sizeof( g_homePath ) );
		(*env)->ReleaseStringUTFChars( env, path, p );
		LOGI( "Home path set: %s", g_homePath );
	}
}

/* ---- Entry point ---- */

void ANativeActivity_onCreate( ANativeActivity *activity, void *savedState, size_t savedStateSize ) {
	(void)savedState;
	(void)savedStateSize;

	g_activity = activity;
	g_assetManager = activity->assetManager;

	/* Set default data path from internal storage */
	if ( activity->internalDataPath ) {
		Q_strncpyz( g_dataPath, activity->internalDataPath, sizeof( g_dataPath ) );
	}
	if ( activity->externalDataPath ) {
		Q_strncpyz( g_homePath, activity->externalDataPath, sizeof( g_homePath ) );
	}

	activity->callbacks->onDestroy = onDestroy;
	activity->callbacks->onPause = onPause;
	activity->callbacks->onResume = onResume;
	activity->callbacks->onNativeWindowCreated = onNativeWindowCreated;
	activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
	activity->callbacks->onNativeWindowResized = onNativeWindowResized;
	activity->callbacks->onInputEvent = onInputEvent;

	LOGI( "ANativeActivity_onCreate: id Tech 3 starting" );
	LOGI( "  Internal: %s", activity->internalDataPath ? activity->internalDataPath : "null" );
	LOGI( "  External: %s", activity->externalDataPath ? activity->externalDataPath : "null" );

	g_running = 1;
	g_paused = 0;
	g_engineInitialized = 0;

	pthread_create( &g_gameThread, NULL, gameThreadFunc, NULL );
}

#endif /* __ANDROID__ */
