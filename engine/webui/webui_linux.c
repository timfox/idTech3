/*
 * webui_linux.c - WebKitGTK integration for Linux
 * 
 * Uses WebKitGTK (LGPL-2.1) via dynamic linking for compliance
 * with GPL-2.0-only engine licensing.
 * 
 * Licensing compliance:
 * - Engine: GPL-2.0-only
 * - This file: GPL-2.0-only
 * - WebKitGTK: LGPL-2.1 (dynamically linked)
 * - MIT webview wrapper: retained copyright/license
 * 
 * Build requirements:
 * - pkg-config for WebKitGTK-4.1
 * - libwebkit2gtk-4.1-dev on Ubuntu/Debian
 */

#include "q_shared.h"
#include "webui_linux.h"
#include <webkit2/webkit2.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <pthread.h>
#include <string>
#include <queue>

/* WebKitGTK objects */
static WebKitWebView *s_webView = nullptr;
static GtkWidget *s_window = nullptr;
static GMainLoop *s_mainLoop = nullptr;
static pthread_t s_mainLoopThread;
static bool s_mainLoopRunning = false;

/* Message queue for thread-safe communication */
static GQueue s_messageQueue;
static GMutex s_messageMutex;

/* Forward declarations */
static void *MainLoopThread(void *arg);
static void OnWebMessageReceived(WebKitWebView *webView, GVariant *message, gpointer data);
static void OnLoadChanged(WebKitWebView *webView, WebKitLoadEvent loadEvent, gpointer data);

/* Initialize WebKitGTK and create web view */
static bool CreateWebKitGTKView(const webui_config_t *config) {
    /* Initialize GTK */
    if (!gtk_init_check(nullptr, nullptr)) {
        Com_Printf("ERROR: Failed to initialize GTK\n");
        return false;
    }
    
    /* Create main window */
    s_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(s_window), config->title ? config->title : "1337 Surf UI");
    gtk_window_set_default_size(GTK_WINDOW(s_window), config->width, config->height);
    
    if (config->transparent) {
        gtk_widget_set_visual(s_window, gdk_screen_get_rgba_visual(gdk_screen_get_default()));
    }
    
    /* Create web view */
    s_webView = webkit_web_view_new();
    
    /* Configure web view */
    WebKitWebSettings *settings = webkit_web_view_get_settings(s_webView);
    g_object_set(settings, "enable-javascript", TRUE, nullptr);
    g_object_set(settings, "enable-developer-extras", config->debug_tools ? TRUE : FALSE, nullptr);
    
    /* Set up message handling */
    g_signal_connect(s_webView, "script-message-received::game", OnWebMessageReceived, nullptr);
    g_signal_connect(s_webView, "load-changed", OnLoadChanged, nullptr);
    
    /* Add web view to window */
    gtk_container_add(GTK_CONTAINER(s_window), s_webView);
    
    /* Show window */
    gtk_widget_show_all(s_window);
    
    return true;
}

/* Web message received callback */
static void OnWebMessageReceived(WebKitWebView *webView, GVariant *message, gpointer data) {
    const gchar *messageStr = g_variant_get_string(message, nullptr);
    
    if (messageStr) {
        GMutex *mutex = &s_messageMutex;
        g_mutex_lock(mutex);
        g_queue_push_tail(&s_messageQueue, g_strdup(messageStr));
        g_mutex_unlock(mutex);
    }
}

/* Load changed callback */
static void OnLoadChanged(WebKitWebView *webView, WebKitLoadEvent loadEvent, gpointer data) {
    if (loadEvent == WEBKIT_LOAD_FINISHED) {
        Com_Printf("WebUI: Page loaded successfully\n");
    }
}

/* Main loop thread function */
static void *MainLoopThread(void *arg) {
    s_mainLoop = g_main_loop_new(nullptr, FALSE);
    s_mainLoopRunning = true;
    
    g_main_loop_run(s_mainLoop);
    
    s_mainLoopRunning = false;
    g_main_loop_unref(s_mainLoop);
    s_mainLoop = nullptr;
    
    return nullptr;
}

bool WebUI_InitLinux(const webui_config_t *config) {
    if (!config) {
        Com_Printf("ERROR: WebUI config is null\n");
        return false;
    }
    
    /* Initialize message queue */
    g_queue_init(&s_messageQueue);
    g_mutex_init(&s_messageMutex);
    
    /* Create WebKitGTK view */
    if (!CreateWebKitGTKView(config)) {
        Com_Printf("ERROR: Failed to create WebKitGTK view\n");
        g_mutex_clear(&s_messageMutex);
        return false;
    }
    
    /* Start main loop thread */
    if (pthread_create(&s_mainLoopThread, nullptr, MainLoopThread, nullptr) != 0) {
        Com_Printf("ERROR: Failed to create main loop thread\n");
        WebUI_ShutdownLinux();
        return false;
    }
    
    Com_Printf("WebUI: Linux initialization complete\n");
    return true;
}

void WebUI_PumpEventsLinux(void) {
    /* Process GTK events */
    while (g_main_context_iteration(nullptr, FALSE));
    
    /* Process pending messages */
    GMutex *mutex = &s_messageMutex;
    g_mutex_lock(mutex);
    
    while (!g_queue_is_empty(&s_messageQueue)) {
        gchar *message = (gchar *)g_queue_pop_head(&s_messageQueue);
        if (message) {
            Com_DPrintf("WebUI: Received message: %s\n", message);
            g_free(message);
        }
    }
    
    g_mutex_unlock(mutex);
}

void WebUI_EvaluateJavaScriptLinux(const char *script) {
    if (!s_webView) {
        Com_Printf("ERROR: WebKitWebView not initialized\n");
        return;
    }
    
    webkit_web_view_run_javascript(s_webView, script, nullptr, nullptr);
}

void WebUI_PostGameEventLinux(const char *json) {
    if (!s_webView) {
        Com_Printf("ERROR: WebKitWebView not initialized\n");
        return;
    }
    
    /* Send message to JavaScript */
    webkit_web_view_send_message_to_page(s_webView, "game", g_variant_new_string(json));
}

void WebUI_ShutdownLinux(void) {
    /* Stop main loop */
    if (s_mainLoop) {
        g_main_loop_quit(s_mainLoop);
        pthread_join(s_mainLoopThread, nullptr);
    }
    
    /* Clean up WebKitGTK objects */
    if (s_webView) {
        gtk_widget_destroy(GTK_WIDGET(s_webView));
        s_webView = nullptr;
    }
    
    if (s_window) {
        gtk_widget_destroy(s_window);
        s_window = nullptr;
    }
    
    /* Clean up message queue */
    g_queue_clear_full(&s_messageQueue, g_free);
    g_mutex_clear(&s_messageMutex);
    
    Com_Printf("WebUI: Linux shutdown complete\n");
}
