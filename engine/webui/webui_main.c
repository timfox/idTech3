/*
 * webui_main.c - WebView integration for 1337 Surf
 * 
 * This module provides a platform-neutral interface to WebView
 * components, using the MIT-licensed webview/webview library as
 * the common API with native backends:
 * - Windows: Microsoft Edge WebView2
 * - Linux: WebKitGTK (dynamically linked, LGPL-2.1)
 * - macOS: WKWebView
 * 
 * Licensing:
 * - Engine: GPL-2.0-only
 * - WebUI bridge: GPL-2.0-only
 * - webview wrapper: MIT (compatible with GPL-2)
 * - WebKitGTK: LGPL-2.1 (dynamically linked)
 * - WebView2 Runtime: Microsoft redistributable terms
 */

#include "q_shared.h"
#include "webui.h"

#ifdef _WIN32
#include "webui_win32.h"
#elif defined(__APPLE__)
#include "webui_macos.h"
#else
#include "webui_linux.h"
#endif

/* Global state */
static bool s_webuiInitialized = false;

bool WebUI_Init(const webui_config_t *config) {
    if (s_webuiInitialized) {
        Com_Printf("WARNING: WebUI already initialized\n");
        return true;
    }
    
#ifdef _WIN32
    bool result = WebUI_InitWin32(config);
#elif defined(__APPLE__)
    bool result = WebUI_InitMacOS(config);
#else
    bool result = WebUI_InitLinux(config);
#endif
    
    if (result) {
        s_webuiInitialized = true;
        Com_Printf("WebUI initialized successfully\n");
    } else {
        Com_Printf("ERROR: WebUI initialization failed\n");
    }
    
    return result;
}

void WebUI_PumpEvents(void) {
    if (!s_webuiInitialized) {
        return;
    }
    
#ifdef _WIN32
    WebUI_PumpEventsWin32();
#elif defined(__APPLE__)
    WebUI_PumpEventsMacOS();
#else
    WebUI_PumpEventsLinux();
#endif
}

void WebUI_EvaluateJavaScript(const char *script) {
    if (!s_webuiInitialized) {
        Com_Printf("WARNING: WebUI not initialized, ignoring JS evaluation\n");
        return;
    }
    
    if (!script || !*script) {
        Com_Printf("WARNING: Empty JavaScript string\n");
        return;
    }
    
#ifdef _WIN32
    WebUI_EvaluateJavaScriptWin32(script);
#elif defined(__APPLE__)
    WebUI_EvaluateJavaScriptMacOS(script);
#else
    WebUI_EvaluateJavaScriptLinux(script);
#endif
}

void WebUI_PostGameEvent(const char *json) {
    if (!s_webuiInitialized) {
        Com_Printf("WARNING: WebUI not initialized, ignoring game event\n");
        return;
    }
    
    if (!json || !*json) {
        Com_Printf("WARNING: Empty JSON string\n");
        return;
    }
    
#ifdef _WIN32
    WebUI_PostGameEventWin32(json);
#elif defined(__APPLE__)
    WebUI_PostGameEventMacOS(json);
#else
    WebUI_PostGameEventLinux(json);
#endif
}

void WebUI_Shutdown(void) {
    if (!s_webuiInitialized) {
        return;
    }
    
#ifdef _WIN32
    WebUI_ShutdownWin32();
#elif defined(__APPLE__)
    WebUI_ShutdownMacOS();
#else
    WebUI_ShutdownLinux();
#endif
    
    s_webuiInitialized = false;
    Com_Printf("WebUI shutdown\n");
}
