# id Tech 7–class Renderer Sprint

**Date:** 2026-07-22  
**Principle:** Strengthen raster, material, lighting, geometry, and image formation **before** ReSTIR / full RT GI / neural reconstruction.

Canonical ownership: [RENDERER_PATH_OWNERSHIP.md](RENDERER_PATH_OWNERSHIP.md) · Correctness gate: [BLACK_FRAME_REGRESSION.md](BLACK_FRAME_REGRESSION.md) · G-buffer: [GBUFFER_2_0.md](GBUFFER_2_0.md)

---

## Primary architecture (locked)

| Path | Role |
|------|------|
| Deferred clustered compute | Standard opaque world |
| Forward+ clustered | Complex opaque, transparency / WBOIT, weapons |
| Specialized | Water, volumetrics, sky, decals, particles |
| Optional later | Visibility buffer |

Shared: GPU scene, materials, lights, clusters, shadows, probes, atmosphere, exposure, **BRDF** (`pbr_brdf_core.glsl`), debug.

**Preserve:** Vulkan-first, WBOIT production / MBOIT experimental, Architecture B weapons, Surf low-latency, non-TAA AA options, BSP/QVM/native/material compatibility, stable fallbacks.

---

## Immediate sprint status

| # | Item | Status |
|---|------|--------|
| 1 | Black-frame / SceneHDR validation | **Done** — `renderer_validate_frame`, `renderer_resource_status`, `renderer_capture_black_frame`, `renderer_draw_status` |
| 2 | Compact G-buffer design + bandwidth | **Design + reporting** — see GBUFFER_2_0.md; layout migration not shipping |
| 3 | Unify BRDF (Deferred / Forward+ / WBOIT) | **Core shared** — `pbr_brdf_core.glsl`; gen_frag still has local wrappers for non-light-eval paths |
| 4 | Specular AA (Toksvig + geo floor + glancing) | **Hardened** in Forward+ `ApplySpecularAA` and deferred `ApplyDeferredSpecularAA` |
| 5 | GPU scene records expansion | Existing Raster Ultra 1.6 scaffold — next: widen records (not this patch) |
| 6 | Hi-Z + indirect draw validation | Existing `vk_hiz` / `gpu_scene_status` — compare path next |
| 7 | Meshlet: one BSP class + one dynamic model | Existing `vk_meshlets` — conversion pilot next |
| 8 | Virtual-shadow design / page-table prototype | Existing Raster Ultra 1.9 `vk_vshadow` |
| 9 | Reflection-source hierarchy debug | **Not started** — planned `r_reflectionDebug` |
| 10 | Renderer laboratory expansion | Reference Lab + regression specs exist — Surf-speed suite next |

**Do not start:** ReSTIR, full ray-traced GI, neural reconstruction in this sprint.

---

## Milestone 1 commands

```text
renderer_validate_frame
renderer_draw_status
renderer_resource_status
renderer_capture_black_frame
gbuffer_bandwidth
render_path_status verbose
```

Validation checks: SceneHDR present, OIT gen match, no post-OIT G-buffer capture, exposure finite/non-zero, writer chain when opaques drew.

---

## Shared BRDF

`renderers/vulkan/shaders/glsl/pbr_brdf_core.glsl`

- Burley diffuse, GGX NDF, Smith visibility, Schlick Fresnel  
- Multiscatter energy helper  
- Specular AA + glancing roughness helpers  

Consumed by:

- `forward_plus_light_eval.glsl` (OIT / Forward+ clustered)  
- `deferred_lighting_common.glsl`  

---

## Next blockers before claiming Milestone 2–4 complete

1. Compact G-buffer encode/decode + dual-write parity.  
2. Route `gen_frag.tmpl` lighting through `pbr_brdf_core` (or generated include).  
3. LEAN / filtered roughness maps (optional Ultra).  
4. `r_reflectionDebug` hierarchy visualization.  
5. Automated Surf-speed lab path in Reference Lab.

---

## Definition of done (program)

See user roadmap Milestones 1–27. This document tracks the **immediate sprint** only. Success requires motion-stable showcase (dense Surf, lights, volumetrics, glass, weapon), not still screenshots alone.
