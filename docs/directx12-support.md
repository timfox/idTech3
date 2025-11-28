# DirectX 12 Support

## Overview

The id Tech 3 engine now includes DirectX 12 renderer support for Windows platforms, providing modern graphics API access with improved performance and features.

## Features

### DirectX 12 Renderer

- **Modern Graphics API**: Full DirectX 12 support with feature level 12.0+
- **Triple Buffering**: Efficient frame presentation with triple buffering
- **Command Lists**: Efficient command recording and execution
- **Descriptor Heaps**: Optimized resource management
- **Root Signatures**: Flexible shader resource binding
- **Pipeline State Objects**: Pre-compiled rendering pipelines
- **Resource Barriers**: Efficient resource state transitions
- **GPU Synchronization**: Proper fence-based synchronization

### Supported Features

- **Feature Levels**: 12.2, 12.1, 12.0, 11.1, 11.0 (with fallback)
- **Resource Binding Tiers**: Tier 1, 2, and 3 support detection
- **Debug Layer**: Optional D3D12 debug layer in debug builds
- **Swap Chain**: DXGI swap chain with flip discard model
- **Render Targets**: Multiple render target support
- **Depth Stencil**: 32-bit depth buffer support
- **Ray Tracing (DXR)**: DirectX Raytracing support with acceleration structures

## Requirements

### Windows SDK

- **Minimum**: Windows 10 SDK (10.0.17763.0 or later)
- **Recommended**: Latest Windows SDK
- **Visual Studio**: 2019 or later (for D3D12 headers)

### Hardware

- **GPU**: DirectX 12 compatible graphics card
- **Windows**: Windows 10 or later
- **Driver**: Latest graphics drivers recommended

## Building

### CMake Configuration

Enable D3D12 support:
```bash
cmake .. -DUSE_D3D12=ON
```

### Visual Studio

D3D12 is automatically enabled on Windows builds when `USE_D3D12=ON`:
```bash
cmake .. -G "Visual Studio 17 2022" -A x64 -DUSE_D3D12=ON
```

### Build Options

- `USE_D3D12`: Enable/disable DirectX 12 renderer (default: ON on Windows)
- `USE_RENDERER_DLOPEN`: Build renderer as dynamic library (supports multiple renderers)

## Usage

### Selecting D3D12 Renderer

Set renderer at runtime:
```
/r_renderer d3d12
```

Or set default in config:
```
set r_renderer d3d12
```

### CVars

- `r_renderer`: Select renderer backend (opengl, vulkan, d3d12)
- `r_d3d12_debug`: Enable D3D12 debug layer (debug builds only)

## Architecture

### Directory Structure

```
src/rendererd3d12/
├── d3d12.h          # D3D12 context and API
├── d3d12.c          # D3D12 initialization and management
├── tr_local.h       # Renderer local definitions
├── tr_common.h      # Common renderer definitions
├── tr_init.c        # Renderer initialization
└── tr_main.c        # Main renderer interface
```

### Key Components

1. **D3D12 Context** (`d3d12.h/.c`):
   - Device creation and management
   - Swap chain management
   - Command queue and lists
   - Synchronization objects
   - Descriptor heaps

2. **Renderer Interface** (`tr_main.c`):
   - Public renderer API implementation
   - Model/shader/texture registration
   - Frame rendering

3. **Initialization** (`tr_init.c`):
   - Window setup
   - Resource creation
   - State management

## Implementation Status

### Completed

- ✅ D3D12 device creation
- ✅ Swap chain creation
- ✅ Render target setup
- ✅ Command list recording
- ✅ Basic synchronization
- ✅ Root signature creation
- ✅ Build system integration
- ✅ DXR capability detection
- ✅ Ray tracing device interfaces
- ✅ Acceleration structure framework
- ✅ Shader binding table creation
- ✅ Ray tracing output buffer

### In Progress

- 🔄 Shader compilation (HLSL)
- 🔄 Pipeline state objects
- 🔄 Texture loading and management
- 🔄 Vertex/index buffer management
- 🔄 Full rendering pipeline

### Planned

- ⏳ Mesh rendering
- ⏳ Shader system
- ⏳ Texture system
- ⏳ Lighting system
- ⏳ Post-processing effects
- ⏳ ImGui integration
- ⏳ HLSL ray tracing shaders (ray generation, miss, closest hit)
- ⏳ Acceleration structure building with geometry
- ⏳ Ray tracing pipeline state with compiled shaders
- ⏳ Ray tracing integration with rendering pipeline

## Performance Considerations

### Advantages

- **Lower CPU Overhead**: Command lists reduce CPU overhead
- **Better Multi-threading**: Parallel command list recording
- **Efficient Resource Management**: Descriptor heaps and resource barriers
- **Modern Features**: Access to latest GPU features

### Optimization Tips

1. **Use Command Lists Efficiently**: Record commands once, execute multiple times
2. **Batch Draw Calls**: Group similar draws together
3. **Minimize State Changes**: Cache pipeline states
4. **Use Resource Barriers Wisely**: Batch barrier transitions
5. **Optimize Descriptor Heaps**: Reuse descriptors where possible

## Debugging

### Debug Layer

Enable D3D12 debug layer in debug builds:
```c
// Automatically enabled in _DEBUG builds
```

### Validation

The debug layer provides:
- Parameter validation
- Resource state tracking
- Memory leak detection
- Performance warnings

### Common Issues

**Device Removed:**
- Update graphics drivers
- Check for TDR (Timeout Detection and Recovery)
- Verify GPU compatibility

**Swap Chain Creation Failed:**
- Check window handle validity
- Verify format support
- Check feature level compatibility

**Command List Errors:**
- Ensure proper resource state transitions
- Verify resource lifetimes
- Check descriptor heap sizes

## Comparison with Other Renderers

### vs OpenGL

- **Lower Overhead**: D3D12 has less driver overhead
- **Better Control**: More explicit resource management
- **Modern Features**: Access to latest GPU features
- **Windows Only**: D3D12 is Windows-specific

### vs Vulkan

- **Similar Architecture**: Both are low-level APIs
- **Platform Specific**: D3D12 is Windows-only, Vulkan is cross-platform
- **API Style**: D3D12 uses COM, Vulkan uses C API
- **Feature Parity**: Similar feature sets

## Ray Tracing (DXR)

### Overview

The D3D12 renderer includes DirectX Raytracing (DXR) support for hardware-accelerated ray tracing. DXR provides real-time ray tracing capabilities for advanced lighting, reflections, and shadows.

### Requirements

- **GPU**: DirectX 12 compatible GPU with DXR support (NVIDIA RTX series, AMD RX 6000+)
- **Windows**: Windows 10 version 1809 (October 2018 Update) or later
- **Driver**: Latest graphics drivers with DXR support

### Features

- **Hardware Acceleration**: Uses GPU ray tracing cores when available
- **Acceleration Structures**: Bottom-level (BLAS) and top-level (TLAS) acceleration structures
- **Shader Binding Table**: Efficient shader dispatch for ray tracing
- **HDR Output**: Ray tracing writes to HDR buffer for proper tone mapping
- **Tier Detection**: Automatically detects DXR tier (1.0, 1.1)

### Usage

Ray tracing is automatically enabled if:
- GPU supports DXR
- Windows version is compatible
- Driver supports DXR

To check ray tracing status:
```
/r_d3d12_raytracing 1  // Enable ray tracing (if supported)
```

### Implementation Status

- ✅ DXR capability detection
- ✅ Device interface acquisition (ID3D12Device5)
- ✅ Command list interface (ID3D12GraphicsCommandList4)
- ✅ Acceleration structure framework
- ✅ Shader binding table creation
- ✅ Ray tracing output buffer
- 🔄 HLSL shader compilation (in progress)
- 🔄 Geometry acceleration structure building (in progress)
- ⏳ Ray tracing pipeline state with shaders
- ⏳ Integration with rendering pipeline

## Future Enhancements

Planned improvements:
- **Mesh Shaders**: Support for mesh shader pipeline
- **Variable Rate Shading**: VRS for performance optimization
- **Sampler Feedback**: Advanced texture sampling
- **Meshlet Rendering**: Efficient geometry processing
- **DXR Tier 1.1**: Support for inline ray tracing

## Troubleshooting

### Build Issues

**Missing D3D12 Headers:**
- Install Windows SDK
- Update Visual Studio
- Verify SDK version

**Link Errors:**
- Ensure d3d12.lib is linked
- Check dxgi.lib linkage
- Verify d3dcompiler.lib

### Runtime Issues

**Renderer Not Found:**
- Verify USE_D3D12=ON in CMake
- Check renderer DLL exists
- Verify Windows version

**Initialization Failed:**
- Check GPU compatibility
- Update graphics drivers
- Verify feature level support

## Resources

- [DirectX 12 Documentation](https://docs.microsoft.com/en-us/windows/win32/direct3d12/directx-12-programming-guide)
- [D3D12 Samples](https://github.com/Microsoft/DirectX-Graphics-Samples)
- [Windows SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/)

