# Renderer Architecture & Feature Management

## Renderer Philosophy

The engine maintains **dual renderer architecture** (Vulkan + OpenGL) with **centralized feature management** to ensure:

- **Zero Breaking Changes**: Existing mods work unchanged
- **Progressive Enhancement**: Automatic upgrade path from OpenGL to Vulkan
- **Fail-Safe Design**: Graceful fallback prevents crashes
- **Observable Systems**: Clear logging of renderer decisions and features

## Renderer Selection Logic

### Primary Selection (`cl_renderer`)
```bash
# Automatic selection (recommended)
./idtech3.x86_64

# Explicit Vulkan (RTX enabled systems)
./idtech3.x86_64 +set cl_renderer vulkan

# Force OpenGL fallback
./idtech3.x86_64 +set cl_renderer opengl
```

### Fallback Chain
```
Requested → Available → Compatible → Working
    ↓           ↓           ↓          ↓
  vulkan   →  vulkan   →  vulkan  →  vulkan
    ↓           ↓           ↓          ↓
   rtx     →   opengl   →   opengl →   opengl
    ↓           ↓           ↓          ↓
 opengl   →   (error)  →   (error) →   (error)
```

### Hardware Detection
- **Vulkan Support**: Library load test + device enumeration
- **RTX Capability**: Extension query + hardware detection
- **OpenGL Fallback**: Always available as compatibility layer

## Feature Flag System

### Feature Categories

#### 🎯 **STABLE** (Always Enabled)
Core rendering features that are well-tested and production-ready:
- Vertex lighting (`r_vertexLight`)
- VBO support (`r_vbo`)
- Lightmap atlases (`r_mergeLightmaps`)
- Texture mipmapping (`r_simpleMipMaps`)

#### 🧪 **EXPERIMENTAL** (Safe Mode Disabled)
New features that may have edge cases or performance issues:
- Variable Rate Shading (`r_vrs`) - Vulkan only
- Bindless textures (`r_vk_bindlessTextures`) - Vulkan only
- Ray tracing (`r_rtx_enable`) - RTX hardware only
- Mesh shaders (`r_vkMeshShaders`) - Vulkan 1.4+

#### 🐛 **DEBUG** (Development Only)
Features for debugging and development:
- Shader validation (`r_vk_shaderValidation`)
- Debug overlays (`r_vk_debug_overlay`)
- Performance profiling (`r_vk_profiling`)
- Wireframe mode (`r_showtris`)

### Safe Mode Behavior

#### Activation Triggers
- Command line: `-safe` or `-safemode`
- Flag file: `logs/safe_mode.flag`
- Auto-detection: Engine crash recovery

#### Safe Mode Restrictions
```c
// Experimental features disabled
r_vrs = "0"
r_vk_bindlessTextures = "0"
r_rtx_enable = "0"
r_vk_raytracing = "0"

// Debug features disabled
developer = "0"
r_vk_debug_overlay = "0"
r_vk_profiling = "0"

// Conservative settings
r_vulkan_validation = "0"
cl_maxpackets = "30"  // Rate limiting
```

## Renderer-Specific Features

### Vulkan Renderer (`cl_renderer vulkan`)

#### Core Features
- **Vulkan 1.4** API with modern memory management
- **RTX Integration** for hardware ray tracing
- **Async Shader Compilation** for faster loading
- **Dynamic Rendering** for reduced API overhead

#### Performance Features
- **Variable Rate Shading** (VRS) for foveated rendering
- **Bindless Textures** for reduced draw calls
- **Mesh Shaders** for GPU-driven geometry
- **Pipeline Caching** for fast shader loading

#### Debug Features
- **Validation Layers** for API correctness
- **RenderDoc Integration** for frame capture
- **Shader Hot Reload** for development
- **Performance Profiling** with GPU timestamps

### OpenGL Renderer (`cl_renderer opengl`)

#### Core Features
- **OpenGL 3.3+** with ARB extensions
- **Compatibility Mode** for legacy hardware
- **Fallback Rendering** when Vulkan fails
- **Software Rasterization** as last resort

#### Quality Features
- **Multisample AA** (`r_ext_multisample`)
- **HDR Rendering** (`r_hdr`)
- **Bloom Effects** (`r_bloom`)
- **Post-Processing** pipeline

#### Performance Features
- **Vertex Buffer Objects** for batching
- **Texture Atlases** for reduced state changes
- **Shader Precompilation** for faster startup

## Startup Logging

### Clear Renderer Selection
```
Renderer: Vulkan Renderer v1.4 RTX
Safe Mode: inactive
Experimental Features: 2 enabled (VRS, Bindless Textures)
Debug Features: 0 enabled
Feature Summary: 45 enabled (3 experimental, 0 debug)
```

### Fallback Messages
```
Renderer fallback: vulkan → opengl (Vulkan library load failed)
Renderer: OpenGL Renderer v3.3
Safe Mode: inactive
```

### Safe Mode Activation
```
SAFE MODE ACTIVATED: safe mode flag detected
Experimental and debug features will be disabled
Renderer: Vulkan Renderer v1.4 RTX
Safe Mode: ACTIVE (safe mode flag detected)
Experimental Features: None enabled
Debug Features: None enabled
```

## Configuration Management

### Centralized Cvar Registration
All renderer features registered in `renderer_features.h` with:
- **Category**: stable/experimental/debug
- **Safe Mode State**: default when safe mode active
- **Restart Required**: `CVAR_LATCH` for expensive changes
- **Description**: Human-readable explanation

### Override Mechanisms
```bash
# Force experimental feature on
/set r_vrs 1

# Disable all experimental features
touch logs/safe_mode.flag

# Debug single feature
/set developer 1
/set r_vk_debug_overlay 1
```

## Testing Strategy

### Automated Validation
- **Smoke Tests**: Basic renderer initialization
- **Feature Tests**: Individual capability validation
- **Performance Tests**: Regression detection
- **Compatibility Tests**: Cross-hardware validation

### Manual Testing
- **Safe Mode**: Verify conservative fallbacks
- **Feature Flags**: Test individual feature toggles
- **Renderer Switching**: Verify state preservation

---

## Key Architectural Decisions

### Why Dual Renderers?
**Compatibility First**: Not all systems support Vulkan. OpenGL provides reliable fallback.

### Why Feature Flags?
**Fail-Safe Design**: Any feature can be disabled instantly if issues arise.

### Why Safe Mode?
**Production Stability**: Prevents "mystery state" in deployed applications.

### Why Clear Logging?
**Observability**: Every architectural decision is visible and auditable.

The renderer system is designed for **maximum compatibility** with **zero breaking changes** while providing a **clear upgrade path** to modern graphics features.