/*
 * webui_android.c - WebView integration for Android
 * 
 * Uses Android WebView (Apache 2.0) for integration with the
 * GPL-2.0-only engine via JNI bridge.
 * 
 * Licensing compliance:
 * - Engine: GPL-2.0-only
 * - WebUI bridge: GPL-2.0-only
 * - Android WebView: Apache 2.0 (compatible with GPL-2)
 */

#include "q_shared.h"
#include "webui_android.h"
#include <jni.h>
#include <pthread.h>
#include <string.h>
#include <queue>
#include <mutex>

/* JNI environment and class references */
static JavaVM *s_javaVM = nullptr;
static jobject s_webViewClient = nullptr;
static jclass s_webViewClass = nullptr;
static jmethodID s_evaluateJavaScriptMethod = nullptr;
static jmethodID s_postEventMethod = nullptr;

/* Message queue for thread-safe communication */
static std::queue<std::string> s_messageQueue;
static pthread_mutex_t s_messageMutex;

/* Android WebView objects */
static jobject s_webView = nullptr;
static jobject s_webViewClient = nullptr;

/* Forward declarations */
static void *AndroidMainLoop(void *arg);
static pthread_t s_mainThread;
static bool s_mainThreadRunning = false;

/* Get JNI environment for current thread */
static JNIEnv *GetJNIEnv() {
    JNIEnv *env = nullptr;
    int status = s_javaVM->GetEnv((void **)&env, JNI_VERSION_1_6);
    
    if (status == JNI_EDETACHED) {
        s_javaVM->AttachCurrentThread(&env, nullptr);
    }
    
    return env;
}

/* Initialize Android WebView */
static bool CreateWebView(const webui_config_t *config) {
    JNIEnv *env = GetJNIEnv();
    if (!env) {
        Com_Printf("ERROR: Failed to get JNI environment\n");
        return false;
    }
    
    /* Find WebView class */
    jclass webViewClass = env->FindClass("android/webkit/WebView");
    if (!webViewClass) {
        Com_Printf("ERROR: Failed to find WebView class\n");
        return false;
    }
    
    /* Create WebView instance */
    jmethodID constructor = env->GetMethodID(webViewClass, "<init>", "(Landroid/content/Context;)V");
    s_webView = env->NewObject(webViewClass, constructor, nullptr);
    
    if (!s_webView) {
        Com_Printf("ERROR: Failed to create WebView instance\n");
        return false;
    }
    
    /* Enable JavaScript */
    jclass webSettingsClass = env->FindClass("android/webkit/WebSettings");
    jmethodID getSettings = env->GetMethodID(webViewClass, "getSettings", "()Landroid/webkit/WebSettings;");
    jobject settings = env->CallObjectMethod(s_webView, getSettings);
    
    jmethodID setJavaScriptEnabled = env->GetMethodID(webSettingsClass, "setJavaScriptEnabled", "(Z)V");
    env->CallVoidMethod(settings, setJavaScriptEnabled, JNI_TRUE);
    
    /* Enable developer tools if requested */
    if (config->debug_tools) {
        jclass webSettingsClass2 = env->FindClass("android/webkit/WebSettings");
        jmethodID setDomStorageEnabled = env->GetMethodID(webSettingsClass2, "setDomStorageEnabled", "(Z)V");
        env->CallVoidMethod(settings, setDomStorageEnabled, JNI_TRUE);
        
        jmethodID setDatabaseEnabled = env->GetMethodID(webSettingsClass2, "setDatabaseEnabled", "(Z)V");
        env->CallVoidMethod(settings, setDomStorageEnabled, JNI_TRUE);
    }
    
    /* Create WebViewClient */
    jclass webViewClientClass = env->FindClass("android/webkit/WebViewClient");
    jmethodID clientConstructor = env->GetMethodID(webViewClientClass, "<init>", "()V");
    s_webViewClient = env->NewObject(webViewClientClass, clientConstructor);
    
    /* Set WebViewClient */
    jmethodID setClient = env->GetMethodID(webViewClass, "setWebViewClient", "(Landroid/webkit/WebViewClient;)V");
    env->CallVoidMethod(s_webView, setClient, s_webViewClient);
    
    /* Load initial URL if provided */
    if (config->initial_url) {
        jstring urlStr = env->NewStringUTF(config->initial_url);
        jmethodID loadUrl = env->GetMethodID(webViewClass, "loadUrl", "(Ljava/lang/String;)V");
        env->CallVoidMethod(s_webView, loadUrl, urlStr);
        env->DeleteLocalRef(urlStr);
    }
    
    return true;
}

/* JNI callback for receiving messages from JavaScript */
JNIEXPORT void JNICALL
Java_com_1337surf_1engine_1webui_WebUIBridge_nativeSendMessage(
    JNIEnv *env, jclass clazz, jstring message) {
    
    const char *json = env->GetStringUTFChars(message, nullptr);
    
    pthread_mutex_lock(&s_messageMutex);
    s_messageQueue.push(std::string(json));
    pthread_mutex_unlock(&s_messageMutex);
    
    env->ReleaseStringUTFChars(message, json);
}

/* Android main loop */
static void *AndroidMainLoop(void *arg) {
    /* This would run the Android Looper if needed */
    s_mainThreadRunning = true;
    
    /* Keep thread alive */
    while (s_mainThreadRunning) {
        usleep(10000); /* 10ms */
    }
    
    return nullptr;
}

bool WebUI_InitAndroid(const webui_config_t *config) {
    if (!config) {
        Com_Printf("ERROR: WebUI config is null\n");
        return false;
    }
    
    /* Initialize mutex */
    pthread_mutex_init(&s_messageMutex, nullptr);
    
    /* Get JavaVM from engine */
    extern JavaVM *GetJavaVM();
    s_javaVM = GetJavaVM();
    
    if (!s_javaVM) {
        Com_Printf("ERROR: Failed to get JavaVM\n");
        pthread_mutex_destroy(&s_messageMutex);
        return false;
    }
    
    /* Create WebView */
    if (!CreateWebView(config)) {
        Com_Printf("ERROR: Failed to create WebView\n");
        pthread_mutex_destroy(&s_messageMutex);
        return false;
    }
    
    /* Start main thread */
    if (pthread_create(&s_mainThread, nullptr, AndroidMainLoop, nullptr) != 0) {
        Com_Printf("ERROR: Failed to create main thread\n");
        WebUI_ShutdownAndroid();
        return false;
    }
    
    Com_Printf("WebUI: Android initialization complete\n");
    return true;
}

void WebUI_PumpEventsAndroid(void) {
    pthread_mutex_lock(&s_messageMutex);
    
    while (!s_messageQueue.empty()) {
        std::string message = s_messageQueue.front();
        s_messageQueue.pop();
        
        Com_DPrintf("WebUI: Received message: %s\n", message.c_str());
    }
    
    pthread_mutex_unlock(&s_messageMutex);
}

void WebUI_EvaluateJavaScriptAndroid(const char *script) {
    if (!s_webView) {
        Com_Printf("ERROR: WebView not initialized\n");
        return;
    }
    
    JNIEnv *env = GetJNIEnv();
    if (!env) {
        Com_Printf("ERROR: Failed to get JNI environment\n");
        return;
    }
    
    jstring scriptStr = env->NewStringUTF(script);
    env->CallVoidMethod(s_webView, s_evaluateJavaScriptMethod, scriptStr);
    env->DeleteLocalRef(scriptStr);
}

void WebUI_PostGameEventAndroid(const char *json) {
    if (!s_webView) {
        Com_Printf("ERROR: WebView not initialized\n");
        return;
    }
    
    JNIEnv *env = GetJNIEnv();
    if (!env) {
        Com_Printf("ERROR: Failed to get JNI environment\n");
        return;
    }
    
    jstring jsonStr = env->NewStringUTF(json);
    env->CallVoidMethod(s_webView, s_postEventMethod, jsonStr);
    env->DeleteLocalRef(jsonStr);
}

void WebUI_ShutdownAndroid(void) {
    s_mainThreadRunning = false;
    
    if (s_mainThread) {
        pthread_join(s_mainThread, nullptr);
    }
    
    /* Clean up WebView */
    JNIEnv *env = GetJNIEnv();
    if (env && s_webView) {
        env->DeleteGlobalRef(s_webView);
        s_webView = nullptr;
    }
    
    if (env && s_webViewClient) {
        env->DeleteGlobalRef(s_webViewClient);
        s_webViewClient = nullptr;
    }
    
    /* Clean up message queue */
    pthread_mutex_lock(&s_messageMutex);
    while (!s_messageQueue.empty()) {
        s_messageQueue.pop();
    }
    pthread_mutex_unlock(&s_messageMutex);
    
    pthread_mutex_destroy(&s_messageMutex);
    
    Com_Printf("WebUI: Android shutdown complete\n");
}