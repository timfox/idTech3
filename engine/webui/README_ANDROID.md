# WebUI Android Build Configuration

## Overview

This directory contains Android-specific WebView integration for the 1337 Surf engine.

## Files

- `webui_android.c` - Native C implementation with JNI bridge
- `webui_android.h` - Android-specific API header
- `WebUIBridge.java` - Java bridge class for WebView communication
- `AndroidManifest.xml` - Android manifest configuration

## Build Configuration

### CMake

Add to your `CMakeLists.txt`:

```cmake
# Enable WebUI
option(USE_WEBUI "Enable WebView integration" ON)

if(USE_WEBUI)
    # Android
    if(ANDROID)
        target_sources(engine PRIVATE
            engine/webui/webui_main.c
            engine/webui/webui_android.c
        )
        
        target_link_libraries(engine PRIVATE
            log
            jnigraphics
        )
        
        add_definitions(-DWEBUI_USE_WEBVIEW)
    endif()
endif()
```

### Java Bridge

The `WebUIBridge.java` class provides the JNI bridge between Java WebView and native C code:

```java
// Initialize WebUIBridge with WebView
WebView webView = new WebView(context);
WebUIBridge bridge = new WebUIBridge(webView);

// Send message to native code
bridge.sendMessage("{\"type\":\"menu.join_server\",\"payload\":{}}");

// Evaluate JavaScript
bridge.evaluateJavaScript("console.log('Hello from Java');");
```

## JNI Functions

### Java_com_1337surf_1engine_1webui_WebUIBridge_nativeSendMessage

Called from Java when JavaScript sends a message to native code.

```java
@JavascriptInterface
public void sendMessage(String message) {
    nativeSendMessage(message);
}
```

## Android API Level

- Minimum SDK: 21 (Android 5.0 Lollipop)
- Target SDK: 34 (Android 14)
- Compile SDK: 34

## WebView Features

- JavaScript execution
- DOM storage
- Database storage
- Hardware acceleration
- SSL error handling
- Page load callbacks

## Message Protocol

### From JavaScript to Native

```javascript
// In WebView JavaScript
window.game.sendMessage(JSON.stringify({
    type: "menu.join_server",
    payload: {
        address: "127.0.0.1:27960"
    }
}));
```

### From Native to JavaScript

```c
// In C code
WebUI_EvaluateJavaScript("window.game.onGameEvent('some_event');");
```

## Testing

### Build

```bash
# Configure with CMake
cmake -DUSE_WEBUI=ON -DANDROID_ABI=arm64-v8a ..

# Build
make
```

### Run

```bash
# Install on device
adb install app.apk

# Check logs
adb logcat | grep WebUI
```

## Troubleshooting

### WebView not loading

- Check internet permission in AndroidManifest.xml
- Verify WebView is enabled on device
- Check SSL certificates for HTTPS URLs

### JNI not found

- Ensure library is loaded: `System.loadLibrary("webui");`
- Check JNI function naming convention
- Verify CMake target sources include webui_android.c

### Messages not passing

- Verify @JavascriptInterface annotation on Java methods
- Check message queue is being processed
- Verify pthread mutex is working correctly

## Future Enhancements

1. **Offscreen Rendering**: Render WebView to texture
2. **Hybrid UI**: Combine native and web UI
3. **Performance**: Optimize message passing
4. **Security**: Add sandboxing options
5. **Accessibility**: Improve accessibility support
