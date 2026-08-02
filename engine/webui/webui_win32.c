/*
 * webui_win32.c - WebView2 integration for Windows
 * 
 * Uses the MIT-licensed webview/webview library with
 * Microsoft Edge WebView2 Runtime (evergreen, separately installed)
 * 
 * Licensing compliance:
 * - Engine: GPL-2.0-only
 * - This file: GPL-2.0-only
 * - webview wrapper: MIT (retained copyright/license)
 * - WebView2 Runtime: Microsoft redistributable terms
 */

#include "q_shared.h"
#include "webui_win32.h"
#include <windows.h>
#include <WebView2.h>
#include <string>
#include <map>

/* WebView2 environment and browser objects */
static ICoreWebView2Environment *s_webView2Env = nullptr;
static ICoreWebView2 *s_webView2 = nullptr;
static HWND s_hwnd = nullptr;

/* Message handling */
static std::map<std::string, std::string> s_pendingMessages;
static CRITICAL_SECTION s_messageLock;

/* Forward declarations */
static LRESULT CALLBACK WebUI_WebViewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* Initialize WebView2 environment */
static HRESULT CreateWebView2Environment() {
    HRESULT hr = S_OK;
    
    /* Create WebView2 environment */
    hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,                    /* browser executable folder */
        nullptr,                    /* user data folder */
        nullptr,                    /* environment options */
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [&s_webView2Env](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {
                if (SUCCEEDED(result) && env) {
                    s_webView2Env = env;
                    s_webView2Env->AddRef();
                }
                return S_OK;
            }).Get());
    
    return hr;
}

/* Create WebView2 browser window */
static HRESULT CreateWebView2Window(const webui_config_t *config) {
    HRESULT hr = S_OK;
    WNDCLASSEX wc = {0};
    
    /* Register window class */
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WebUI_WebViewWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"WebUI_Window";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    
    if (!RegisterClassEx(&wc)) {
        Com_Printf("ERROR: Failed to register WebUI window class\n");
        return E_FAIL;
    }
    
    /* Create window */
    s_hwnd = CreateWindowEx(
        0,
        L"WebUI_Window",
        UTF8_ToWideChar(config->title ? config->title : "1337 Surf UI").c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        config->width, config->height,
        nullptr, nullptr, wc.hInstance, nullptr);
    
    if (!s_hwnd) {
        Com_Printf("ERROR: Failed to create WebUI window\n");
        return E_FAIL;
    }
    
    ShowWindow(s_hwnd, SW_SHOW);
    UpdateWindow(s_hwnd);
    
    /* Create WebView2 browser */
    hr = s_webView2Env->CreateCoreWebView2(
        s_hwnd,
        Callback<ICoreWebView2CreateCoreWebView2CompletedHandler>(
            [&s_webView2](HRESULT result, ICoreWebView2 *webView) -> HRESULT {
                if (SUCCEEDED(result) && webView) {
                    s_webView2 = webView;
                    s_webView2->AddRef();
                    
                    /* Configure WebView2 */
                    ICoreWebView2Settings *settings = nullptr;
                    s_webView2->get_Settings(&settings);
                    if (settings) {
                        settings->put_IsScriptEnabled(config->debug_tools ? TRUE : FALSE);
                        settings->put_IsWebMessageEnabled(TRUE);
                        settings->Release();
                    }
                    
                    /* Add event handlers */
                    s_webView2->add_WebMessageReceived(
                        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                            [](ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
                                LPWSTR message = nullptr;
                                args->get_WebMessageAsString(&message);
                                
                                std::string json = WideCharToUTF8(message);
                                CoTaskMemFree(message);
                                
                                EnterCriticalSection(&s_messageLock);
                                s_pendingMessages["web_message"] = json;
                                LeaveCriticalSection(&s_messageLock);
                                
                                return S_OK;
                            }).Get());
                }
                return S_OK;
            }).Get());
    
    return hr;
}

/* UTF-8 to wide string conversion */
static std::wstring UTF8_ToWideChar(const std::string &utf8) {
    if (utf8.empty()) return std::wstring();
    
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (size <= 0) return std::wstring();
    
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], size);
    return result;
}

/* Wide string to UTF-8 conversion */
static std::string WideCharToUTF8(const std::wstring &wstr) {
    if (wstr.empty()) return std::string();
    
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return std::string();
    
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

/* Window procedure */
static LRESULT CALLBACK WebUI_WebViewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            if (s_webView2) {
                RECT rect;
                GetClientRect(hwnd, &rect);
                s_webView2->put_Bounds(rect);
            }
            return 0;
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool WebUI_InitWin32(const webui_config_t *config) {
    if (!config) {
        Com_Printf("ERROR: WebUI config is null\n");
        return false;
    }
    
    /* Initialize COM */
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        Com_Printf("ERROR: Failed to initialize COM\n");
        return false;
    }
    
    /* Initialize critical section */
    InitializeCriticalSection(&s_messageLock);
    
    /* Create WebView2 environment */
    hr = CreateWebView2Environment();
    if (FAILED(hr)) {
        Com_Printf("ERROR: Failed to create WebView2 environment\n");
        CoUninitialize();
        return false;
    }
    
    /* Create WebView2 window */
    hr = CreateWebView2Window(config);
    if (FAILED(hr)) {
        Com_Printf("ERROR: Failed to create WebView2 window\n");
        if (s_webView2Env) {
            s_webView2Env->Release();
            s_webView2Env = nullptr;
        }
        CoUninitialize();
        return false;
    }
    
    return true;
}

void WebUI_PumpEventsWin32(void) {
    /* Process Windows messages */
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    /* Process pending messages from WebView2 */
    EnterCriticalSection(&s_messageLock);
    for (const auto &pair : s_pendingMessages) {
        /* Process message */
        Com_DPrintf("WebUI: Received message: %s\n", pair.first.c_str());
    }
    s_pendingMessages.clear();
    LeaveCriticalSection(&s_messageLock);
}

void WebUI_EvaluateJavaScriptWin32(const char *script) {
    if (!s_webView2) {
        Com_Printf("ERROR: WebView2 not initialized\n");
        return;
    }
    
    std::wstring scriptW = UTF8_ToWideChar(script);
    s_webView2->ExecuteScript(
        scriptW.c_str(),
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [](HRESULT result, LPCWSTR resultJson) -> HRESULT {
                if (FAILED(result)) {
                    Com_Printf("ERROR: JavaScript execution failed\n");
                }
                return S_OK;
            }).Get());
}

void WebUI_PostGameEventWin32(const char *json) {
    if (!s_webView2) {
        Com_Printf("ERROR: WebView2 not initialized\n");
        return;
    }
    
    std::wstring jsonW = UTF8_ToWideChar(json);
    s_webView2->PostWebMessageAsString(jsonW.c_str());
}

void WebUI_ShutdownWin32(void) {
    if (s_webView2) {
        s_webView2->Close();
        s_webView2->Release();
        s_webView2 = nullptr;
    }
    
    if (s_webView2Env) {
        s_webView2Env->Release();
        s_webView2Env = nullptr;
    }
    
    if (s_hwnd) {
        DestroyWindow(s_hwnd);
        s_hwnd = nullptr;
    }
    
    DeleteCriticalSection(&s_messageLock);
    CoUninitialize();
}
