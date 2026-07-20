# Raster Ultra 2.0 — Production Hardening + Quality Parity + Certification

**Not a new rendering technique milestone.** Unifies ownership, hardens lifecycle, certifies combinations, and documents the production frame contract from **actual code**.

**RT remains optional and locked off under Raster Ultra.** Boot fallback stays `modern_vulkan.cfg`.

**Candidate profile:** `config/modern_raster_ultra_2.cfg` — **internal candidate** (not boot default).

## Production frame contract (authoritative)

Observed order for Raster Ultra / mode 3 (`RB_DrawSurfs` → `vk_end_frame`):

| # | Pass | Spine ID | View | Notes |
|---|------|----------|------|-------|
| 1 | Frame prep | `FRAME_PREP` | any | `vk_spine_frame_begin` |
| 2 | Sun CSM | `SUN_SHADOW` | main | Raster cascades |
| 3 | Light pack + tile cull | `LIGHT_PACK`, `TILE_CONSTRUCT` | main | Clustered lists |
| 4 | Opaque geometry | `WORLD_OPAQUE` (via `main` RP) | main | Lightmaps in HDR |
| 5 | G-buffer capture | `GBUFFER_FILL` | main | After opaque |
| 6 | Deferred decals | (stamp TBD) | main | Onto G-buffer |
| 7 | Ambient visibility (GTAO) | `AMBIENT_VISIBILITY` | main | Before deferred lights |
| 8 | Deferred lighting | `DEFERRED_LIGHTING` | main | Dynamic direct owner |
| 9 | WBOIT accum/resolve **or** Forward+ transparent | `WBOIT_*` / `OIT_RESOLVE` | main | Transparent owner |
| 10 | Refractive / water | sorted after OIT | main | Not OIT |
| 11 | Raster GI (probe+cache+SSGI) | `RASTER_GI` | main | Diffuse GI delta |
| 12 | Particles / distortion | — | main | Effects |
| 13 | SSR (if enabled) | `SSR` | main | Ultra 2.0 default **off** |
| 14 | Bloom | `BLOOM` | main | |
| 15 | Volumetrics (if enabled) | `FROXEL_VOLUME` | main | |
| 16 | SMAA / temporal | `SMAA` / `TEMPORAL_RECON` | main | Ultra 2.0: SMAA |
| 17 | Weapon | `WEAPON` | weapon | After world AA |
| 18 | Exposure | `EYE_ADAPTATION` | main | |
| 19 | Presentation / gamma | `PRESENTATION` | any | |
| 20 | History maint | `HISTORY_MAINT` | any | Frame end + Ultra contract check |

Weapon and UI never write world TAA history. Unresolved OIT never enters history.

## Signal ownership

| Signal | Owner | Fallback |
|--------|--------|----------|
| Opaque lighting | Mode 3 deferred | Forward+ / classic |
| Transparent lighting | WBOIT + Forward+ tiles | Sorted Forward+ |
| Shadows | Raster CSM (+ local atlas when enabled) | Unshadowed |
| Diffuse GI | Lightmaps (static) + `raster_gi` probe/cache delta | SH/IBL in forward |
| Ambient visibility | GTAO (`r_ambientVisibilityMode 2`) | none (`r_ssao` must stay 0) |
| Reflections | IBL/probe in Forward+ | SSR opt-in; RT locked |
| Water | Refractive sorted path | Opaque fallback |
| OIT | WBOIT (`r_oit 1`) | Sorted alpha |
| AA | SMAA (`r_aaMode 2`) | none |
| Exposure | Luminance / optional histogram overlay | Fixed |
| Presentation | Gamma → swapchain | — |

## Consolidation fixes (this ship)

1. **Deferred lighting** registered in Spine (`VK_SPINE_PASS_DEFERRED_LIGHTING`).
2. **AV / RASTER_GI phase tags** aligned with execution (AV = opaque lighting; GI = post, after OIT).
3. **`havenrp_renderer_status`** reports effective `raster_gi` / opaque / transparent / OIT / AA / exposure / spine contract.
4. **Ultra frame contract** check at `vk_spine_frame_end` (`vk_spine_validate_ultra_frame_contract`).
5. **Thin-wall GI leak**: denser occupancy, segment solid test between clipmap neighbors, stronger leak mute in sample + resolve.

## Material parity

| Path | Status |
|------|--------|
| Deferred vs Forward+ BRDF | Shared intent via `deferred_lighting_common.glsl` ↔ `gen_frag.tmpl` (parallel, not single include yet) |
| Weapon | Forward+ full BRDF |
| WBOIT | Simplified lighting — **quality opt-in residual** |
| Reflection / SSR | IBL owner; deferred terminal IBL still a known gap |
| Legacy stages | Preserved under PBR translation |

## Reliability / visual / performance

Lifecycle soaks, image metrics, and GPU timings are **not invented** here. Static gates: `scripts/raster_ultra_2_0_check.sh`. Runtime soak remains a promotion requirement for production-ready.

## Promotion table

| Subsystem | Class |
|-----------|--------|
| Mode 2 + SMAA + CSM + GTAO + lightmaps | **certified stable** |
| Mode 3 deferred opaque | **Raster Ultra certified** (contract-stamped) |
| Forward+ transparent / weapon | **Raster Ultra certified** |
| WBOIT | **Raster Ultra certified** (MBOIT experimental) |
| Probe GI + radiance cache | **quality opt-in** (thin-wall hardened; soak pending) |
| SSGI | **quality opt-in** |
| SSR | **quality opt-in** (off in Ultra 2.0 candidate) |
| Adaptive reconstruction / TAA | **quality opt-in** (not in candidate) |
| Virtual shadows / materials 1.8 / atmosphere / terrain 1.14 | **quality opt-in** overlays |
| Hybrid1 / path tracing / neural GI / MBOIT | **experimental** |
| Frame generation | **rejected** (forbidden) |

## Release candidate decision

`modern_raster_ultra_2.cfg` = **internal candidate**.

Not production-ready opt-in until: long soak, measured budgets, WBOIT×lifecycle GPU cert, deferred IBL parity, and material-include unification land.

## Enable

```
exec modern_raster_ultra_2.cfg
vid_restart
havenrp_renderer_status
pass_registry_status
```

Recovery: `exec modern_vulkan.cfg; vid_restart`

## Highest-impact next fixes

1. Extract shared `pbr_brdf_common.glsl` (deferred ↔ Forward+)
2. Deferred terminal specular IBL
3. WBOIT material-parity lighting subset
4. Automated pairwise combo matrix + 60-minute Ultra soak
