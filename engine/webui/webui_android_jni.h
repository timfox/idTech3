#ifndef SURF_WEBUI_ANDROID_JNI_H
#define SURF_WEBUI_ANDROID_JNI_H

/*
 * webui_android_jni.h - JNI function declarations for Android WebView
 * 
 * This header declares the JNI functions that are called from Java
 * to communicate with the native C engine.
 */

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * JNI callback from Java WebUIBridge
 * Called when JavaScript sends a message to native code
 */
JNIEXPORT void JNICALL
Java_com_1337surf_1engine_1webui_WebUIBridge_nativeSendMessage(
    JNIEnv *env, jclass clazz, jstring message);

#ifdef __cplusplus
}
#endif

#endif /* SURF_WEBUI_ANDROID_JNI_H */
