/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

JNI from Java touch HUD -> thread-safe queue -> Sys_QueEvent / Cbuf_AddText.
RegisterNatives from GameActivity.nativeRegisterTouchOverlayJni (class loader context).
===========================================================================
*/

#ifdef __ANDROID__

#include "q_shared.h"
#include "qcommon.h"
#include "android_touch_overlay.h"

#include <android/log.h>
#include <jni.h>
#include <pthread.h>
#include <string.h>

#define AOV_TAG "idTech3Touch"
#define AOV_QLEN 256

typedef enum {
	AOV_KEY = 1,
	AOV_MOUSE,
	AOV_CMD
} aov_type_t;

typedef struct {
	aov_type_t t;
	int v1;
	int v2;
	char cmd[160];
} aov_event_t;

static aov_event_t g_aov_q[AOV_QLEN];
static int g_aov_head;
static int g_aov_tail;
static pthread_mutex_t g_aov_mtx = PTHREAD_MUTEX_INITIALIZER;

static void aov_enqueue( const aov_event_t *e ) {
	pthread_mutex_lock( &g_aov_mtx );
	if ( ( g_aov_head + 1 ) % AOV_QLEN == g_aov_tail ) {
		pthread_mutex_unlock( &g_aov_mtx );
		__android_log_print( ANDROID_LOG_WARN, AOV_TAG, "input queue full, drop" );
		return;
	}
	g_aov_q[g_aov_head] = *e;
	g_aov_head = ( g_aov_head + 1 ) % AOV_QLEN;
	pthread_mutex_unlock( &g_aov_mtx );
}

void Android_TouchOverlay_PumpEvents( void ) {
	for ( ;; ) {
		aov_event_t e;

		pthread_mutex_lock( &g_aov_mtx );
		if ( g_aov_tail == g_aov_head ) {
			pthread_mutex_unlock( &g_aov_mtx );
			break;
		}
		e = g_aov_q[g_aov_tail];
		g_aov_tail = ( g_aov_tail + 1 ) % AOV_QLEN;
		pthread_mutex_unlock( &g_aov_mtx );

		switch ( e.t ) {
			case AOV_KEY:
				Sys_QueEvent( 0, SE_KEY, e.v1, e.v2, 0, NULL );
				break;
			case AOV_MOUSE:
				Sys_QueEvent( 0, SE_MOUSE, e.v1, e.v2, 0, NULL );
				break;
			case AOV_CMD:
				Cbuf_AddText( e.cmd );
				Cbuf_AddText( "\n" );
				break;
			default:
				break;
		}
	}
}

static void JNICALL aov_jni_Key( JNIEnv *env, jclass clazz, jint key, jboolean down ) {
	aov_event_t e;
	(void)env;
	(void)clazz;
	e.t = AOV_KEY;
	e.v1 = (int)key;
	e.v2 = down ? 1 : 0;
	e.cmd[0] = '\0';
	aov_enqueue( &e );
}

static void JNICALL aov_jni_MouseDelta( JNIEnv *env, jclass clazz, jint dx, jint dy ) {
	aov_event_t e;
	(void)env;
	(void)clazz;
	if ( dx == 0 && dy == 0 ) {
		return;
	}
	e.t = AOV_MOUSE;
	e.v1 = dx;
	e.v2 = dy;
	e.cmd[0] = '\0';
	aov_enqueue( &e );
}

static void JNICALL aov_jni_Command( JNIEnv *env, jclass clazz, jstring jcmd ) {
	aov_event_t e;
	const char *s;

	(void)clazz;
	e.t = AOV_CMD;
	e.v1 = e.v2 = 0;
	e.cmd[0] = '\0';

	if ( !jcmd ) {
		return;
	}
	s = (*env)->GetStringUTFChars( env, jcmd, NULL );
	if ( !s ) {
		return;
	}
	Q_strncpyz( e.cmd, s, sizeof( e.cmd ) );
	(*env)->ReleaseStringUTFChars( env, jcmd, s );
	aov_enqueue( &e );
}

static const JNINativeMethod aov_native_methods[] = {
	{ "nativeKey", "(IZ)V", (void *)&aov_jni_Key },
	{ "nativeMouseDelta", "(II)V", (void *)&aov_jni_MouseDelta },
	{ "nativeCommand", "(Ljava/lang/String;)V", (void *)&aov_jni_Command },
};

JNIEXPORT void JNICALL Java_com_gopex_idtech3_GameActivity_nativeRegisterTouchOverlayJni(
	JNIEnv *env, jclass clazz )
{
	jclass bridgeClass;

	(void)clazz;
	bridgeClass = (*env)->FindClass( env, "com/gopex/idtech3/TouchOverlayBridge" );
	if ( !bridgeClass ) {
		__android_log_print( ANDROID_LOG_ERROR, AOV_TAG, "FindClass TouchOverlayBridge failed" );
		return;
	}
	if ( (*env)->RegisterNatives( env, bridgeClass, aov_native_methods,
			(int)( sizeof( aov_native_methods ) / sizeof( aov_native_methods[0] ) ) ) < 0 ) {
		__android_log_print( ANDROID_LOG_ERROR, AOV_TAG, "RegisterNatives failed" );
		return;
	}
	__android_log_print( ANDROID_LOG_INFO, AOV_TAG, "Touch overlay JNI registered" );
}

#endif /* __ANDROID__ */
