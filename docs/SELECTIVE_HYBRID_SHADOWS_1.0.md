# Selective Hybrid Shadows 1.0

**Status:** Implementation landed — **GPU certification pending**.

Does **not** change the certified raster spine or `modern_vulkan.cfg` boot defaults.

## Target configuration

| Signal | Owner |
|--------|-------|
| Primary visibility | Raster (Unified Clustered / mode 3–4) |
| Clustered direct lighting | Raster Forward+ / deferred |
| **Sun shadow visibility** | **Hybrid1 RT or ray-query** (exclusive) |
| Local-light shadows | Raster only |
| AO | GTAO (Ambient Visibility) |
| GI / lightmaps | Lightmaps + SH/IBL |
| Transparency | Optional WBOIT (`r_oit 1`) |
| Presentation AA | SMAA (`r_aaMode 2`) |
| Weapon | Outside temporal histories |
| UI | After tonemapping |

Out of scope for this milestone: RT reflections, RT GI, path tracing, new AA, frame generation.

## Ownership rules

1. Sun shadow has **exactly one** owner per frame: `raster` or `hybrid1_rt`.
2. Declared via `r_havenrpSunShadowOwner` (`auto|raster|hybrid1_rt`).
3. RT ownership requires TLAS ready + RT pipeline/ray-query healthy + descriptors/history healthy.
4. Failures demote to raster and set `r_havenrpFallbackReason` (visible in `havenrp_renderer_status` / `shs_status`).
5. Path-traced reference / `r_pathtrace` **blocks** SHS composition.
6. Raster cascade generation and `pbrSunShadowParams` are suppressed when RT owns sun.
7. Never multiply raster × RT sun visibility.
8. Local lights never use RT shadows under SHS (`r_hybrid1_dlightShadows` forced 0 in UBO).

## RT geometry coverage

- Shared Hybrid1 / RTX TLAS: world BSP + entity BLAS when `r_rtxEntities` enabled.
- Movers / dynamic entities: included when packed into TLAS; TLAS revision invalidates shadow history.
- Alpha-tested foliage/fences: world/entity BLAS built **non-opaque**; Hybrid1 shadow hit group runs `hybrid1_shadow.rahit` when `RTX_PRIM_MATERIAL_FLAG_ALPHA_TEST` is set from `GLS_ATEST_*` stages (bindless diffuse alpha < 0.5 → ignore). Default raw path is Hybrid1 RT (not rayQuery) so any-hit runs. Ray-query opt-in (`r_shsPreferRayQuery 1`) still cannot execute any-hit — confirms all candidates.

## Raw visibility

Preferred path when `vk.rayQueryAvailable`: compute `selective_hybrid/shs_sun_shadow.comp`.

Fallback: Hybrid1 `hybrid1_shadow.rgen` (KHR ray-tracing pipeline).

Per shaded world pixel:

- Reconstruct world position from depth
- Load G-buffer normal
- Robust origin offset along normal + light
- Trace toward directional light
- Binary visibility + hit distance + validity

## Denoiser

Dedicated Hybrid1 shadow history (`hist_shadow` / `var_shadow` / `filtered_shadow`) — **not** world `taa_history`.

SHS temporal mode:

- Motion-vector / reprojection
- Normal + hit-distance discontinuity rejection
- Finite max history age (`r_shsMaxHistoryAge`)
- Alpha floor (`r_shsTemporalAlphaFloor`) — prefer a little noise over ghosting
- Edge-aware A-trous spatial filter (existing Hybrid1)

Composite under SHS darkens an **estimated sun term** (albedo × N·L × sun intensity), not full HDR (preserves local lights / lightmaps).

## Debug (`r_shsDebug` / `r_hybrid1_debug`)

| Mode | View |
|-----:|------|
| 1 | Raw RT visibility |
| 2 | Filtered visibility |
| 3 | Difference proxy |
| 4 | Ray hit distance |
| 5 | TLAS / validity coverage |
| 6 | Alpha-candidate count |
| 9 | Temporal history weight |
| 10 | Rejection reason |
| 11 | Final filtered shadow |

## Fail injection (`r_shsFailInject`)

| Bit | Failure |
|----:|---------|
| 1 | TLAS |
| 2 | RT pipeline |
| 4 | Descriptor |
| 8 | History allocation / validity |

## Enable

```text
exec vulkan_overlay_selective_hybrid_shadows.cfg
vid_restart
```

Recovery: `exec modern_vulkan.cfg`

Console: `shs_status`, `hybrid1_status`, `havenrp_renderer_status`

## Static gate

```bash
./scripts/selective_hybrid_shadows_1_0_check.sh
```

## GPU certification checklist (pending)

- Menu, oa_minia, ten classic maps, indoor/outdoor
- Alpha-tested fences/foliage, movers, rapid camera, weapon visible
- WBOIT off/on, SMAA
- Resize ≥50, vid_restart ≥20, focus ≥20, map transitions ≥20
- RT forced off + fail injects
- 30-minute camera soak
- No validation errors, ownership violations, double shadowing, DEVICE_LOST, persistent trails
- Raster fallback always works; resource counts return to baseline; GPU timings reported

## Certification decision

**Not certified yet** — static contracts + implementation complete; GPU soak and alpha-tested coverage remain open. Highest-impact follow-up: alpha-tested any-hit / non-opaque TLAS geometry for foliage/fences.
