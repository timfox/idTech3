# Metal Renderer Support

## Overview

The id Tech 3 engine provides integration with Apple's Metal graphics API, enabling native rendering on both macOS and iOS. This renderer delivers a modern GPU-accelerated experience for Apple hardware, with a unified codebase across desktop and mobile.

---

## Implementation Progress

### Features & Status

| Feature                         | Status   |
|----------------------------------|----------|
| Metal device/context creation    | ✅       |
| Swap chain (CAMetalLayer)        | ✅       |
| Render pipeline setup            | ✅       |
| Command buffer management        | ✅       |
| Triple buffering                 | ✅       |
| Metal shader compilation         | ✅       |
| UI/2D render path & shaders      | ✅       |
| 2D projection matrix             | ✅       |
| Resource management (basic)      | ✅       |
| State tracking (color, depth)    | ✅       |
| Vertex buffer pooling            | 🚧       |
| Texture management/loading       | 🚧       |
| Shader system integration        | 🚧       |
| Model/world rendering            | ⏳       |
| Ray tracing (Metal 3.0)          | ⏳       |

**Recent improvements:**
- UI render pipeline state (`uiPipelineState`)
- Dynamic orthographic projection for 2D rendering
- Separate depth-stencil state for 2D/UI
- Enhanced UI shaders (attribute layout, color modulation)
- State tracking (color, projection, buffer clearing)
- Drawable management (`currentDrawable`) and improved frame presentation

**Current development focus:**
- Vertex buffer management for 2D/3D pipelines
- Texture support (format conversion, image loading, binding)
- Shader system connection/integration
- Support for structured draw calls & simple models
- Extended resource tracking

**Future/Planned:**
- Full mesh and model rendering
- BSP/world rendering pipeline
- Lighting, post-processing, and advanced effects
- Metal 3 ray tracing and acceleration structures

---

## Requirements

### macOS

- macOS 10.13 (High Sierra) or later (11.0 recommended)
- Xcode 9.0+ with Metal tools
- Metal-compatible Mac GPU

### iOS

- iOS 11.0 or later (13.0+ recommended)
- Xcode 9.0+ (with iOS SDK)
- Metal-compatible iPhone/iPad

---

## Building

### CMake (macOS and iOS)

To enable Metal support:

```bash
cmake .. -DUSE_METAL=ON
make
```

For iOS:

```bash
cmake .. \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DUSE_METAL=ON
```

Or generate Xcode project:

```bash
cmake .. -G Xcode -DUSE_METAL=ON
```

Build options:
- `USE_METAL`: Enable/disable Metal renderer (default ON for Apple platforms)
- `USE_RENDERER_DLOPEN`: Build renderer as a dynamic library

---

## Usage

- To use Metal at runtime:
  ```
  /r_renderer metal
  ```
- Or set as default in configuration:
  ```
  set r_renderer metal
  ```

Main CVar:  
- `r_renderer`: {opengl, vulkan, d3d12, metal}

---

## Architecture

    src/renderermetal/
    ├── metal.h          # Metal context structure & API
    ├── metal.mm         # Objective-C++ Metal implementation
    ├── tr_local.h       # Renderer internal definitions
    ├── tr_common.h      # Renderer common/shared data
    ├── tr_init.c        # Renderer initialization
    ├── tr_main.c        # Public renderer interface
    └── shaders/
        ├── default.metal   # Main 3D shaders
        └── ui.metal        # 2D/UI shaders

Key components:
- **Metal context:** device, command queue, CAMetalLayer, pipeline state, depth-stencil state
- **Renderer entry points:** initialization, frame, shutdown; model/shader/texture registration
- **Shader system:** Metal shaders compiled to `.metallib` at build time

---

## Platform-Specific Notes

**macOS:**
- Uses `NSWindow` and AppKit for windowing
- Metal layer attached to window content view

**iOS:**
- Uses `UIView` and UIKit
- Metal layer as sublayer
- Supports app lifecycle and device rotation

---

## Troubleshooting

**Shader compile errors:**
- Ensure Xcode CLI and `xcrun metal` are available
- Check shader files in `src/renderermetal/shaders/`

**Renderer startup errors:**
- Metal-capable hardware required
- Correct OS/iOS version needed

**Build problems:**
- Confirm Xcode, SDKs, and CMake setup

---

## Related Documentation

- [Build Instructions](BUILD.md)
- [DirectX 12 Support](directx12-support.md)
- [Metal Programming Guide (Apple)](https://developer.apple.com/metal/)

---

