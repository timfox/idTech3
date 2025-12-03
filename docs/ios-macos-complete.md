# Complete iOS and macOS Platform Support

## Overview

This document summarizes all the components added for complete iOS and macOS platform support in the id Tech 3 engine.

## Components Added

### 1. Metal Renderer (`src/renderermetal/`)

**Files:**
- `metal.h` / `metal.mm` - Metal device and context management
- `tr_local.h` - Renderer local definitions
- `tr_init.c` - Renderer initialization
- `tr_main.c` - Main renderer interface
- `shaders/default.metal` - Metal Shading Language shaders

**Features:**
- Metal device creation and management
- CAMetalLayer swap chain setup
- Command buffer and encoder management
- Render pipeline state creation
- Shader library loading
- Triple buffering with semaphore synchronization
- Feature detection (Metal 1.0/2.0/3.0, ray tracing, argument buffers)

### 2. Platform Abstraction Layer (`src/unix/macos_platform.*`)

**Files:**
- `macos_platform.h` - Platform API definitions
- `macos_platform.mm` - Platform implementation (Objective-C++)

**Features:**
- Unified API for iOS and macOS
- Window/view management
- File system path utilities
- System information queries
- Metal feature detection
- Platform capability queries

### 3. iOS App Lifecycle (`src/unix/ios_appdelegate.mm`)

**Features:**
- UIApplicationDelegate implementation
- App lifecycle handling (background/foreground, memory warnings)
- Main entry point for iOS applications
- View controller setup
- Metal view integration

### 4. iOS Integration (`src/unix/ios_integration.mm`)

**Features:**
- Engine initialization on iOS
- View size change handling
- Frame update loop
- Memory warning handling
- Platform event processing

### 5. macOS Main Entry (`src/unix/macos_main.mm`)

**Features:**
- NSApplication initialization
- Window creation and management
- Main loop integration
- Platform cleanup

### 6. macOS Integration (`src/unix/macos_integration.c`)

**Features:**
- Integration with existing engine initialization
- File system path setup
- Platform info queries
- Shutdown handling

### 7. Build System Integration (`CMakeLists.txt`)

**Changes:**
- iOS vs macOS detection
- Platform-specific source files
- Framework linking (UIKit/AppKit, Foundation, Metal, QuartzCore)
- Objective-C++ compilation flags
- Metal shader compilation

### 8. Engine Integration (`src/unix/unix_main.c`)

**Changes:**
- Integration with Sys_Init()
- Platform-specific initialization hooks

### 9. Documentation

**Files:**
- `docs/metal-support.md` - Metal renderer documentation
- `docs/ios-macos-platform.md` - Platform support documentation
- `docs/ios-macos-complete.md` - This file

## Initialization Flow

### iOS

1. `main()` in `ios_appdelegate.mm` calls `UIApplicationMain()`
2. `application:didFinishLaunchingWithOptions:` creates window and view
3. `iOS_InitializeEngine()` initializes platform and engine
4. `Com_Init()` initializes engine subsystems
5. Renderer initialization happens when `CL_InitRef()` is called
6. Metal renderer attaches to view via `Metal_InitWindow()`

### macOS

1. `main()` in `macos_main.mm` initializes NSApplication
2. `Platform_Init()` initializes platform abstraction
3. `Sys_Init_Apple()` sets up file system paths
4. `Com_Init()` initializes engine subsystems
5. Window created via `Platform_CreateWindow()`
6. Metal renderer initialized via `Metal_InitWindow()`
7. `[app run]` starts main loop

## File System Paths

### iOS
- **Base Path**: `/path/to/App.app`
- **User Path**: `~/Library/Application Support/AppName`
- **Resource Path**: `/path/to/App.app/`

### macOS
- **Base Path**: `/path/to/App.app/Contents/MacOS`
- **User Path**: `~/Library/Application Support/AppName`
- **Resource Path**: `/path/to/App.app/Contents/Resources`

## Build Instructions

### iOS

```bash
cmake .. \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DUSE_METAL=ON
```

### macOS

```bash
cmake .. -DUSE_METAL=ON
make
```

## Usage

### Selecting Metal Renderer

Set renderer at runtime:
```
/r_renderer metal
```

Or set default in config:
```
set r_renderer metal
```

## Platform Capabilities

The platform abstraction layer automatically detects:

- **Metal Support**: Metal device availability
- **Metal 2.0+**: Argument buffers, indirect command buffers
- **Metal 3.0+**: Ray tracing support
- **Retina Display**: High-DPI display detection
- **Variable Refresh Rate**: ProMotion support (iOS 15+, macOS 12+)

## Integration Points

### With Engine

- `Sys_Init()` calls `Sys_Init_Apple()` for platform initialization
- File system paths set before `FS_InitFilesystem()`
- Renderer initialized via `CL_InitRef()` which loads Metal renderer

### With SDL2

- SDL2 can still be used for input/audio if `USE_SDL=ON`
- Metal renderer works independently of SDL2
- Platform abstraction provides native window/view management

### With Renderer System

- Metal renderer follows same interface as OpenGL/Vulkan/D3D12
- `GetRefAPI()` returns renderer interface
- Window/view handle passed to `Metal_InitWindow()`

## Testing Checklist

- [ ] iOS app launches and creates window
- [ ] macOS app launches and creates window
- [ ] Metal renderer initializes successfully
- [ ] File system paths are correct
- [ ] App lifecycle events handled (iOS)
- [ ] Window resizing works (macOS)
- [ ] Memory warnings handled (iOS)
- [ ] Shader compilation succeeds
- [ ] Renderer can be selected via cvar

## Known Limitations

1. **Input Handling**: Basic input handling implemented, full touch/mouse support needs completion
2. **Audio**: Audio system integration pending
3. **File System**: Some file system operations may need iOS sandbox considerations
4. **Rendering**: Full rendering pipeline implementation pending (basic structure in place)

## Future Enhancements

1. Complete rendering pipeline implementation
2. Full input handling (touch, mouse, keyboard)
3. Audio system integration
4. iOS-specific optimizations (battery, thermal)
5. macOS-specific features (fullscreen, window management)
6. Metal 3.0 ray tracing implementation
7. Performance profiling integration

## See Also

- [Metal Renderer Support](metal-support.md)
- [iOS/macOS Platform Support](ios-macos-platform.md)
- [Build Instructions](../BUILD.md)

