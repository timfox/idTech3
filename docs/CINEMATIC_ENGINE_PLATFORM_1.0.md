# Cinematic Engine Platform 1.0 — Environment Vertical Slice

**Candidate profile:** `config/modern_cinematic_raster.cfg` — **NOT the boot default.**  
**Recovery:** `exec modern_vulkan.cfg; vid_restart`  
**Preserves:** `modern_vulkan.cfg`, `modern_raster_ultra.cfg`, mode 3 deferred opaque, Forward+ transparent/weapon, SMAA/spatial AA, classic BSP, Q3 materials, lightmaps, QVM, safe recovery.

Ray tracing and TAA remain **locked off** for this candidate. Character and destruction systems are **out of scope** until this environment slice is stable.

## Scope (first vertical slice)

| # | Capability | Status |
|---|------------|--------|
| 1 | Unified scene node registry + stable IDs | **shipped** (`vk_scene_platform`) |
| 2 | Live transform / visibility / material / light edit + invalidation log | **shipped** |
| 3 | Photometric unit contract + Kelvin + LTC GPU path | **shipped** (`vk_photometric`, `vk_ltc`) |
| 4 | Layered materials | **opt-in via 1.8 overlay** |
| 5 | Raster shadows | **Ultra CSM owner** (unchanged) |
| 6 | Dynamic GI owner | **raster_gi / radiance cache** from ultra_2 |
| 7 | Reflection waterfall | **IBL owner**; SSR still off until waterfall cert |
| 8 | Volumetrics | **available**; not forced |
| 9 | Cinematic camera | **1.10 overlay** |
| 10 | Deterministic capture + `screenshotEXR` | **shipped** |
| 11 | Lifecycle static gates | **shipped** (`scripts/cinematic_engine_1_0_check.sh`) |

## Scene architecture

- **Authoring vs runtime:** registry holds stable `vkSceneId_t` (kind + generation + index). Classic BSP is registered as `bsp_world` but **not** GPU-scene driven.
- **Compile revision:** bumps on world load (`compileRevision`).
- **Live edits:** `scene_set_origin`, `scene_set_visible`, `light_set_color`, `light_set_radius`, `light_spawn_area` → selective dirty flags. Linked GPU instances update without `vid_restart`.
- **Commands:** `scene_status`, `scene_node_status`, `scene_invalidate_debug`, `light_status`, `photometric_status`, `ltc_status`, `screenshotEXR`.

## Rendering (this slice)

| Signal | Owner |
|--------|--------|
| Opaque | Mode 3 deferred (ultra_2) |
| Transparent | WBOIT + Forward+ |
| AA | Spatial AA + SMAA (`modern_raster_cinematic`) |
| Lights | Classic dlights + photometric contract + **LTC rect area** (`lc.w >= 1.5`) |
| GI | Lightmaps + raster_gi |
| Reflections | IBL/probe; SSR off |
| Temporal / RT | **none** |

### Area lights (LTC)

- Pack: `dlight_t.area` or authored scene fixture → Forward+ record type `2.0`, halfU/halfV axes.
- Shade: Heitz LTC in Forward+ (`gen_frag`) and deferred lighting compute (`deferred_lighting.comp`).
- LUT: 64×64 R16G16B16A16 mat + amp uploaded by `vk_ltc`.
- Spawn: `light_spawn_area x y z halfW halfH r g b [radius]`

## Live editing

| Edit | Invalidates |
|------|-------------|
| Transform | node, GPU instance, bounds, shadows, GI, volumes (lights) |
| Visibility | shadows, GI |
| Material ID | material; probes if light-like |
| Light color/radius/area | light pack, shadows, GI, volumes |

Latency: same-frame registry + next pack. Failure: refuse BSP world transform; inactive when `r_sceneLiveEdit 0`.

## Characters / destruction / full timeline

**Not in this slice.**

## Reliability

Static gate only. Runtime soak / resize / restart counts are **not invented** here.

## Promotion decision

| System | Class |
|--------|--------|
| Scene registry + live-edit invalidation | quality opt-in (environment candidate) |
| Photometric contract / Kelvin | quality opt-in |
| LTC rect area-light shading | quality opt-in (GPU path shipped; certify on demo map) |
| Spatial AA / SMAA / Ultra 2.0 stack | as previously classified |
| `screenshotEXR` | quality opt-in (display-linearized) |
| Character / destruction / full timeline | not started |

## Highest-impact remaining environment gap

Polish an indoor/outdoor demo sequence with authored area fixtures, fog, water, and a fixed camera track; soak live-edit + map change + resize. Do not expand to character/destruction until that passes.
