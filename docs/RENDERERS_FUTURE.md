# Future Renderers: Metal, RTX, DXR

This document outlines the architecture and implementation plan for three renderer backends that are planned but not yet implemented.

## Current Status

| Backend | Status | Scope |
|---------|--------|-------|
| **Vulkan** | ✅ Complete | Primary (and supported) renderer, ~18k LOC |
| **WebSplatter** | 🔶 Demo path | **`r_wsp` 1–3** (latched): WebGPU-style **tile-binned** compute splats (16×16 tiles, 16 splats/tile, 16×16 wg). Clear → prepare → bin (atomics) → tile draw → composite. Overrides **`r_mgs`** when both on. Vulkan today; WGSL export planned. See **`docs/WEB_SPLATTER.md`**. |
| **Mobile-GS** | 🔶 Demo path | **`r_mgs` 1–3** (latched): tiered Vulkan compute splatting (64–1024 splats, 0.25–1.0 accum scale, capped footprint). Prepare → per-splat footprint → depth-aware composite into `color_image` after geometry. No RTX; targets Android / WebGPU-class budgets. Procedural Gaussians on map load; manifest `.mgs` reserved. See **`docs/MOBILE_GAUSSIAN_SPLATTING.md`**. |
| **GRTX (Gaussian RT)** | 🔶 Demo path | With `USE_VULKAN_RTX=ON` and **`r_grtx`>0** (latched): KHR RT over **3D Gaussian AABB proxies** (one BLAS, TLAS instance, `gl_PrimitiveID/12` → SSBO). Depth-based primary rays like `rtx_demo`; blit to `color_image` after main/post-bloom. **`r_grtxDemo` 1**, procedural Gaussians on map load; **`vk_grtx_spirv.inc`** from `glsl/grtx/*`. Future: `.ply`/splats, analytic hits, merged TLAS with BSP meshes. See **`docs/GAUSSIAN_RAY_TRACING_GRTX.md`**. |
| **Vulkan RTX** | 🔶 Demo path | With `USE_VULKAN_RTX=ON` and `r_rtx`>0 (latched): device enables KHR AS + ray-tracing pipeline + BDA. `r_rtxDemo` 1 (default): world BSP BLAS (all brush submodels `*0..*N-1`: SF_FACE + SF_TRIANGLES, capped by latched `r_rtxWorldPrimCap`) when a map is loaded, else a fallback triangle; raygen uses main-pass depth + `invViewProj` (Vulkan Y-flip projection, first-person projection when active); trace resolution matches **`vk_get_render_target_*`** (FBO / `r_renderScale`). **Closest-hit tints by `r_rtx` (1–3)** for visualization; trace after main/post-bloom, blit into `color_image`. **`r_rtxEntities` 1** (default **0**): optional entity proxy BLAS + TLAS (`r_rtxEntityCap`). `r_rtxDemo` 0: extensions only. Real lighting / full mesh BLAS still TODO. **`scripts/compile_shaders.sh --apply` regenerates `vk_rtx_demo_spirv.inc` from `glsl/rtx_demo.*`.** |
| **Path trace (C6)** | 🔶 Experimental | Parallel to `r_rtx` demo: **`r_pathtrace` 1** + **`r_pathtrace_arch`** `megakernel` or `wavefront` — multi-bounce diffuse over **same TLAS** (`vk_pathtrace.c`, `pt_mega` / `pt_wave` shaders). Requires **`r_rtxDemo` 1**. Nsight benchmark template: **`docs/PATHTRACE_ARCH_BENCHMARK.md`**. Not production hybrid GI. |
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
- Implements `refexport_t` (same interface as Vulkan)
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
