# WebUI Module - WebView Integration for 1337 Surf

## Overview

This module provides a platform-neutral interface to WebView components, enabling
HTML-based UI for menus, tournament systems, and other web-based interfaces.

## Architecture

```
1337 Surf Engine (GPL-2.0)
├── WebUI Integration Layer (GPL-2.0)
│   ├── webui.h (platform-neutral API)
│   ├── webui_main.c (core implementation)
│   ├── webui_win32.c (Windows WebView2)
│   ├── webui_linux.c (Linux WebKitGTK)
│   └── webui_macos.c (macOS WKWebView)
└── WebView Wrappers (MIT)
    └── webview/webview (common API)
```

## Platform Backends

### Windows
- **Backend**: Microsoft Edge WebView2 Runtime
- **Runtime**: Evergreen, separately installed
- **License**: Microsoft redistributable terms
- **Dependency**: WebView2 SDK (headers only)

### Linux
- **Backend**: WebKitGTK (LGPL-2.1)
- **Linking**: Dynamically linked (LGPL compliance)
- **Package**: libwebkit2gtk-4.1-dev
- **Dependency**: pkg-config, WebKitGTK-4.1

### macOS
- **Backend**: WKWebView (Apple SDK)
- **License**: Apple Software License
- **Framework**: WebKit.framework

## Licensing Compliance

### GPL-2.0 Engine
- Engine code: GPL-2.0-only
- WebUI bridge: GPL-2.0-only
- JavaScript bindings: GPL-2.0-only

### MIT Wrapper
- webview/webview: MIT (compatible with GPL-2)
- Retained copyright and license notices

### LGPL-2.1 Library (Linux)
- WebKitGTK: LGPL-2.1 (dynamically linked)
- No static linking required
- No relinking requirements

### Microsoft Runtime (Windows)
- WebView2 Runtime: Microsoft redistributable
- Evergreen updates
- Separate installation required

## Build Requirements

### Windows
- WebView2 SDK (included in Visual Studio or standalone)
- Windows 10+ (WebView2 Runtime available)

### Linux
```bash
# Ubuntu/Debian
sudo apt install libwebkit2gtk-4.1-dev

# Fedora
sudo dnf install webkit2gtk3-devel

# Arch Linux
sudo pacman -S webkit2gtk
```

### macOS
- Xcode 12+ (WebKit.framework included)
- macOS 10.10+ (WKWebView available)

## API Reference

### Initialization
```c
bool WebUI_Init(const webui_config_t *config);
```

### Event Pumping
```c
void WebUI_PumpEvents(void);
```

### JavaScript Evaluation
```c
void WebUI_EvaluateJavaScript(const char *script);
```

### Game Event Posting
```c
void WebUI_PostGameEvent(const char *json);
```

### Shutdown
```c
void WebUI_Shutdown(void);
```

## Message Protocol

### JSON Format
```json
{
    "version": 1,
    "type": "menu.join_server",
    "requestId": 24,
    "payload": {
        "address": "127.0.0.1:27960"
    }
}
```

### Allowed Commands
- `menu.resume`
- `menu.disconnect`
- `menu.join_server`
- `settings.set`
- `tournament.register`

## Security Considerations

1. **Content Security Policy**: Use restrictive CSP in HTML files
2. **Local Content Only**: Load packaged local content, not arbitrary websites
3. **Message Validation**: Validate all messages in native code
4. **No Direct Console Access**: Use narrow message protocol
5. **Disable Dangerous Features**: Pop-ups, downloads, DevTools (release)

## Usage Example

```c
webui_config_t config = {
    .title = "1337 Surf Menu",
    .initial_url = "https://ui.1337surf.internal/index.html",
    .width = 1280,
    .height = 720,
    .debug_tools = false,
    .transparent = false
};

if (WebUI_Init(&config)) {
    /* Main loop */
    while (running) {
        WebUI_PumpEvents();
        /* Game logic */
    }
    
    WebUI_Shutdown();
}
```

## Build Integration

### CMake
```cmake
option(USE_WEBUI "Enable WebView integration" ON)

if(USE_WEBUI)
    # Windows
    if(WIN32)
        find_package(WebView2 REQUIRED)
    endif()
    
    # Linux
    if(UNIX AND NOT APPLE)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(WEBKIT2GTK REQUIRED IMPORTED_TARGET webkit2gtk-4.1)
    endif()
    
    # macOS
    if(APPLE)
        find_library(WEBKIT WebKit)
    endif()
    
    target_sources(engine PRIVATE
        engine/webui/webui_main.c
        engine/webui/webui_win32.c
        engine/webui/webui_linux.c
        engine/webui/webui_macos.c
    )
    
    target_include_directories(engine PRIVATE engine/webui)
    
    # Link dependencies
    if(WIN32)
        target_link_libraries(engine PRIVATE WebView2)
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(engine PRIVATE PkgConfig::WEBKIT2GTK)
    elseif(APPLE)
        target_link_libraries(engine PRIVATE ${WEBKIT})
    endif()
endif()
```

## Distribution Requirements

### License Files
- `COPYING` (GPL-2.0)
- `LICENSES/MIT-webview.txt`
- `LICENSES/LGPL-2.1.txt` (Linux)

### Third-Party Notices
- webview/webview
- WebKitGTK (Linux)
- WebView2 SDK (Windows)
- Any JavaScript packages

### Source Code
- Provide corresponding source for GPL-2.0 components
- Include build scripts and configuration

## Testing

### Windows
1. Install WebView2 Runtime
2. Build with WebView2 SDK
3. Test WebUI initialization
4. Verify message passing

### Linux
1. Install WebKitGTK-4.1
2. Build with pkg-config
3. Test WebUI initialization
4. Verify message passing

### macOS
1. Build with Xcode
2. Test WebUI initialization
3. Verify message passing
4. Check window rendering

## Troubleshooting

### Windows
- **WebView2 not found**: Install WebView2 Runtime
- **SDK missing**: Install WebView2 SDK
- **Runtime version**: Use evergreen runtime

### Linux
- **WebKitGTK not found**: Install libwebkit2gtk-4.1-dev
- **Dynamic linking**: Ensure WebKitGTK is dynamically linked
- **Version compatibility**: Use WebKitGTK-4.1 or later

### macOS
- **WebKit not found**: Use Xcode 12+
- **Framework path**: Check WebKit.framework location
- **Deployment target**: macOS 10.10+

## Future Enhancements

1. **Offscreen Rendering**: Render WebView to texture
2. **Hybrid UI**: Combine native and web UI
3. **Performance**: Optimize message passing
4. **Security**: Add sandboxing options
5. **Accessibility**: Improve accessibility support

## References

- [WebView2 Documentation](https://docs.microsoft.com/en-us/microsoft-edge/webview2/)
- [WebKitGTK Documentation](https://webkitgtk.org/documentation/)
- [WKWebView Documentation](https://developer.apple.com/documentation/webkit/wkwebview)
- [GPL-2.0 License](https://www.gnu.org/licenses/gpl-2.0.html)
- [LGPL-2.1 License](https://www.gnu.org/licenses/lgpl-2.1.html)
