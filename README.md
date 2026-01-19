[![build](../../workflows/build/badge.svg)](../../actions?query=workflow%3Abuild) <a href="https://discord.com/invite/X3Exs4C"><img src="https://img.shields.io/discord/314456230649135105?color=7289da&logo=discord&logoColor=white" alt="Discord server" /></a>

# id Tech 3

## **Modern Vulkan Renderer with RTX Support**

This enhanced version of the id Tech 3 engine features a complete Vulkan renderer implementation with RTX hardware acceleration, professional debugging tools, and robust error handling.

## **Key Features**

### **Advanced Rendering**
- **Vulkan API**: Modern graphics API with RTX ray tracing support
- **RTX Hardware**: Automatic detection and utilization of NVIDIA RTX GPUs
- **OpenGL Fallback**: Reliable fallback renderer for compatibility
- **imGUI Integration**: Professional performance monitoring and debugging

### **Robust Stability**
- **Memory Safety**: Comprehensive corruption detection and prevention
- **Error Recovery**: Automatic renderer fallback on failures
- **SIGFPE Protection**: Floating point exception handling
- **Filesystem Protection**: Safe file operations with corruption prevention

### **Developer Tools**
- **Performance HUD**: Real-time GPU and CPU monitoring
- **Shader Analysis**: Advanced shader performance tracking
- **Memory Profiling**: Detailed allocation and leak detection
- **Debug Logging**: Comprehensive diagnostic information

## **Enhanced Tools & Scripts**

### **Smart Launcher (Recommended)**
```bash
# Intelligent auto-configuration
./scripts/run_engine.sh --auto

# Force specific configurations
./scripts/run_engine.sh --vulkan --gpu=1        # Vulkan on discrete GPU
./scripts/run_engine.sh --opengl --developer    # OpenGL with debugging
./scripts/run_engine.sh --validation --perf-hud # Full Vulkan debugging

# Launch with mods/maps
./scripts/run_engine.sh +map q3dm9
./scripts/run_engine.sh +set fs_game mymod
```

### **Testing & Benchmarking**
```bash
# Run comprehensive test suite
./scripts/test_engine.sh

# Performance benchmarking
./scripts/benchmark_engine.sh --compare        # Compare Vulkan vs OpenGL
./scripts/benchmark_engine.sh --vulkan --duration=60

# System diagnostics
./scripts/run_engine.sh --detect-gpu           # GPU detection
./scripts/benchmark_engine.sh --system-info    # System information
```

## **Quick Start**

### **Basic Usage**
```bash
# Launch with Vulkan renderer (recommended)
./release/idtech3.x86_64

# Force specific renderer
./release/idtech3.x86_64 +set cl_renderer vulkan
./release/idtech3.x86_64 +set cl_renderer opengl

# Force NVIDIA GPU selection
VK_LAYER_MESA_device_select=device=1 ./release/idtech3.x86_64 +set cl_renderer vulkan
```

### **Advanced Configuration**
```bash
# Enable Vulkan extended features
touch logs/enable_vulkan_patch1_tiny.flag

# Developer mode with detailed logging
./release/idtech3.x86_64 +set developer 1 +set r_vk_enable_validation 1

# Performance monitoring
./release/idtech3.x86_64 +set r_perfhud 1
```

### **Automated Testing & Quality Assurance**
```bash
# Quick 10-second smoke test (recommended for basic validation)
./scripts/smoke_test.sh

# Safe mode launcher (disables experimental features)
./scripts/run_safe_mode.sh

# Run full test suite (recommended before production use)
./scripts/test_engine.sh

# Generate detailed test reports in logs/test_report_*.txt

# Performance regression testing
./scripts/benchmark_engine.sh --compare

# Automated validation of all engine features
# - Renderer initialization (Vulkan/OpenGL)
# - Memory safety systems
# - GPU detection and selection
# - Mod loading compatibility
# - Performance monitoring tools
```

### **Safe Mode & Feature Management**
The engine includes a comprehensive feature management system to prevent "mystery state":

```bash
# Launch in safe mode (disables experimental features)
./scripts/run_safe_mode.sh

# Command line safe mode options
./idtech3.x86_64 -safe
./idtech3.x86_64 -safemode

# Create safe mode flag file (persistent)
touch logs/safe_mode.flag
```

**Safe Mode Features:**
- ✅ Disables experimental renderer features (VRS, bindless textures, etc.)
- ✅ Forces conservative graphics settings
- ✅ Disables Vulkan validation layers
- ✅ Clear startup logging shows active restrictions
- ✅ Automatic fallback to stable configurations

**Startup Logging:**
The engine now provides clear, standardized logging:
- Renderer selection and fallback information
- Active experimental features warning
- Safe mode status and applied restrictions
- Feature summary with counts

## **Building from Source**

### **Prerequisites**
- GCC 15+ with C23/C++23 support
- CMake 3.20+
- Vulkan SDK
- SDL2 development libraries
- OpenEXR, Assimp, and other media libraries

### **Build Commands**
```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build all components
make -j$(nproc)

# Install to release directory
make install
```

### **Build Options**
```bash
# Enable/disable components
-DUSE_CIMGUI=ON          # imGUI support (default: ON)
-DUSE_VULKAN=ON          # Vulkan renderer (default: ON)
-DUSE_OPENGL=ON          # OpenGL renderer (default: ON)

# Optimization levels
-DCMAKE_BUILD_TYPE=Release    # Optimized build
-DCMAKE_BUILD_TYPE=Debug      # Debug build with symbols
```

## **Game Compatibility**

### **Supported Games**
- **Quake III Arena**: Full compatibility
- **Team Arena**: Complete support
- **Custom Mods**: All id Tech 3 based modifications

### **Asset Requirements**
- Base game PK3 files in `baseq3/` directory
- Mods in individual directories
- No additional asset conversion required

## **Troubleshooting**

### **Vulkan Issues**
```bash
# Check Vulkan installation
vulkaninfo --summary

# Test Vulkan renderer
./release/idtech3.x86_64 +set cl_renderer vulkan +quit

# Force OpenGL fallback
./release/idtech3.x86_64 +set cl_renderer opengl
```

### **Common Solutions**
1. **Vulkan crashes**: Engine automatically falls back to OpenGL
2. **GPU selection**: Use `VK_LAYER_MESA_device_select=device=N`
3. **Performance**: Enable Vulkan for best RTX performance
4. **Debugging**: Use `+set developer 1` for detailed logs

## **Performance Features**

### **Vulkan Optimizations**
- **Async Compute**: Parallel GPU operations
- **Memory Pooling**: Efficient GPU memory management
- **Pipeline Caching**: Fast shader loading
- **Multi-threading**: Parallel rendering pipelines

### **Monitoring Tools**
- **Real-time FPS**: Frame rate monitoring
- **GPU Usage**: Graphics processor utilization
- **Memory Stats**: GPU memory allocation tracking
- **Shader Performance**: Individual shader timing

## **Development**

### **Architecture**
```
src/
├── renderers/
│   ├── vulkan/          # Vulkan renderer implementation
│   └── opengl/          # OpenGL fallback renderer
├── client/              # Client-side game logic
├── server/              # Server implementation
├── common/              # Shared utilities
└── qcommon/            # Quake common code
```

### **Key Components**
- **Vulkan Renderer**: Modern graphics pipeline with RTX support
- **imGUI Integration**: Professional debugging interface
- **Memory Safety**: Comprehensive corruption detection
- **Error Handling**: Graceful failure recovery

### **Contributing**
1. Follow existing code style and patterns
2. Add comprehensive error checking
3. Include performance monitoring
4. Test with both Vulkan and OpenGL renderers

## **System Requirements**

### **Minimum**
- Linux distribution with Vulkan support
- 4GB RAM
- OpenGL 3.3 compatible GPU

### **Recommended**
- NVIDIA RTX GPU (for ray tracing)
- 8GB+ RAM
- Vulkan 1.1+ compatible GPU
- Multi-core CPU

---

**Built with modern C23/C++23 standards, comprehensive safety features, and professional debugging tools.**

This is a modernized id Tech 3 engine fork with PBR and ray tracing using C23 and C++23 standards.

Go to [Releases](../../releases) section to download the latest binaries for your platform or follow [Build Instructions](#build-instructions).

*This repository does not contain any game content from Quake III Arena.*

**Key features**:

* C23 and C++23 standards
* Ray tracing
* Physically based rendering (PBR)
* Realtime global illumination
* Raymarching for volumetrics
* Material clearcoat, anisotropy, and subsurface scattering options
* Steamworks and Steam Deck compatible
* Modernized graphics options menu
* ImGui layer for in-engine tools and overlays
* SysCall registry
* QT Radiant world editor
* Job System and multi-threading
* Online and offline documentation
* Font Rendering with OTF, TTF, and Glyph support

## Vulkan renderer

* Vulkan 1.4
* Ray tracing (hardware-accelerated where available, DXR-compatible)
* High-quality per-pixel dynamic lighting
* Surfel-based indirect lighting and global illumination
* Physically correct area and spot lights
* Raymarching for volumetrics
* FSR and dynamic resolution
* Greatly reduced API overhead (call/dispatch ratio)
* Flexible vertex buffer memory management to allow loading huge maps
* Pipeline cache and incremental pipeline compilation for faster loading and less stutter
* Multiple command buffers to reduce processing bottlenecks
* [Reversed depth buffer](https://developer.nvidia.com/content/depth-precision-visualized) eliminates z-fighting on big maps
* Merged lightmaps (atlases)
* Multitexturing optimizations
* Static world surfaces cached in VBO (**\r_vbo 1**)
* Useful debug markers for [RenderDoc](https://renderdoc.org/)
* Fixed framebuffer corruption on some Intel iGPUs
* Robust device lost & swapchain recreation handling
* Offscreen rendering, enabled with **\r_fbo 1** (required for features below):
    * `screenMap` texture rendering for realistic environment reflections
    * Multisample anti-aliasing (**\r_ext_multisample**)
    * Supersample anti-aliasing (**\r_ext_supersample**)
    * Per-window gamma-correction (important for OBS/screen-capture tools)
    * High dynamic range render targets (**\r_hdr 1**), prevents color banding
    * Bloom post-processing effect
    * Arbitrary resolution rendering
    * Greyscale mode
* Triple-buffered V-Sync for low-latency and smooth frame pacing
* VRR (Adaptive Sync/G-SYNC/Freesync) support where available
* You can minimize the game window any time during **\video** or **\video-pipe** recording


## OpenGL renderer

* Supports OpenGL 4.6 core profile
* High-quality per-pixel dynamic lighting (via **\r_dlightMode**)
* Lightmap atlases for improved lighting performance and quality
* Static geometry cached in vertex buffer objects (**\r_vbo 1**) for faster rendering
* Full suite of offscreen rendering capabilities (matching Vulkan renderer): includes HDR, bloom, multisample, supersample, environment reflections, greyscale mode, gamma-correct rendering, and more
* Post-processed bloom reflections


## DirectX 12 renderer

* Modern Graphics API: Full DirectX 12 support (feature level 12.0+)
* Triple buffering for smooth frame presentation
* Command lists and descriptor heaps for optimized resource and command management
* Root signatures and pre-compiled pipeline state objects
* Resource barriers and fence-based GPU sync
* Multiple render target (MRT) and 32-bit depth buffer support
* DXGI swap chain with flip discard model
* Optional D3D12 debug layer (in debug builds)
* Automatic detection of supported Feature Levels (12.2, 12.1, 12.0, 11.1, 11.0 fallback)
* Support for Resource Binding Tiers 1, 2, and 3
* **Ray Tracing (DXR):**
  * GPU-accelerated ray tracing (where supported)
  * Realistic ray-traced lighting and effects
  * Hardware-dependent


## Metal renderer

* Modern Metal API support (macOS/iOS)
* Triple buffering for efficient presentation
* Command buffers and encoders for rendering
* Render pipeline state objects for shaders
* Depth stencil state management
* Metal 2.0+ features: argument buffers, indirect command buffers
* Metal 3.0+ ray tracing support (macOS/iOS, where available)
* Automatic feature detection and capability queries
* iOS and macOS support with a unified codebase
* Metal Shading Language (MSL) shader compilation
* Efficient Metal resource allocation


**Requirements:**

* macOS 10.13+ or iOS 11.0+
* Metal-compatible GPU
* Xcode with Metal development tools


**Platform Support:**

* Full iOS app lifecycle management (UIApplicationDelegate)
* macOS window management (NSWindow)
* Unified platform abstraction layer for both iOS and macOS
* Automatic Metal feature detection/capability queries
* Retina display support
* Variable refresh rate (ProMotion) support
* High-DPI menu and input support on all platforms

## **Scripts & Tools**

### **Enhanced Launcher (`scripts/run_engine.sh`)**
Intelligent engine launcher with automatic GPU detection and renderer selection:
- **Auto-detection**: Automatically selects best renderer for your GPU
- **GPU Selection**: Force specific GPU devices (integrated/discrete)
- **Debug Options**: Enable validation layers, performance HUD, developer mode
- **Mod Support**: Easy launching with custom modifications

### **Testing Suite (`scripts/test_engine.sh`)**
Comprehensive automated testing framework:
- **Renderer Tests**: Validates Vulkan and OpenGL initialization
- **Safety Tests**: Memory corruption prevention and error recovery
- **Feature Tests**: imGUI, RTX support, performance monitoring
- **Mod Compatibility**: Tests custom modification loading
- **Report Generation**: Detailed test results and diagnostics

### **Benchmarking (`scripts/benchmark_engine.sh`)**
Performance analysis and comparison tools:
- **FPS Measurement**: Automated frame rate testing
- **GPU Monitoring**: Real-time GPU utilization and temperature
- **Renderer Comparison**: Side-by-side Vulkan vs OpenGL performance
- **System Profiling**: Hardware capability assessment

### **Usage Examples**
```bash
# Quick start with auto-configuration
./scripts/run_engine.sh

# Full system validation
./scripts/test_engine.sh

# Performance comparison
./scripts/benchmark_engine.sh --compare

# Detailed diagnostics
./scripts/run_engine.sh --detect-gpu
./scripts/benchmark_engine.sh --system-info
```

## [Build Instructions](docs/BUILD.md)

## Links

* https://idtech3.com
* https://github.com/timfox/idTech3Radiant
* https://bitbucket.org/CPMADevs/cnq3
* https://github.com/ioquake/ioq3
* https://github.com/kennyalive/Quake-III-Arena-Kenny-Edition
* https://github.com/OpenArena/engine
* https://github.com/tomkidd/Quake3-iOS
* https://github.com/NVIDIA/Q2RTX
* https://github.com/TTimo/GtkRadiant
* https://github.com/JKSunny/Quake3e