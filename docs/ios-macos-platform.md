# iOS and macOS Platform Support

## Overview

This document describes the platform-specific support for iOS and macOS in the id Tech 3 engine, including window management, app lifecycle, and platform integration.

## Architecture

### Platform Abstraction Layer

The platform abstraction layer (`macos_platform.h` / `macos_platform.mm`) provides a unified interface for both iOS and macOS platforms:

- **Window/View Management**: Create and manage windows (macOS) or views (iOS)
- **File System**: Access to base path, user path, and resource path
- **System Information**: CPU count, system info, Retina display detection
- **Platform Capabilities**: Metal feature detection and support

### iOS Support

#### App Lifecycle

iOS applications use `UIApplicationDelegate` to handle app lifecycle events:

- **didFinishLaunchingWithOptions**: Initialize engine and create main view
- **applicationWillResignActive**: Pause game when app goes to background
- **applicationDidEnterBackground**: Save state when app enters background
- **applicationWillEnterForeground**: Resume game when app comes to foreground
- **applicationDidBecomeActive**: Resume game when app becomes active
- **applicationWillTerminate**: Cleanup when app terminates
- **applicationDidReceiveMemoryWarning**: Handle memory warnings

#### View Management

iOS uses `UIView` for rendering:

```objective-c
// Create view
void* view = Platform_CreateView(width, height);

// Set view size
Platform_SetViewSize(view, width, height);

// Get view size
int width, height;
Platform_GetViewSize(view, &width, &height);
```

### macOS Support

#### Window Management

macOS uses `NSWindow` for window management:

```objective-c
// Create window
void* window = Platform_CreateWindow("Game Title", width, height, fullscreen);

// Set window size
Platform_SetWindowSize(window, width, height);

// Get window size
int width, height;
Platform_GetWindowSize(window, &width, &height);

// Set window title
Platform_SetWindowTitle(window, "New Title");

// Toggle fullscreen
Platform_SetWindowFullscreen(window, qtrue);
```

#### Main Loop

macOS uses `NSApplication` main loop:

```objective-c
NSApplication* app = [NSApplication sharedApplication];
[app run];
```

## Building

### iOS Build

For iOS builds, use CMake with iOS toolchain:

```bash
cmake .. \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DUSE_METAL=ON
```

Or use Xcode project generation:

```bash
cmake .. -G Xcode -DUSE_METAL=ON
```

### macOS Build

Standard macOS build:

```bash
cmake .. -DUSE_METAL=ON
make
```

## Platform Detection

The platform abstraction layer automatically detects:

- **iOS vs macOS**: Uses `TARGET_OS_IPHONE` preprocessor define
- **Metal Support**: Checks for Metal device availability
- **Metal 2.0+ Features**: Argument buffers, indirect command buffers
- **Metal 3.0+ Features**: Ray tracing support
- **Retina Display**: Detects high-DPI displays
- **Variable Refresh Rate**: ProMotion support (iOS 15+, macOS 12+)

## File System Paths

### Base Path

Returns the application bundle path:

- **iOS**: `/path/to/App.app`
- **macOS**: `/path/to/App.app/Contents/MacOS`

### User Path

Returns the user application support directory:

- **iOS**: `~/Library/Application Support/AppName`
- **macOS**: `~/Library/Application Support/AppName`

### Resource Path

Returns the application resource directory:

- **iOS**: `/path/to/App.app/`
- **macOS**: `/path/to/App.app/Contents/Resources`

## Integration with Metal Renderer

The platform abstraction layer integrates seamlessly with the Metal renderer:

1. **Window/View Creation**: Platform creates window/view, Metal renderer attaches CAMetalLayer
2. **Size Management**: Platform handles window/view resizing, Metal renderer updates swap chain
3. **Event Processing**: Platform processes system events, engine handles game events

## Example Usage

### iOS

```objective-c
// In AppDelegate
- (BOOL)application:(UIApplication *)application 
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    
    // Create main view
    void* view = Platform_CreateView(1024, 768);
    
    // Initialize engine
    Com_Init(argc, argv);
    
    // Initialize Metal renderer with view
    Metal_InitWindow(view);
    
    return YES;
}
```

### macOS

```objective-c
int main(int argc, char *argv[]) {
    // Initialize platform
    Platform_Init();
    
    // Create window
    void* window = Platform_CreateWindow("id Tech 3", 1024, 768, qfalse);
    
    // Initialize engine
    Com_Init(argc, argv);
    
    // Initialize Metal renderer with window
    Metal_InitWindow(window);
    
    // Run main loop
    [NSApp run];
    
    return 0;
}
```

## Troubleshooting

### iOS Build Issues

- **Missing UIKit**: Ensure iOS SDK is properly configured
- **App Delegate Not Found**: Check that `ios_appdelegate.mm` is included in build
- **Metal Not Available**: Verify device supports Metal (iPhone 5s or later)

### macOS Build Issues

- **Missing AppKit**: Ensure macOS SDK is properly configured
- **Window Not Showing**: Check that `[window makeKeyAndOrderFront:nil]` is called
- **Metal Not Available**: Verify Mac supports Metal (2012 or later)

## See Also

- [Metal Renderer Support](metal-support.md)
- [Build Instructions](BUILD.md)

