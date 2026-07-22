# id Tech 7–class Renderer Sprint

**Date:** 2026-07-22  
**Principle:** Strengthen raster, material, lighting, geometry, and image formation **before** ReSTIR / full RT GI / neural reconstruction.

Canonical ownership: [RENDERER_PATH_OWNERSHIP.md](RENDERER_PATH_OWNERSHIP.md) · Correctness gate: [BLACK_FRAME_REGRESSION.md](BLACK_FRAME_REGRESSION.md) · G-buffer: [GBUFFER_2_0.md](GBUFFER_2_0.md)

**Foundation Consolidation (2026-07):** Shared foundation docs + smoke tests supersede isolated sprint notes for frame contract, GPU scene, BRDF, shadows, reflections, indirect, HDR, and reference lab. Hub: run `tests/scripts/test_foundation_consolidation.sh`. Docs: [RENDERER_FRAME_CONTRACT.md](RENDERER_FRAME_CONTRACT.md), [GPU_SCENE.md](GPU_SCENE.md), [GPU_DRIVEN_RENDERING.md](GPU_DRIVEN_RENDERING.md), [GBUFFER_2.md](GBUFFER_2.md), [SHARED_BRDF.md](SHARED_BRDF.md), [SPECULAR_AA.md](SPECULAR_AA.md), [SHADOW_CONTRACT.md](SHADOW_CONTRACT.md), [REFLECTION_HIERARCHY.md](REFLECTION_HIERARCHY.md), [INDIRECT_LIGHTING.md](INDIRECT_LIGHTING.md), [HDR_PIPELINE.md](HDR_PIPELINE.md), [RENDERER_LAB.md](RENDERER_LAB.md).

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
| 1b | Frame contract (FC Phase 1) | **Done** — `renderer_frame_status`, `renderer_capture_frame_contract`, per-resource history + black-frame class |
| 2 | Compact G-buffer design + bandwidth | **Prep** — octahedral helpers, `r_gbufferCompact` dual-write, `gbuffer_bandwidth` scaffold vs compact + Forward+ fallback % |
| 3 | Unify BRDF (Deferred / Forward+ / WBOIT) | **Done** — `pbr_brdf_core.glsl` in Forward+/OIT, deferred, and `gen_frag.tmpl` |
| 4 | Specular AA (Toksvig + geo floor + glancing) | **Hardened** — shipping baseline; `r_pbr_specularAA` kill switch; no LEAN this sprint |
| 5 | GPU scene records expansion | **Done** — prevTransform, objectId, temporalGeneration, shadowFlags, renderFlags; `r_gpuDrawCompare`; `gpu_scene_layout` |
| 6 | Hi-Z + indirect draw validation | **Done** — compute downsample (`hiz_downsample.comp`), conservative sample, `hiz_status` / `depth_status` / `gpu_scene_status` |
| 7 | Meshlet: one BSP class + one dynamic model | **Done** — `r_meshletsBspPilot` (SF_FACE), `r_meshletsModelPilot` (static MD3; animated skip) |
| 8 | Virtual-shadow page fill | **Scaffold** — `vshadow_status` / `shadow_status`; CSM fallback default (full virtualization is next milestone) |
| 8b | Deferred multi-cascade CSM | **Done** — `ShadowContract_SampleCSM` select+blend via SSBO records; parity with Forward+ splits |
| 9 | Reflection-source hierarchy debug | **Done** — `r_reflectionDebug` aliases `r_shrDebug` source IDs |
| 10 | Renderer laboratory expansion | **Done** — `VK_REFLAB_SCENE_SURF_SPEED` + `demo_reference_lab_surf_speed.cfg` + `test_reference_lab_surf_speed.sh` |
| FC | Foundation Consolidation hub | **Gates green** — `tests/scripts/test_foundation_consolidation.sh` + `test_foundation_unit.sh` |


**Safe defaults:** `r_gpuScene 0`, `r_gpuDriven 0`, `r_hiZ 0` — classic BSP draws remain authoritative until GPU path is enabled deliberately.

**Do not claim:** ReSTIR, full ray-traced GI, neural reconstruction, full G-buffer layout cutover, replacing BSP default draws, M5–M27 program milestones.

---

## Milestone 1 commands

```text
renderer_validate_frame
renderer_draw_status
renderer_resource_status
renderer_capture_black_frame
gbuffer_bandwidth
render_path_status verbose
gpu_scene_status
hiz_status
meshlet_status
vshadow_status
```

Validation checks: SceneHDR present, OIT gen match, no post-OIT G-buffer capture, exposure finite/non-zero, writer chain when opaques drew.

---

## Shared BRDF

`renderers/vulkan/shaders/glsl/pbr_brdf_core.glsl`

Consumed by:

- `forward_plus_light_eval.glsl` (OIT / Forward+ clustered)  
- `deferred_lighting_common.glsl`  
- `gen_frag.tmpl` (opaque Forward+ shade)

Regression: `tests/scripts/test_pbr_brdf_core.sh`

---

## Specular AA baseline

Toksvig + geometric floor + glancing roughness. Optional Ultra LEAN deferred. Kill switch: `r_pbr_specularAA`.

---

## Opt-in overlays (defaults safe)

| Cvar | Default | Notes |
|------|---------|-------|
| `r_gbufferCompact` | 0 | Dual-write oct into material.ba; lighting uses scaffold AO when compact |
| `r_gpuScene` / `r_hiZ` | 0 | GPU-driven path + Hi-Z; classic draws remain default |
| `r_gpuDrawCompare` | 0 | Classic vs GPU cull metrics |
| `r_meshletsBspPilot` | 0 | BSP face meshlet pilot |
| `r_meshletsModelPilot` | 1 | Static MD3 when `r_meshlets` 1 |
| `r_reflectionDebug` | 0 | Hierarchy false-color (cheat) |
| `r_vshadowFallbackCsm` | 1 | Keep CSM sampling until page residency proven |

---

## Definition of done (program)

See user roadmap Milestones 1–27. This document tracks the **immediate sprint** only. Success requires motion-stable showcase (dense Surf, lights, volumetrics, glass, weapon), not still screenshots alone.
