package com._1337surf_engine.webui;

import android.webkit.JavascriptInterface;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.util.Log;

/**
 * WebUI Bridge - JNI bridge between Java WebView and C engine
 * 
 * This class provides the communication bridge between the Android WebView
 * and the native C engine via JNI. It handles JavaScript message passing
 * and event callbacks.
 */
public class WebUIBridge {
    private static final String TAG = "WebUIBridge";
    private static WebUIBridge s_instance = null;
    private WebView mWebView = null;
    
    static {
        System.loadLibrary("webui");
    }
    
    public WebUIBridge(WebView webView) {
        mWebView = webView;
        s_instance = this;
        
        /* Configure WebView */
        mWebView.getSettings().setJavaScriptEnabled(true);
        mWebView.getSettings().setDomStorageEnabled(true);
        mWebView.getSettings().setDatabaseEnabled(true);
        
        /* Set WebViewClient to handle page events */
        mWebView.setWebViewClient(new WebViewClient() {
            @Override
            public void onPageFinished(WebView view, String url) {
                Log.d(TAG, "Page finished loading: " + url);
            }
            
            @Override
            public void onReceivedError(WebView view, int errorCode, String description, String failingUrl) {
                Log.e(TAG, "Received error: " + description + " (code: " + errorCode + ")");
            }
        });
    }
    
    /**
     * Send message from JavaScript to native code
     * This is called from JavaScript via window.game.sendMessage()
     */
    @JavascriptInterface
    public void sendMessage(String message) {
        Log.d(TAG, "Received message from JS: " + message);
        nativeSendMessage(message);
    }
    
    /**
     * Evaluate JavaScript in the WebView
     */
    public void evaluateJavaScript(String script) {
        if (mWebView != null) {
            mWebView.post(() -> {
                mWebView.evaluateJavascript(script, null);
            });
        }
    }
    
    /**
     * Post game event to WebView
     */
    public void postGameEvent(String json) {
        if (mWebView != null) {
            String js = "window.game.onGameEvent(" + json + ");";
            evaluateJavaScript(js);
        }
    }
    
    /**
     * Native callback for receiving messages from JavaScript
     */
    private native void nativeSendMessage(String message);
    
    /**
     * Get the singleton instance
     */
    public static WebUIBridge getInstance() {
        return s_instance;
    }
    
    /**
     * Get the WebView instance
     */
    public WebView getWebView() {
        return mWebView;
    }
}
