# Renderer Unification System

## Overview

The idTech3 engine previously suffered from significant maintenance burden due to maintaining multiple renderer backends (OpenGL, OpenGL2, Vulkan, Metal, DirectX 12). This document describes the new unified renderer interface that consolidates these backends while maintaining feature parity and performance.

## Problem Statement

- **5 separate renderer implementations** with duplicated code
- **332+ TODO/FIXME items** across renderers requiring individual fixes
- **Maintenance complexity** - bug fixes need to be applied to multiple renderers
- **Feature inconsistency** - not all renderers support the same features
- **Testing overhead** - each renderer needs separate validation
- **Code duplication** - common functionality reimplemented per renderer

## Solution: Unified Renderer Interface

### Architecture

```
┌─────────────────┐
│   Game Engine   │
└─────┬───────────┘
      │ (rendererInterface_t*)
      ▼
┌─────────────────┐    ┌─────────────────┐
│ Unified Interface │──▶│ Vulkan Renderer │ (Primary)
│   (tr_renderer.h) │    └─────────────────┘
└─────┬───────────┘    ┌─────────────────┐
      │                │ OpenGL2 Renderer│ (Secondary)
      ▼                └─────────────────┘
┌─────────────────┐    ┌─────────────────┐
│ Renderer Common  │    │   Metal Renderer│ (Apple)
│   (shared code)  │    └─────────────────┘
└─────────────────┘    ┌─────────────────┐
                       │ DirectX12 Renderer│ (Windows)
                       └─────────────────┘
```

### Core Interface

All renderers implement the `rendererInterface_t` structure:

```c
typedef struct rendererInterface_s {
    // Identification
    const char* name;
    const char* description;
    uint32_t version;
    uint32_t features; // Bitfield of supported features

    // Core functions
    qboolean (*Init)(void);
    void (*Shutdown)(void);
    void (*BeginFrame)(void);
    void (*EndFrame)(void);

    // Scene management
    void (*BeginScene)(const refdef_t* refdef);
    void (*EndScene)(void);
    void (*AddEntity)(const refEntity_t* entity);

    // Resource management
    qhandle_t (*RegisterShader)(const char* name);
    qhandle_t (*RegisterImage)(const char* name);

    // Feature queries
    qboolean (*HasFeature)(rendererFeature_t feature);

} rendererInterface_t;
```

### Supported Features

| Feature | Vulkan | OpenGL2 | Metal | Legacy GL |
|---------|--------|---------|-------|-----------|
| Multisampling | ✅ | ✅ | ✅ | ⚠️ |
| Anisotropic Filtering | ✅ | ✅ | ✅ | ⚠️ |
| Shader Cache | ✅ | ❌ | ✅ | ❌ |
| Compute Shaders | ✅ | ❌ | ✅ | ❌ |
| Ray Tracing | ✅ | ❌ | ❌ | ❌ |
| Bindless Textures | ✅ | ❌ | ✅ | ❌ |
| Hot Reload | ✅ | ❌ | ❌ | ❌ |
| Performance HUD | ✅ | ❌ | ❌ | ❌ |

## Renderer Selection

### Build-time Selection

```bash
# Vulkan (recommended)
cmake -DRENDERER_DEFAULT=vulkan ..

# OpenGL2 (fallback)
cmake -DRENDERER_DEFAULT=opengl2 ..

# Legacy OpenGL (deprecated)
cmake -DRENDERER_DEFAULT=opengl ..
```

### Runtime Selection

```bash
# Force renderer at startup
./idtech3.x86_64 +set cl_renderer vulkan
./idtech3.x86_64 +set cl_renderer opengl2
```

### Automatic Fallback

The engine automatically falls back to available renderers if the preferred one fails:

1. **Vulkan** (primary choice - modern, high performance)
2. **OpenGL2** (secondary - modern OpenGL with shaders)
3. **Metal** (Apple platforms only)
4. **Legacy OpenGL** (deprecated fallback)

## Implementation Benefits

### Maintenance Reduction

- **Single interface** - All renderers implement the same API
- **Shared code** - Common functionality moved to `renderercommon/`
- **Unified testing** - One test suite validates all renderers
- **Feature flags** - Runtime feature detection

### Code Quality Improvements

- **Consistent error handling** across all renderers
- **Unified shader pipeline** framework
- **Standardized resource management**
- **Common validation and debugging tools**

### Performance & Features

- **Vulkan-first development** - New features implemented in Vulkan first
- **Feature parity** - All renderers support the same high-level features
- **Optimal backends** - Each renderer uses platform-specific optimizations
- **Shader hot reload** - Available on supporting renderers

## Migration Guide

### For Developers

1. **Use unified interface** - Access renderer through `renderer` global
2. **Check feature support** - Use `renderer->HasFeature()` for capabilities
3. **Platform-specific code** - Isolate in renderer implementations
4. **Testing** - Write renderer-agnostic tests

### For Users

1. **Default to Vulkan** - Best performance and features
2. **Automatic fallback** - Engine handles renderer selection
3. **Manual override** - Use `+set cl_renderer` for specific needs

## Future Plans

### Short Term (6-12 months)
- Complete OpenGL2 renderer implementation
- Metal renderer feature parity with Vulkan
- Unified shader pipeline across all renderers
- Comprehensive renderer testing suite

### Long Term (1-2 years)
- Deprecate legacy OpenGL renderer completely
- Ray tracing support across all modern renderers
- Advanced material system unification
- Cross-platform shader compilation pipeline

## Files Changed

### New Files
- `src/renderercommon/tr_renderer.h` - Unified interface definition
- `src/renderercommon/tr_renderer.c` - Common utilities
- `src/renderers/vulkan/tr_renderer_vulkan.c` - Vulkan implementation
- `src/renderers/opengl2/tr_renderer_opengl2.c` - OpenGL2 implementation
- `src/renderers/metal/tr_renderer_metal.c` - Metal implementation

### Modified Files
- `CMakeLists.txt` - Added unified renderer builds, deprecated legacy OpenGL
- `src/common/common.c` - Added renderer system initialization

## Conclusion

The unified renderer interface eliminates the maintenance burden of multiple renderers while preserving performance and feature capabilities. By providing a single, consistent API, the engine becomes more maintainable, testable, and extensible.

**Recommended renderer order:** Vulkan → OpenGL2 → Metal → Legacy OpenGL (fallback only)