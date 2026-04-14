# SIGGRAPH-Inspired Features Implementation Roadmap

**Date**: March 2025  
**Scope**: Real-time rendering techniques from SIGGRAPH 2024–2025 Advances in Real-Time Rendering.

**Vulkan file map (2026)**: Monolithic `vk.c` was split. OIT and post paths: `vk_postfx_passes.c`, `vk_post_fog.c`, `vk_draw_state.c`. Volumetrics: `vk_volumetric_params.c`, `vk_volumetric_pass_compute.c`, `vk_volumetric_internal.c`. See `docs/ARCHITECTURE.md`.

---

## Overview

This document tracks implementation of 10 features from recent Siggraph papers, prioritized by tractability and fit with the idTech3 Vulkan renderer.

| # | Feature | Status | Priority | Est. Effort |
|---|---------|--------|----------|-------------|
| 1 | OIT (WBOIT) | Implemented (resolve; accum pipeline pending) | High | 1–2 days |
| 2 | Volumetric fog improvements | In progress (jitter + composite mode) | High | 2–3 days |
| 3 | MegaLights | Planned | High | 3–5 days |
| 4 | ReSTIR GI | Planned | High | 2–4 weeks |
| 5 | Neural Light Grid | Planned | Medium | 4–8 weeks |
| 6 | ReSTIR SSS | Planned | Medium | 2–3 weeks |
| 7 | Strand-based hair/fur | Planned | Medium | 2–3 weeks |
| 8 | TransGI | Experimental | Low | 4+ weeks |
| 9 | Radiant Foam | Experimental | Low | 4+ weeks |
| 10 | FastAtlas | Experimental | Low | 3+ weeks |

---

## 1. Order-Independent Transparency (WBOIT)

**Source**: Advances 2025, NVIDIA Weighted Blended OIT  
**Cvar**: `r_oit` (0=off, 1=WBOIT)

### Algorithm
- **Accumulation pass**: Render transparent surfaces with depth-weighted alpha. Output:
  - `accum_color += color * w` (w = alpha * pow(1 - linear_depth, 2))
  - `accum_weight += w`
- **Resolve pass**: `final = accum_color / max(accum_weight, 1e-5)`

### Integration
- Detect transparent surfaces: `GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA`
- Split `RB_RenderDrawSurfList`: opaque first, then OIT accumulation for transparent
- New render passes: `oit_accum`, `oit_resolve`
- Buffers: `oit_accum_image` (RGBA16F), packed (color*w, w)

### Status (March 2025)
- **Implemented**: Cvar `r_oit`, draw surf filter (opaque/transparent), OIT resolve pass, OIT accum pass, copy/resolve pipeline. OIT accum pipeline (`vk.oit_accum_pipeline`) with `oit_accum.vert`/`oit_accum.frag`, additive blend, gen vertex layout (position, color, texcoord). OIT draw path re-enabled in `RB_DrawSurfs` when `r_oit 1` + `r_fbo 1`.
- **Flow**: Opaque surfaces drawn first; `vk_oit_pass` copies opaque to fog_scene, runs OIT accum (transparent surfaces with WBOIT), resolves opaque + accum to main color, resumes post_bloom for sun/flares.

### Files
- `vk_postfx_passes.c` (`vk_oit_pass`), `vk_attachments.c`, `vk_pipeline_helpers.c`, related `vk_*.c`: OIT passes, buffers, pipelines
- `shaders/glsl/oit_accum.frag`, `oit_accum.vert`, `oit_resolve.frag`
- `tr_backend.c`: Draw surf filter, `RB_RenderDrawSurfList` (non-static for OIT)

---

## 2. Volumetric Fog Improvements (TLOU2-style)

**Source**: The Last of Us Part II, Advances  
**Cvars**: Existing `r_volumetricFog*` + `r_volumetricFogJitter`, **`r_volumetricFogCompositeMode`**

### Improvements
- **Jittered sampling**: Per-frame offset to reduce banding (already have temporal)
- **Composite modes** (`r_volumetricFogCompositeMode`): **0** = standard `scene * T + L`; **1** = depth-weighted in-scatter `scene * T + L * T` (reduces near-camera fog glow); **2** = optional per-channel clamp using `r_volumetricFogFireflyClamp` after composite
- **Transparent compositing**: Proper depth-aware blend with alpha-tested geometry
- **Froxel culling**: Aggressive frustum/occlusion culling for empty cells
- **Indirect dispatch**: Sort froxels by workload for better GPU utilization

### Current State
- Froxel grid: 160×90×64 (configurable)
- Temporal blend: `r_volumetricFogTemporalWeight`
- Compute stages: density, volume, sun, local lights, clamp, temporal

### Files
- `vk_volumetric_params.c`, `vk_volumetric_pass_compute.c`, `volumetric_fog.comp` / **`volumetric_fog.frag`**: Parameters, dispatch, jitter, composite resolve

---

## 3. MegaLights (Many-Lights)

**Source**: Epic, Advances 2025  
**Cvar**: `r_megaLights` (0=legacy 32, 1=stochastic many)

### Algorithm
- **Tile/cluster-based**: Divide screen into tiles; per-tile light list
- **Stochastic sampling**: Sample subset of lights per pixel; temporal accumulation
- **Importance sampling**: Weight by distance, angle

### Current State
- `MAX_DLIGHTS = 32`
- Forward rendering: per-surface dlight passes
- Shaders: `frag_light`, `frag_light_line`

### Integration
- Increase `MAX_DLIGHTS` or add `MAX_MEGALIGHTS`
- Tile pass: build light lists per tile (compute)
- Fragment shader: sample from tile's light list (bindless or UBO)
- Fallback: when `r_megaLights` off, use current path

### Files
- `tr_types.h`: MAX_DLIGHTS / MAX_MEGALIGHTS
- Future tile light culling would live under `vk_*.c` compute modules (not yet split as a single `vk.c` entry point)
- `light_frag.tmpl`: Many-lights sampling path

---

## 4. ReSTIR GI

**Source**: NVIDIA, Cyberpunk 2077, UE5 NvRTX  
**Cvar**: `r_restirGI` (requires `r_rtx`)

### Prerequisites
- Full RT pipeline: BLAS, TLAS, raygen/miss/closest-hit shaders
- See `docs/RENDERERS_FUTURE.md` for RT phases

### Algorithm
- **Reservoir sampling**: Maintain candidate light paths; resample across pixels/frames
- **Temporal reuse**: Reuse reservoirs from previous frame
- **Spatial reuse**: Share across neighborhood

### Phases
1. Implement RT pipeline (BLAS/TLAS, basic raygen)
2. Add ReSTIR DI (direct illumination) first
3. Extend to ReSTIR GI (indirect)

### References
- [ReSTIR Paper](https://research.nvidia.com/publication/2020-07_spatiotemporal-reservoir-resampled-importance-sampling)
- RTXDI SDK

---

## 5. Neural Light Grid

**Source**: Activision, SIGGRAPH 2024  
**Cvar**: `r_neuralLightGrid` (0=off, 1=learned probes)

### Algorithm
- Irradiance probes in 3D grid
- Learned weighting functions per probe (meta-learning)
- Runtime: trilinear sample probes, apply weights

### Prerequisites
- Precomputation tool (Python) for training probe weights
- Asset pipeline: bake or stream probe data
- Runtime: probe texture/SSBO, sample in fragment shader

### Integration
- Complements existing PBR IBL (cubemaps)
- Add probe grid for large outdoor levels
- Fallback to cubemap when no probes

---

## 6. ReSTIR Subsurface Scattering

**Source**: NVIDIA, SIGGRAPH 2025  
**Cvar**: `r_restirSSS`

### Prerequisites
- ReSTIR pipeline (from ReSTIR GI)
- Diffusion profile for skin/wax/marble

### Algorithm
- Hybrid: path-traced transport + diffusion approximation
- ReSTIR for path sampling; diffusion for smooth falloff

---

## 7. Strand-Based Hair/Fur

**Source**: MachineGames, Advances 2025  
**Cvar**: `r_hairMesh`

### Prerequisites
- `VK_EXT_mesh_shader` support
- Hair mesh asset format (strand data)
- Mesh shader: generate triangles from strands on GPU

### Algorithm
- Compact strand representation (~13–21 KB per model)
- Mesh shader: expand to geometry
- Custom texture layout for hair shading

---

## 8. TransGI (Neural GI)

**Source**: TransGI paper  
**Status**: Experimental

### Algorithm
- Object-centric neural transfer
- Radiance-sharing lighting
- Sub-10 ms target

### Prerequisites
- Neural network inference (ONNX, TensorRT, or custom)
- Training pipeline for game assets

---

## 9. Radiant Foam (Software RT)

**Source**: Differentiable ray tracing paper  
**Status**: Experimental

### Algorithm
- Voronoi tessellation scene representation
- Real-time ray tracing without RT cores
- Fallback when `r_rtx` unavailable

---

## 10. FastAtlas (Texture-Space Shading)

**Source**: FastAtlas paper  
**Status**: Experimental

### Algorithm
- Per-frame GPU atlasing
- Decouple shading from rasterization
- Network streaming, temporal reuse

---

## Implementation Order

1. **OIT** – Self-contained, immediate visual benefit
2. **Volumetric fog** – Incremental, already have froxels
3. **MegaLights** – Extends existing lighting, no RT required
4. **ReSTIR GI** – Unblocked by RT pipeline; high impact
5. **Neural Light Grid** – Precomputation + runtime; production-ready
6. **ReSTIR SSS** – After ReSTIR GI
7. **Strand hair** – Mesh shader pipeline
8. **TransGI / Radiant Foam / FastAtlas** – Research/experimental

---

## References

- [Advances in Real-Time Rendering 2025](https://advances.realtimerendering.com/s2025)
- [Advances in Real-Time Rendering 2024](https://advances.realtimerendering.com/s2024)
- [Neural Light Grid (Activision)](https://research.activision.com/publications/2024/08/Neural_Light_Grid)
- [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md) – RTX/Metal/DXR plan
