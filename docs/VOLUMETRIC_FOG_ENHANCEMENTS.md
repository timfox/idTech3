# Volumetric Fog Enhancement Opportunities

**Date**: March 2025  
**Scope**: Additional areas to enhance the fog system toward cinematic-quality volumetric effects.  
**Reference**: Techniques from Advances in Real-Time Rendering, SIGGRAPH talks, and production volumetric pipelines.

**Vulkan paths (2026)**: Dispatch and composite logic are in `vk_volumetric_pass_compute.c`, `vk_volumetric_internal.c`, and related `vk_*.c` (not `vk.c`).

---

## Current State Summary

The engine already has:
- **Froxel grid**: 160×90×64 (configurable), frustum-aligned
- **Compute pipeline**: Density → volume → local lights → sun → clamp → temporal → fluid
- **Temporal reprojection**: History blend, motion-based rejection, camera-cut detection
- **Jittered sampling**: `r_volumetricFogJitter` in compute (density) and composite (raymarch)
- **Phase function**: Henyey-Greenstein anisotropy
- **Noise**: 3D FBM-style turbulence, wind scroll
- **Shadows**: Sun PCF, local spot/point shadow maps
- **VDB integration**: OpenVDB/NanoVDB for authored density volumes
- **Fluid simulation**: Navier-Stokes advection for dynamic fog motion

---

## Enhancement Areas (Cinematic-Style)

### 1. Froxel Culling and Workload Sorting

**Current**: Full grid dispatched every frame; no culling of empty or occluded froxels.

**Enhancement**:
- **Frustum culling**: Skip froxels outside the view frustum or behind the far plane
- **Depth culling**: Skip froxels entirely behind scene depth (use min depth per froxel)
- **Density culling**: Skip froxels with near-zero density (early-out in compute)
- **Indirect dispatch**: Build a list of active froxel indices; dispatch only those
- **Workload buckets**: Sort froxels by estimated cost (e.g., shadowed vs unshadowed) for better GPU occupancy

**Files**: `volumetric_fog.comp`, `vk_volumetric_pass_compute.c` (dispatch), new compute pass for culling/bucketing

---

### 2. Jittered Sampling Refinements

**Current**: Basic jitter in density (`rand31` + `densityParams.z`) and composite (`hash12` + `frameIndex`). Single-sample per froxel in compute.

**Enhancement**:
- **Halton/Low-discrepancy sequences**: Replace `rand31`/`hash12` with Halton(2,3) or similar for better temporal convergence
- **Per-slice jitter**: Vary jitter pattern along depth to break up banding
- **Blue-noise jitter**: Precomputed blue-noise texture for screen-space jitter (less structured aliasing)
- **TAA-style sample pattern**: Rotate sample positions per frame for temporal anti-aliasing of fog edges

**Files**: `volumetric_fog.comp`, `volumetric_fog.frag`, shared noise utility

---

### 3. Transparent Object Compositing

**Current**: Fog composites over full scene; alpha-tested geometry (foliage, fences) and transparent objects (glass, particles) are treated as opaque for fog depth.

**Enhancement**:
- **Depth-aware fog vs alpha**: When scene has alpha-tested or transparent surfaces, fog should appear *between* camera and geometry, not simply in front
- **Per-pixel depth ordering**: Ray march fog up to first opaque surface; blend transparent surfaces with fog behind them
- **OIT integration**: When OIT is active, fog should composite correctly with weighted-blended transparent layers (fog in front of, between, behind transparent objects)

**Files**: `volumetric_fog.frag`, OIT resolve path in `vk_postfx_passes.c` / `vk_post_fog.c`

---

### 4. Multi-Scattering Approximation

**Current**: Single-scatter only (in-scattered light from sun and local lights).

**Enhancement**:
- **Dual-lobe phase**: Separate phase for direct sun vs ambient (e.g., forward + isotropic)
- **Ambient multi-scatter**: Approximate higher-order scattering with a simple ambient term (e.g., fog color × density × constant)
- **Directional ambient**: Use SH or directional probes for sky contribution to fog color

**Files**: `volumetric_fog.comp` (sun/light stages), `VolumetricParams`

---

### 5. Shadowed Volumetrics Quality

**Current**: PCF 3×3 for sun; similar for local shadows. Single sample per froxel.

**Enhancement**:
- **Stochastic shadow sampling**: Sample shadow map with jitter; accumulate over frames
- **Cascaded shadow integration**: When using CSM, sample appropriate cascade per froxel
- **Soft shadow penumbra**: Vary shadow hardness by distance from occluder
- **Volumetric shadow resolution**: Optional higher-res shadow for fog-only (narrow FOV, focused on fog)

**Files**: `volumetric_fog.comp` (ComputeShadow, SampleSpotShadow, SamplePointShadow)

---

### 6. Slice Distribution and Resolution

**Current**: Exponential, linear, logarithmic modes; fixed grid dimensions.

**Enhancement**:
- **Adaptive slice count**: Reduce slices in low-density or distant regions
- **View-dependent resolution**: Lower XY resolution at screen edges (radial falloff)
- **LOD by distance**: Coarser froxels in the distance; finer near camera
- **Importance-based distribution**: Allocate more slices where density or light variation is high

**Files**: `volumetric_fog.comp`, `vk_volumetric_params.c` (grid setup), `VolumetricParams`

---

### 7. Authored Volume Integration (VDB)

**Current**: `VDB_Load` / `VDB_UploadToGPU` (R32 3D texture), `VDB_BindAsFogDensity`, **`r_vdbFog`** / **`r_vdbFogBlend`** blend bound density in **`volumetric_fog.comp`** global density stage (binding **17**). CPU `VDB_SampleFloat` remains for tooling.

**Enhancement**:
- **Animated VDB sequences** and richer blending (still open below)
- **VDB + procedural**: Combine authored clouds/smoke with height fog and noise
- **Animated VDB sequences**: Support frame-indexed VDB for explosions, smoke plumes
- **VDB world-space mapping**: Proper transform from VDB voxel space to world

**Files**: `vk_vdb.c`, `volumetric_fog.comp`, `vk_descriptor_sets.c` / volumetric update helpers (descriptor binding)

---

### 8. Fog Color and Lighting

**Current**: Fog color from map volume, tint cvar, or IBL SH. Sun and local lights.

**Enhancement**:
- **Height-based fog color**: Different tint at ground vs sky (gradient)
- **Distance-based color shift**: Cooler in distance (aerial perspective)
- **Light color contribution**: Local lights tint fog by their color; sun tint varies by time-of-day
- **God rays / crepuscular**: Emphasize scattering toward camera when view aligns with sun

**Files**: `volumetric_fog.comp`, `VolumetricParams`, cvars

---

### 9. Temporal Stability and Denoising

**Current**: Temporal blend, motion rejection, firefly clamp. Optional Gaussian denoise.

**Enhancement**:
- **Variance-guided blend**: Reduce history weight in high-variance regions
- **Neighborhood clamping**: Clamp history to min/max of current neighborhood (reduce ghosting)
- **Bilateral / edge-aware denoise**: Preserve fog edges while smoothing
- **Temporal accumulation for shadows**: Accumulate shadow samples over frames for softer, cheaper shadows

**Files**: `volumetric_fog.comp` (temporal stage), `volumetric_fog.frag`

---

### 10. Performance and Quality Tiers

**Current**: Quality tiers 0–3; resolution scale; step budget.

**Enhancement**:
- **Async compute overlap**: Run fog compute in parallel with main pass (if hardware supports)
- **Half-resolution composite**: Composite at half-res, upsample with bilateral or guided filter
- **Checkerboard fog**: Alternate frames for left/right or odd/even pixels; temporal resolve
- **Per-tier culling aggressiveness**: Low tier = more aggressive froxel culling
- **Budget-based auto-scale**: Already have `r_fogFluidTargetMs`; extend to full fog pipeline

**Files**: `vk_volumetric_pass_compute.c`, `vk_volumetric_params.c`, `volumetric_fog.comp`, `volumetric_fog.frag`

---

### 11. Fog Volumes and Shapes

**Current**: Box and sphere volumes; global height fog; procedural noise.

**Enhancement**:
- **Cylinder / cone volumes**: For spotlights, area lights, chimney smoke
- **Spline / path volumes**: Fog along a path (e.g., river mist, road dust)
- **Mesh-based volumes**: Convex hull or signed distance for arbitrary shapes
- **Blend modes**: Additive, multiplicative, max for overlapping volumes

**Files**: `volumetric_fog.comp` (ApplyVolumeDensity), `VolumetricParams`, `vk_volumetric_params.c`

---

### 12. Debug and Authoring

**Current**: r_fogDebug 0–13 (coords, extinction, scatter, temporal, motion, shadows, telemetry, etc.).

**Enhancement**:
- **Slice viewer**: Step through depth slices in debug view
- **Froxel occupancy heatmap**: Which froxels are active (culling preview)
- **Cost breakdown**: Per-stage timing (density, lights, temporal, composite)
- **Authoring overlays**: In-editor visualization of volume bounds, density preview

**Files**: `volumetric_fog.frag`, `vk_imgui.cpp`, inspector UI

---

## Implemented (March 2025)

| Area | Status |
|------|--------|
| Halton jitter | Compute and composite use halton2() for depth/slice jitter |
| Volume shapes | Cylinder (type 2) and cone (type 3); cvars r_volumetricFogCylinder*, r_volumetricFogCone* |
| Multi-scattering | Dual-lobe phase (gAmbient=-0.2), ambient multi-scatter term |
| Fog color/lighting | Height-based color (GetFogColorAtHeight), cooler at altitude |
| Shadow quality | Stochastic PCF jitter (frame-based offset) in ComputeShadow |
| Temporal stability | Variance-guided blend in ReprojectHistory |
| VDB fog | `r_vdbFog` + GPU 3D sample in `ComputeBaseDensity` when bound grid is uploaded |

## Priority Matrix

| Area | Impact | Effort | Dependencies |
|------|--------|--------|--------------|
| Froxel culling | High | Medium | None |
| Jitter refinements | Medium | Low | Done |
| Transparent compositing | High | Medium | OIT |
| Multi-scatter approx | Medium | Low | Done |
| Shadow quality | Medium | Medium | Done |
| Slice distribution | Medium | Low | None |
| VDB integration | High | Medium | Animated sequences, art workflow; base GPU path done |
| Fog color/lighting | Medium | Low | Done |
| Temporal/denoise | High | Medium | Done |
| Performance tiers | High | Medium | None |
| Volume shapes | Medium | Medium | Done |
| Debug/authoring | Low | Low | None |

---

## References (No IP)

- Advances in Real-Time Rendering (SIGGRAPH course)
- Bart Wronski, "Volumetric Fog" (SIGGRAPH 2014)
- Frostbite physically-based volumetric rendering
- Froxel-based deferred lighting and fog pipelines
