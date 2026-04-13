# Future Renderers: Metal, RTX, DXR

This document outlines the architecture and implementation plan for three renderer backends that are planned but not yet implemented.

## Current Status

| Backend | Status | Scope |
|---------|--------|-------|
| **Vulkan** | ✅ Complete | Primary renderer, ~18k LOC |
| **OpenGL** | ✅ Complete | Fallback renderer |
| **Vulkan RTX** | 🔶 Stub only | Ray tracing extensions probed; no pipeline |
| **Metal** | ❌ Not started | Native Apple Silicon / macOS |
| **DXR** | ❌ Not started | DirectX 12 + DirectX Raytracing (Windows) |

---

## 1. Vulkan RTX (Ray Tracing)

**Goal**: Hardware-accelerated ray tracing on NVIDIA RTX / AMD RDNA2+ via `VK_KHR_ray_tracing_pipeline` and `VK_KHR_acceleration_structure`.

### Required Extensions
- `VK_KHR_acceleration_structure`
- `VK_KHR_ray_tracing_pipeline`
- `VK_KHR_deferred_host_operations` (for async AS build)
- `VK_KHR_buffer_device_address` (or `VK_EXT_buffer_device_address`)

### Implementation Phases

1. **Extension enablement** (`vk_instance.c` / `vk_device.c` device creation)
   - Add ray tracing extensions to `device_extension_list` when available
   - Query `VkPhysicalDeviceRayTracingPipelinePropertiesKHR`
   - Load `vkCreateRayTracingPipelinesKHR`, `vkCmdTraceRaysKHR`, etc.

2. **Acceleration structures**
   - **BLAS**: Build from BSP world geometry + entity models (vertex/index buffers)
   - **TLAS**: Instance array from visible entities, updated per frame
   - Use `VkAccelerationStructureBuildGeometryInfoKHR` + `vkBuildAccelerationStructuresKHR`

3. **Ray tracing pipeline**
   - Raygen shader (camera rays)
   - Miss shader (sky / fallback)
   - Closest-hit shader (PBR shading, material fetch)
   - Shader binding table (SBT) layout

4. **Integration**
   - Hybrid: raster for primary visibility, RT for shadows/reflections, or full RT path
   - Cvar `r_rtx` (0=off, 1=shadows, 2=reflections, 3=full)
   - Fallback to raster when RT unavailable

### Reference
- [Vulkan Ray Tracing Tutorial](https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/)
- EternalJK's pbr-rtx-inspector (architecture reference in vk_imgui)

---

## 2. Metal Renderer (Apple)

**Goal**: Native Metal renderer for macOS and iOS (Apple Silicon), replacing MoltenVK for better performance and iOS support.

### Architecture
- New renderer module: `src/renderers/metal/`
- Implements `refexport_t` (same interface as Vulkan/OpenGL)
- Uses Metal 3 API (MTLDevice, MTLCommandQueue, MTLRenderPipelineState)
- Shaders: Metal Shading Language (MSL), compiled at runtime or offline

### Key Components
| Component | Metal equivalent |
|-----------|------------------|
| Pipeline | MTLRenderPipelineState |
| Command buffer | MTLCommandBuffer |
| Descriptor set | MTLArgumentEncoder / resource bindings |
| Vertex buffer | MTLBuffer |
| Texture | MTLTexture |
| Sampler | MTLSamplerState |
| Depth/stencil | MTLDepthStencilState |

### Build
- CMake: `USE_METAL_RENDERER=ON` (Apple only)
- Links: Metal.framework, MetalKit.framework
- Output: `idtech3_metal.dylib` (macOS) or static for iOS

### Shared Code
- Reuse: `tr_image.c`, `tr_bsp.c`, `tr_model_*.c`, shader logic (port GLSL→MSL)
- New: Metal-specific pipeline, swapchain, buffer management

### Scope
- ~8–12k LOC (similar to Vulkan subset)
- PBR, shadows, post-process: port from Vulkan

---

## 3. DXR Renderer (DirectX Raytracing)

**Goal**: DirectX 12 renderer with DirectX Raytracing (DXR) for Windows. Enables RT on NVIDIA GPUs and future AMD/Intel hardware via DXR.

### Architecture
- New renderer module: `src/renderers/dx12/` or `src/renderers/dxr/`
- Implements `refexport_t`
- Uses DX12: ID3D12Device, ID3D12CommandQueue, ID3D12GraphicsCommandList
- DXR: ID3D12Device5::CreateRaytracingPipelineState, ID3D12GraphicsCommandList4::DispatchRays

### Key Components
| Component | DX12 equivalent |
|-----------|-----------------|
| Pipeline | ID3D12PipelineState |
| Command list | ID3D12GraphicsCommandList |
| Descriptor heap | ID3D12DescriptorHeap |
| Resource | ID3D12Resource |
| Root signature | ID3D12RootSignature |
| RT pipeline | ID3D12StateObject (DXR) |
| AS | ID3D12Resource (D3D12_RAYTRACING_ACCELERATION_STRUCTURE) |

### Build
- CMake: `USE_DXR_RENDERER=ON` (Windows only)
- Requires: Windows SDK 10.0.19041+ (DXR support)
- Output: `idtech3_dxr.dll`

### DXR Pipeline
- Ray generation shader (HLSL)
- Miss shader
- Closest hit shader
- Shader binding table

### Scope
- ~10–15k LOC (full DX12 renderer + DXR)
- Mirrors Vulkan RTX architecture but uses DXR API

---

## Implementation Order

1. **Vulkan RTX** (incremental): Extend existing Vulkan renderer.
2. **Metal** (standalone): New backend for Apple ecosystem.
3. **DXR** (standalone): New backend for Windows RT.

---

## CMake Options (Scaffolding)

| Option | Default | Description |
|--------|---------|-------------|
| `USE_VULKAN_RTX` | OFF | Enable Vulkan ray tracing (VK_KHR_ray_tracing_*) |
| `USE_METAL_RENDERER` | OFF | Build Metal renderer (Apple only) |
| `USE_DXR_RENDERER` | OFF | Build DXR renderer (Windows only) |

---

## References

- [Vulkan Ray Tracing (KHR)](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#ray-tracing)
- [Metal Programming Guide](https://developer.apple.com/metal/)
- [DirectX Raytracing (DXR)](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html)
