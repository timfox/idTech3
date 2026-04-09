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
#include "../../renderers/common/tr_types.h"
#include "android_surface_glue.h"
#include "android_asset_bootstrap.h"
#include "android_touch_overlay.h"
#include "../../renderers/vulkan/vk_android_surface.h"
#ifndef DEDICATED
#include "../../client/keycodes.h"
#include "../../qcommon/qcommon.h"
#endif
#define Com_QueueEvent Sys_QueEvent
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
static pthread_mutex_t  g_surfaceLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   g_surfaceCond = PTHREAD_COND_INITIALIZER;
/* Token bumped on each window destroy; game thread must finish vk_Android_OnNativeWindowGoingAway before callback returns */
static volatile uint32_t g_surfaceToken = 0;
static volatile uint32_t g_surfaceAckToken = 0;
static volatile int     g_surfaceOpPending = 0; /* 1=teardown, 2=recreate */
static void             *g_vulkanLib = NULL;
static char             g_dataPath[MAX_OSPATH] = "/sdcard/idtech3";
static char             g_homePath[MAX_OSPATH] = "";
static int              g_windowWidth = 1280;
static int              g_windowHeight = 720;
static int              g_engineInitialized = 0;
/* Single-finger touch: relative deltas for CL_MouseEvent (mouselook + UI drag) */
static qboolean         g_touchActive = qfalse;
static float            g_touchLastX;
static float            g_touchLastY;

/* ---- Sys functions ---- */

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

void VKimp_Init( glconfig_t *config ) {
	if ( !g_vulkanLib ) {
		g_vulkanLib = dlopen( "libvulkan.so", RTLD_NOW | RTLD_LOCAL );
	}

	if ( config && g_window ) {
		config->vidWidth = g_windowWidth;
		config->vidHeight = g_windowHeight;
	}

	LOGI( "VKimp_Init: Vulkan %s, window %dx%d",
		g_vulkanLib ? "loaded" : "FAILED", g_windowWidth, g_windowHeight );
}

void VKimp_Shutdown( qboolean unloadDLL ) {
	(void)unloadDLL;
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

/* ---- OpenGL / window stubs (Vulkan path uses VKimp_*; symbols required at link) ---- */

void GLimp_InitGamma( glconfig_t *config ) {
	(void)config;
}

void GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] ) {
	(void)red;
	(void)green;
	(void)blue;
}

void GLimp_Minimize( void ) {
}

void GLimp_LogComment( const char *comment ) {
	(void)comment;
}

void GLimp_Init( glconfig_t *config ) {
	if ( config ) {
		config->isFullscreen = qfalse;
	}
}

void GLimp_Shutdown( qboolean unloadDLL ) {
	(void)unloadDLL;
}

void GLimp_EndFrame( void ) {
}

void *GL_GetProcAddress( const char *name ) {
	(void)name;
	return NULL;
}

qboolean GLimp_VulkanAvailable( void ) {
	return qtrue;
}

/* ---- Audio Backend: AAudio (primary) + OpenSL ES (fallback) ---- */

#include <aaudio/AAudio.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#define ANDROID_AUDIO_SAMPLES   2048
#define ANDROID_AUDIO_CHANNELS  2
#define ANDROID_AUDIO_RATE      48000

static short audioBuffer[2][ANDROID_AUDIO_SAMPLES * ANDROID_AUDIO_CHANNELS];
static int   audioBufferIdx = 0;
static int   audioDmaPos = 0;
static qboolean audioInitialized = qfalse;
static int   audioBackendType = 0; /* 0=none, 1=AAudio, 2=OpenSL ES */

/* Forward declarations */
static void SNDDMA_FillBuffer( short *dest, int framesToFill );
static qboolean SNDDMA_InitAAudio( int sampleRate );
void SNDDMA_Shutdown( void );
static void Android_PollInput( void );
static int32_t onInputEvent( ANativeActivity *activity, AInputEvent *event );

typedef struct {
	int speed;
	int channels;
	int samplebits;
	int samples;
	int submission_chunk;
	byte *buffer;
} dma_t;
extern dma_t dma;
void SNDDMA_Shutdown( void );

/* ---- AAudio backend ---- */

static AAudioStream *aaStream = NULL;

static void SNDDMA_FillBuffer( short *dest, int framesToFill ) {
	int bufSize = framesToFill * ANDROID_AUDIO_CHANNELS * (int)sizeof(short);
	if ( dma.buffer ) {
		int pos = audioDmaPos * (dma.samplebits / 8) * dma.channels;
		int avail = dma.samples * (dma.samplebits / 8) * dma.channels;
		int copyLen = bufSize;
		if ( copyLen > avail ) copyLen = avail;
		int end = pos + copyLen;
		if ( end > avail ) {
			int first = avail - pos;
			memcpy( dest, dma.buffer + pos, first );
			memcpy( (byte*)dest + first, dma.buffer, copyLen - first );
		} else {
			memcpy( dest, dma.buffer + pos, copyLen );
		}
		audioDmaPos = (audioDmaPos + framesToFill) % dma.samples;
	} else {
		memset( dest, 0, bufSize );
	}
}

static aaudio_data_callback_result_t SNDDMA_AAudioCallback(
	AAudioStream *stream, void *userData, void *audioData, int32_t numFrames )
{
	(void)stream; (void)userData;
	SNDDMA_FillBuffer( (short *)audioData, numFrames );
	return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static qboolean SNDDMA_InitAAudio( int sampleRate ) {
	AAudioStreamBuilder *builder = NULL;
	aaudio_result_t result;

	result = AAudio_createStreamBuilder( &builder );
	if ( result != AAUDIO_OK ) {
		LOGE( "AAudio: createStreamBuilder failed (%d)", result );
		return qfalse;
	}

	AAudioStreamBuilder_setDirection( builder, AAUDIO_DIRECTION_OUTPUT );
	AAudioStreamBuilder_setSharingMode( builder, AAUDIO_SHARING_MODE_SHARED );
	AAudioStreamBuilder_setSampleRate( builder, sampleRate );
	AAudioStreamBuilder_setChannelCount( builder, ANDROID_AUDIO_CHANNELS );
	AAudioStreamBuilder_setFormat( builder, AAUDIO_FORMAT_PCM_I16 );
	AAudioStreamBuilder_setPerformanceMode( builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY );
#if __ANDROID_API__ >= 28
	AAudioStreamBuilder_setUsage( builder, AAUDIO_USAGE_GAME );
#endif
	AAudioStreamBuilder_setDataCallback( builder, SNDDMA_AAudioCallback, NULL );
	AAudioStreamBuilder_setFramesPerDataCallback( builder, ANDROID_AUDIO_SAMPLES );

	result = AAudioStreamBuilder_openStream( builder, &aaStream );
	AAudioStreamBuilder_delete( builder );

	if ( result != AAUDIO_OK ) {
		LOGE( "AAudio: openStream failed (%d)", result );
		aaStream = NULL;
		return qfalse;
	}

	result = AAudioStream_requestStart( aaStream );
	if ( result != AAUDIO_OK ) {
		LOGE( "AAudio: requestStart failed (%d)", result );
		AAudioStream_close( aaStream );
		aaStream = NULL;
		return qfalse;
	}

	audioBackendType = 1;
	LOGI( "AAudio: initialized (%d Hz, %d ch, low-latency, game usage)",
		sampleRate, ANDROID_AUDIO_CHANNELS );
	return qtrue;
}

/* ---- OpenSL ES fallback ---- */

static SLObjectItf slEngineObj = NULL;
static SLEngineItf slEngine = NULL;
static SLObjectItf slMixObj = NULL;
static SLObjectItf slPlayerObj = NULL;
static SLPlayItf   slPlay = NULL;
static SLBufferQueueItf slBufferQueue = NULL;

static void SNDDMA_SLCallback( SLBufferQueueItf bq, void *context ) {
	(void)context;
	audioBufferIdx ^= 1;
	SNDDMA_FillBuffer( audioBuffer[audioBufferIdx], ANDROID_AUDIO_SAMPLES );
	(*bq)->Enqueue( bq, audioBuffer[audioBufferIdx],
		ANDROID_AUDIO_SAMPLES * ANDROID_AUDIO_CHANNELS * sizeof(short) );
}

qboolean SNDDMA_Init( int sampleFrequencyInKHz ) {
	SLresult result;
	int sampleRate = sampleFrequencyInKHz > 0 ? sampleFrequencyInKHz * 1000 : ANDROID_AUDIO_RATE;

	/* Try AAudio first (low-latency, modern API) */
	if ( SNDDMA_InitAAudio( sampleRate ) ) {
		dma.speed = sampleRate;
		dma.channels = ANDROID_AUDIO_CHANNELS;
		dma.samplebits = 16;
		dma.samples = ANDROID_AUDIO_SAMPLES * 8;
		dma.submission_chunk = ANDROID_AUDIO_SAMPLES;
		audioInitialized = qtrue;
		return qtrue;
	}

	LOGI( "AAudio unavailable, falling back to OpenSL ES" );

	result = slCreateEngine( &slEngineObj, 0, NULL, 0, NULL, NULL );
	if ( result != SL_RESULT_SUCCESS ) {
		LOGE( "OpenSL ES: slCreateEngine failed (%d)", (int)result );
		return qfalse;
	}
	(*slEngineObj)->Realize( slEngineObj, SL_BOOLEAN_FALSE );
	(*slEngineObj)->GetInterface( slEngineObj, SL_IID_ENGINE, &slEngine );

	(*slEngine)->CreateOutputMix( slEngine, &slMixObj, 0, NULL, NULL );
	(*slMixObj)->Realize( slMixObj, SL_BOOLEAN_FALSE );

	SLDataLocator_AndroidSimpleBufferQueue locBufQ = {
		SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2
	};
	SLDataFormat_PCM formatPcm = {
		SL_DATAFORMAT_PCM, ANDROID_AUDIO_CHANNELS,
		(SLuint32)(sampleRate * 1000),
		SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
		SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
		SL_BYTEORDER_LITTLEENDIAN
	};
	SLDataSource audioSrc = { &locBufQ, &formatPcm };
	SLDataLocator_OutputMix locOutMix = { SL_DATALOCATOR_OUTPUTMIX, slMixObj };
	SLDataSink audioSnk = { &locOutMix, NULL };

	const SLInterfaceID ids[] = { SL_IID_BUFFERQUEUE };
	const SLboolean req[] = { SL_BOOLEAN_TRUE };

	result = (*slEngine)->CreateAudioPlayer( slEngine, &slPlayerObj, &audioSrc, &audioSnk, 1, ids, req );
	if ( result != SL_RESULT_SUCCESS ) {
		LOGE( "OpenSL ES: CreateAudioPlayer failed (%d)", (int)result );
		SNDDMA_Shutdown();
		return qfalse;
	}
	(*slPlayerObj)->Realize( slPlayerObj, SL_BOOLEAN_FALSE );
	(*slPlayerObj)->GetInterface( slPlayerObj, SL_IID_PLAY, &slPlay );
	(*slPlayerObj)->GetInterface( slPlayerObj, SL_IID_BUFFERQUEUE, &slBufferQueue );

	(*slBufferQueue)->RegisterCallback( slBufferQueue, SNDDMA_SLCallback, NULL );

	dma.speed = sampleRate;
	dma.channels = ANDROID_AUDIO_CHANNELS;
	dma.samplebits = 16;
	dma.samples = ANDROID_AUDIO_SAMPLES * 8;
	dma.submission_chunk = ANDROID_AUDIO_SAMPLES;

	memset( audioBuffer, 0, sizeof( audioBuffer ) );
	audioBufferIdx = 0;
	audioDmaPos = 0;

	/* Prime the buffer queue */
	(*slBufferQueue)->Enqueue( slBufferQueue, audioBuffer[0],
		ANDROID_AUDIO_SAMPLES * ANDROID_AUDIO_CHANNELS * sizeof(short) );
	(*slBufferQueue)->Enqueue( slBufferQueue, audioBuffer[1],
		ANDROID_AUDIO_SAMPLES * ANDROID_AUDIO_CHANNELS * sizeof(short) );

	(*slPlay)->SetPlayState( slPlay, SL_PLAYSTATE_PLAYING );

	audioBackendType = 2;
	audioInitialized = qtrue;
	LOGI( "OpenSL ES: initialized (%d Hz, %d ch, %d samples)", sampleRate, ANDROID_AUDIO_CHANNELS, dma.samples );
	return qtrue;
}

void SNDDMA_Shutdown( void ) {
	if ( audioBackendType == 1 && aaStream ) {
		AAudioStream_requestStop( aaStream );
		AAudioStream_close( aaStream );
		aaStream = NULL;
	}
	if ( audioBackendType == 2 ) {
		if ( slPlayerObj ) { (*slPlayerObj)->Destroy( slPlayerObj ); slPlayerObj = NULL; }
		if ( slMixObj ) { (*slMixObj)->Destroy( slMixObj ); slMixObj = NULL; }
		if ( slEngineObj ) { (*slEngineObj)->Destroy( slEngineObj ); slEngineObj = NULL; }
		slPlay = NULL; slBufferQueue = NULL; slEngine = NULL;
	}
	audioBackendType = 0;
	audioInitialized = qfalse;
}

void SNDDMA_BeginPainting( void ) {}
int  SNDDMA_GetDMAPos( void ) { return audioDmaPos; }
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

		/* Touch → mouse button + relative motion (matches desktop mouse deltas) */
		if ( action == AMOTION_EVENT_ACTION_DOWN || action == AMOTION_EVENT_ACTION_POINTER_DOWN ) {
			g_touchActive = qtrue;
			g_touchLastX = x;
			g_touchLastY = y;
			Com_QueueEvent( Sys_Milliseconds(), SE_KEY, K_MOUSE1, qtrue, 0, NULL );
		} else if ( action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_POINTER_UP ||
			action == AMOTION_EVENT_ACTION_CANCEL ) {
			g_touchActive = qfalse;
			Com_QueueEvent( Sys_Milliseconds(), SE_KEY, K_MOUSE1, qfalse, 0, NULL );
		} else if ( action == AMOTION_EVENT_ACTION_MOVE && g_touchActive ) {
			float dx = x - g_touchLastX;
			float dy = y - g_touchLastY;
			float sens = 1.0f;
			const char *ts;
			g_touchLastX = x;
			g_touchLastY = y;
			ts = Cvar_VariableString( "com_androidTouchSens" );
			if ( ts && ts[0] ) {
				float v = Q_atof( ts );
				if ( v > 0.0f ) {
					sens = v;
				}
			}
			Com_QueueEvent( Sys_Milliseconds(), SE_MOUSE, (int)( dx * sens ), (int)( dy * sens ), 0, NULL );
		}
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

/* ---- Surface / Vulkan coordination (see vk_android_surface.c) ---- */

qboolean Android_NativeWindowReady( void ) {
	return g_windowReady ? qtrue : qfalse;
}

void Android_NativeWindowSize( int *width, int *height ) {
	if ( width ) {
		*width = g_windowWidth;
	}
	if ( height ) {
		*height = g_windowHeight;
	}
}

void Android_SurfaceThread_ProcessPending( void ) {
	int op;

	pthread_mutex_lock( &g_surfaceLock );
	op = g_surfaceOpPending;
	if ( op == 0 ) {
		pthread_mutex_unlock( &g_surfaceLock );
		return;
	}
	g_surfaceOpPending = 0;
	pthread_mutex_unlock( &g_surfaceLock );

	if ( op == 1 ) {
		vk_Android_OnNativeWindowGoingAway();
	} else if ( op == 2 ) {
		vk_Android_OnNativeWindowReady();
	}

	pthread_mutex_lock( &g_surfaceLock );
	g_surfaceAckToken = g_surfaceToken;
	pthread_cond_broadcast( &g_surfaceCond );
	pthread_mutex_unlock( &g_surfaceLock );
}

static void android_surface_request_op( int op ) {
	pthread_mutex_lock( &g_surfaceLock );
	g_surfaceToken++;
	g_surfaceOpPending = op;
	while ( g_surfaceAckToken != g_surfaceToken ) {
		pthread_cond_wait( &g_surfaceCond, &g_surfaceLock );
	}
	pthread_mutex_unlock( &g_surfaceLock );
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

	/* Optional: ship read-only game files under assets/apkassets/... in the APK */
	Android_AssetBootstrapUnpack( g_assetManager, g_dataPath );

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
			Android_TouchOverlay_PumpEvents();
			Android_SurfaceThread_ProcessPending();
			usleep( 50000 );
			lastTime = Sys_Milliseconds();
			continue;
		}

		int newTime = Sys_Milliseconds();
		int msec = newTime - lastTime;
		lastTime = newTime;

		if ( msec < 1 ) msec = 1;
		if ( msec > 200 ) msec = 200;

		Android_PollInput();
		Android_TouchOverlay_PumpEvents();
		Android_SurfaceThread_ProcessPending();
		Com_Frame( msec );
	}

	LOGI( "Game thread: shutting down" );
	return NULL;
}

/* ---- Input queue handling ---- */

static AInputQueue *g_inputQueue = NULL;

static void onInputQueueCreated( ANativeActivity *activity, AInputQueue *queue ) {
	(void)activity;
	g_inputQueue = queue;
	LOGI( "Input queue created" );
}

static void onInputQueueDestroyed( ANativeActivity *activity, AInputQueue *queue ) {
	(void)activity; (void)queue;
	g_inputQueue = NULL;
	LOGI( "Input queue destroyed" );
}

/* Called from the game thread to drain pending input events */
static void Android_PollInput( void ) {
	AInputEvent *event = NULL;
	if ( !g_inputQueue ) return;

	while ( AInputQueue_getEvent( g_inputQueue, &event ) >= 0 ) {
		if ( AInputQueue_preDispatchEvent( g_inputQueue, event ) ) continue;
		int handled = onInputEvent( NULL, event );
		AInputQueue_finishEvent( g_inputQueue, event, handled );
	}
}

/* ---- NativeActivity callbacks ---- */

static void onNativeWindowCreated( ANativeActivity *activity, ANativeWindow *window ) {
	(void)activity;
	g_window = window;
	g_windowWidth = ANativeWindow_getWidth( window );
	g_windowHeight = ANativeWindow_getHeight( window );
	g_windowReady = 1;
	LOGI( "Window created: %dx%d", g_windowWidth, g_windowHeight );

	if ( g_engineInitialized ) {
		android_surface_request_op( 2 );
	}
}

static void onNativeWindowDestroyed( ANativeActivity *activity, ANativeWindow *window ) {
	(void)activity; (void)window;
	/* Must destroy VkSurfaceKHR before returning — ANativeWindow* is invalid after this */
	if ( g_engineInitialized ) {
		android_surface_request_op( 1 );
	}
	g_windowReady = 0;
	g_window = NULL;
	LOGI( "Window destroyed" );
}

static void onNativeWindowResized( ANativeActivity *activity, ANativeWindow *window ) {
	(void)activity;
	g_windowWidth = ANativeWindow_getWidth( window );
	g_windowHeight = ANativeWindow_getHeight( window );
	LOGI( "Window resized: %dx%d", g_windowWidth, g_windowHeight );
	/* Same ANativeWindow; swapchain recreation is handled by Vulkan on OUT_OF_DATE / suboptimal */
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
#ifndef DEDICATED
	gw_active = qfalse;
#endif
	if ( audioInitialized ) {
		if ( audioBackendType == 1 && aaStream ) {
			AAudioStream_requestPause( aaStream );
		} else if ( audioBackendType == 2 && slPlay ) {
			(*slPlay)->SetPlayState( slPlay, SL_PLAYSTATE_PAUSED );
		}
	}
	LOGI( "Paused (audio suspended)" );
}

static void onResume( ANativeActivity *activity ) {
	(void)activity;
	g_paused = 0;
#ifndef DEDICATED
	gw_active = qtrue;
#endif
	if ( audioInitialized ) {
		if ( audioBackendType == 1 && aaStream ) {
			AAudioStream_requestStart( aaStream );
		} else if ( audioBackendType == 2 && slPlay ) {
			(*slPlay)->SetPlayState( slPlay, SL_PLAYSTATE_PLAYING );
		}
	}
	LOGI( "Resumed (audio resumed)" );
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
	activity->callbacks->onInputQueueCreated = onInputQueueCreated;
	activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;

	LOGI( "ANativeActivity_onCreate: id Tech 3 starting" );
	LOGI( "  Internal: %s", activity->internalDataPath ? activity->internalDataPath : "null" );
	LOGI( "  External: %s", activity->externalDataPath ? activity->externalDataPath : "null" );

	g_running = 1;
	g_paused = 0;
	g_engineInitialized = 0;

	pthread_create( &g_gameThread, NULL, gameThreadFunc, NULL );
}

#endif /* __ANDROID__ */
