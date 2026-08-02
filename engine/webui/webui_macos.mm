/*
 * webui_macos.c - WKWebView integration for macOS
 * 
 * Uses Apple's WKWebView framework (MIT-compatible licensing)
 * for integration with the GPL-2.0-only engine.
 * 
 * Licensing compliance:
 * - Engine: GPL-2.0-only
 * - This file: GPL-2.0-only
 * - WKWebView: Apple SDK (Apple Software License)
 * - MIT webview wrapper: retained copyright/license
 */

#include "q_shared.h"
#include "webui_macos.h"
#include <objc/objc.h>
#include <objc/message.h>
#include <Cocoa/Cocoa.h>
#include <WebKit/WebKit.h>
#include <pthread.h>
#include <string>
#include <queue>

/* Forward declarations */
static void *MainThread(void *arg);

/* Message queue for thread-safe communication */
static std::queue<std::string> s_messageQueue;
static pthread_mutex_t s_messageMutex;

/* Script message handler implementation */
@interface WKScriptMessageHandlerImpl : NSObject <WKScriptMessageHandler>
@end

@implementation WKScriptMessageHandlerImpl

- (void)userContentController:(WKUserContentController *)userContentController 
      didReceiveScriptMessage:(WKScriptMessage *)message {
    if ([message.name isEqualToString:@"game"]) {
        NSString *body = message.body;
        if ([body isKindOfClass:[NSString class]]) {
            const char *json = [body UTF8String];
            if (json) {
                pthread_mutex_lock(&s_messageMutex);
                s_messageQueue.push(std::string(json));
                pthread_mutex_unlock(&s_messageMutex);
            }
        }
    }
}

@end

/* WKWebView objects */
static NSWindow *s_window = nullptr;
static WKWebView *s_webView = nullptr;
static WKWebViewConfiguration *s_webViewConfig = nullptr;
static WKScriptMessageHandlerImpl *s_messageHandler = nullptr;
static pthread_t s_mainThread;
static bool s_mainThreadRunning = false;

/* Initialize Cocoa and create web view */
static bool CreateWKWebView(const webui_config_t *config) {
    /* Initialize Cocoa if not already initialized */
    if (!s_window) {
        /* Create window */
        NSRect frame = NSMakeRect(0, 0, config->width, config->height);
        s_window = [[NSWindow alloc] initWithContentRect:frame
                                               styleMask:(NSWindowStyleMaskTitled | 
                                                         NSWindowStyleMaskClosable | 
                                                         NSWindowStyleMaskMiniaturizable | 
                                                         NSWindowStyleMaskResizable)
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
        
        [s_window setTitle:[NSString stringWithUTF8String:config->title ? config->title : "1337 Surf UI"]];
        [s_window makeKeyAndOrderFront:nil];
        
        /* Create web view configuration */
        s_webViewConfig = [[WKWebViewConfiguration alloc] init];
        
        /* Configure web view */
        [s_webViewConfig setJavaScriptEnabled:YES];
        [s_webViewConfig setDeveloperExtrasEnabled:config->debug_tools ? YES : NO];
        
        /* Create script message handler */
        s_messageHandler = [[WKScriptMessageHandlerImpl alloc] init];
        
        /* Set up message handling */
        [s_webViewConfig.userContentController addScriptMessageHandler:s_messageHandler
                                                                name:@"game"];
        
        /* Create web view */
        s_webView = [[WKWebView alloc] initWithFrame:frame configuration:s_webViewConfig];
        
        /* Add web view to window content view */
        [[s_window contentView] addSubview:s_webView];
        [s_webView setTranslatesAutoresizingMaskIntoConstraints:NO];
        
        /* Auto-layout constraints */
        [s_window.contentView addConstraints:@[
            [NSLayoutConstraint constraintWithItem:s_webView
                                         attribute:NSLayoutAttributeTop
                                         relatedBy:NSLayoutRelationEqual
                                            toItem:s_window.contentView
                                         attribute:NSLayoutAttributeTop
                                        multiplier:1.0
                                          constant:0],
            [NSLayoutConstraint constraintWithItem:s_webView
                                         attribute:NSLayoutAttributeBottom
                                         relatedBy:NSLayoutRelationEqual
                                            toItem:s_window.contentView
                                         attribute:NSLayoutAttributeBottom
                                        multiplier:1.0
                                          constant:0],
            [NSLayoutConstraint constraintWithItem:s_webView
                                         attribute:NSLayoutAttributeLeft
                                         relatedBy:NSLayoutRelationEqual
                                            toItem:s_window.contentView
                                         attribute:NSLayoutAttributeLeft
                                        multiplier:1.0
                                          constant:0],
            [NSLayoutConstraint constraintWithItem:s_webView
                                         attribute:NSLayoutAttributeRight
                                         relatedBy:NSLayoutRelationEqual
                                            toItem:s_window.contentView
                                         attribute:NSLayoutAttributeRight
                                        multiplier:1.0
                                          constant:0]
        ]];
    }
    
    return true;
}

/* Main thread function */
static void *MainThread(void *arg) {
    @autoreleasepool {
        s_mainThreadRunning = true;
        
        /* Run main event loop */
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        [app activateIgnoringOtherApps:YES];
        
        /* Run the application */
        [app run];
        
        s_mainThreadRunning = false;
    }
    
    return nullptr;
}

bool WebUI_InitMacOS(const webui_config_t *config) {
    if (!config) {
        Com_Printf("ERROR: WebUI config is null\n");
        return false;
    }
    
    /* Initialize mutex */
    pthread_mutex_init(&s_messageMutex, nullptr);
    
    /* Create WKWebView */
    if (!CreateWKWebView(config)) {
        Com_Printf("ERROR: Failed to create WKWebView\n");
        pthread_mutex_destroy(&s_messageMutex);
        return false;
    }
    
    /* Start main thread */
    if (pthread_create(&s_mainThread, nullptr, MainThread, nullptr) != 0) {
        Com_Printf("ERROR: Failed to create main thread\n");
        WebUI_ShutdownMacOS();
        return false;
    }
    
    /* Give main thread time to initialize */
    usleep(100000); /* 100ms */
    
    Com_Printf("WebUI: macOS initialization complete\n");
    return true;
}

void WebUI_PumpEventsMacOS(void) {
    /* Process message queue */
    pthread_mutex_lock(&s_messageMutex);
    
    while (!s_messageQueue.empty()) {
        std::string message = s_messageQueue.front();
        s_messageQueue.pop();
        
        Com_DPrintf("WebUI: Received message: %s\n", message.c_str());
    }
    
    pthread_mutex_unlock(&s_messageMutex);
}

void WebUI_EvaluateJavaScriptMacOS(const char *script) {
    if (!s_webView) {
        Com_Printf("ERROR: WKWebView not initialized\n");
        return;
    }
    
    @autoreleasepool {
        NSString *scriptStr = [NSString stringWithUTF8String:script];
        [s_webView evaluateJavaScript:scriptStr 
                        completionHandler:nil];
    }
}

void WebUI_PostGameEventMacOS(const char *json) {
    if (!s_webView) {
        Com_Printf("ERROR: WKWebView not initialized\n");
        return;
    }
    
    @autoreleasepool {
        NSString *jsonStr = [NSString stringWithUTF8String:json];
        [s_webView.configuration.userContentController 
            sendScriptMessage:[WKScriptMessage new] 
                         name:@"game" 
                      body:jsonStr 
                   userInfo:nil];
    }
}

void WebUI_ShutdownMacOS(void) {
    /* Clean up WebKit objects */
    @autoreleasepool {
        if (s_webView) {
            [s_webView.configuration.userContentController removeScriptMessageHandlerForName:@"game"];
            [s_webView removeFromSuperview];
            [s_webView release];
            s_webView = nullptr;
        }
        
        if (s_window) {
            [s_window close];
            [s_window release];
            s_window = nullptr;
        }
        
        if (s_webViewConfig) {
            [s_webViewConfig release];
            s_webViewConfig = nullptr;
        }
        
        if (s_messageHandler) {
            [s_messageHandler release];
            s_messageHandler = nullptr;
        }
    }
    
    /* Clean up message queue */
    pthread_mutex_lock(&s_messageMutex);
    while (!s_messageQueue.empty()) {
        s_messageQueue.pop();
    }
    pthread_mutex_unlock(&s_messageMutex);
    
    pthread_mutex_destroy(&s_messageMutex);
    
    Com_Printf("WebUI: macOS shutdown complete\n");
}
