# Cinematic Engine Platform 1.0 — Environment Vertical Slice

**Candidate profile:** `config/modern_cinematic_raster.cfg` — **not** boot default.  
**Recovery:** `exec modern_vulkan.cfg; vid_restart`  
**Preserves:** `modern_vulkan.cfg`, `modern_raster_ultra.cfg`, mode 3 deferred opaque, Forward+ transparent/weapon, SMAA/spatial AA, classic BSP, Q3 materials, lightmaps, QVM, safe recovery.

Ray tracing and TAA remain **locked off** for this candidate. Character and destruction systems are **out of scope** until this environment slice is stable.

## Scope (first vertical slice)

| # | Capability | Status |
|---|------------|--------|
| 1 | Unified scene node registry + stable IDs | **shipped** (`vk_scene_platform`) |
| 2 | Live transform / visibility / material edit + invalidation log | **shipped** |
| 3 | Photometric unit contract + Kelvin | **shipped** (`vk_photometric`); LTC GPU upload / IES later |
| 4 | Layered materials | **opt-in via 1.8 overlay** (already existed) |
| 5 | Raster shadows | **Ultra CSM owner** (unchanged; not reworked) |
| 6 | Dynamic GI owner | **raster_gi / radiance cache** from ultra_2 |
| 7 | Reflection waterfall | **IBL owner**; SSR still off until waterfall cert |
| 8 | Volumetrics | **available**; not forced |
| 9 | Cinematic camera | **1.10 overlay** |
| 10 | Deterministic capture + `screenshotEXR` | **shipped** (display-linearized EXR) |
| 11 | Lifecycle static gates | **shipped** (`scripts/cinematic_engine_1_0_check.sh`) |

## Scene architecture

- **Authoring vs runtime:** registry holds stable `vkSceneId_t` (kind + generation + index). Classic BSP is registered as `bsp_world` but **not** GPU-scene driven.
- **Compile revision:** bumps on world load (`compileRevision`).
- **Live edits:** `scene_set_origin`, `scene_set_visible` → selective dirty flags (transform/shadow/GI/probe/light). Linked GPU instances call `vk_gpu_scene_update_instance_transform` without `vid_restart`.
- **Commands:** `scene_status`, `scene_node_status`, `scene_invalidate_debug`, `photometric_status`, `screenshotEXR`.

## Rendering (this slice)

| Signal | Owner |
|--------|--------|
| Opaque | Mode 3 deferred (ultra_2) |
| Transparent | WBOIT + Forward+ |
| AA | Spatial AA + SMAA (`modern_raster_cinematic`) |
| Lights | Classic dlights + photometric **contract**; LTC tables linked, shading opt-in |
| GI | Lightmaps + raster_gi |
| Reflections | IBL/probe; SSR off |
| Temporal / RT | **none** |

## Live editing

Supported: transform, visibility, material ID. Invalidation debugger: `scene_invalidate_debug`. Latency: same-frame registry + optional GPU instance update. Failure: refuse BSP world transform edits; inactive when `r_sceneLiveEdit 0`.

## Characters / destruction / full timeline

**Not in this slice.**

## Reliability

Static gate only. Runtime soak / resize / restart counts are **not invented** here.

## Promotion decision

| System | Class |
|--------|--------|
| Scene registry + live-edit invalidation | quality opt-in (environment candidate) |
| Photometric contract / Kelvin | quality opt-in |
| LTC area-light shading | experimental (tables present, GPU path pending) |
| Spatial AA / SMAA / Ultra 2.0 stack | as previously classified |
| `screenshotEXR` | quality opt-in (display-linearized) |
| Character / destruction / full timeline | not started |

## Highest-impact remaining environment gap

Wire LTC LUT upload into one Forward+/deferred area-light path and add a polished indoor/outdoor demo map sequence. Do not expand to character/destruction until photometric area lights and scene live-edit soak pass.
