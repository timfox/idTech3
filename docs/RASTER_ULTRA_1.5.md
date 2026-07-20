# Raster Ultra 1.5 — Present-Time Adaptive Reconstruction

Continuation of [RASTER_ULTRA_1.4.md](RASTER_ULTRA_1.4.md). **RT remains locked off. Frame generation remains off.**

**Certification:** experimental / quality opt-in. Boot default stays `modern_vulkan.cfg` → SMAA (`r_aaMode 2`). Ultra profile keeps **SMAA** unless the adaptive overlay is exec'd.

## Enable

```
exec modern_raster_ultra.cfg
exec vulkan_overlay_present_adaptive_recon.cfg
vid_restart
```

Recovery: `seta r_aaMode 2; seta r_taa 0; vid_restart`

## AA modes (`r_aaMode`)

| Mode | Role |
|------|------|
| 0 | none |
| 1 | FXAA compatibility |
| 2 | **SMAA 1x** — certified zero-history (shipping / Ultra default) |
| 3 | **Present-Time Adaptive Reconstruction** (migrated from SMAA T2x) |
| 4 | Temporal Reconstruction (native; temporal upscale when render scale &lt; 1) |
| 5 | Temporal Reconstruction + SMAA cleanup |
| 6 | Spatial supersampled reference (`r_ext_supersample`) |

Conceptual Ultra “mode 5 = SS reference” maps to **`r_aaMode 6`** so mode 5 cleanup configs are not silently broken.

**Migration:** former mode 3 (SMAA T2x = SMAA + light temporal) is now current-frame-first adaptive recon. Use mode **2** for SMAA-only.

## Estimator design

Resolve runs after post-fog on the **current simulation frame**:

1. Sample current HDR color (+ optional edge-aware 5-tap spatial when difficult).
2. Reproject history via motion vectors (`currUV - prevUV`) or depth/`prevViewProj`.
3. Confidence gate → YCoCg neighborhood clip → mix with strict history cap.
4. No intermediate / interpolated presentation frame is synthesized.

## Confidence model

Product of:

| Factor | Source |
|--------|--------|
| depth match | current vs reprojected history depth (tighter thresholds in adaptive) |
| motion validity | NaN/Inf / missing MV soft→hard reject |
| luminance agreement | current vs history luma delta |
| reactivity | near-weapon, flash, highlight ghost, history-bleed, stamped OIT/particles/distortion |
| velocity | motion length in pixels |

Hard reject: `reactive > 0.65` (adaptive) / `0.82` (mode 4); `depthConf < 0.35` (adaptive) → immediate current-frame spatial.

## History caps (adaptive)

| Cap | Default |
|-----|---------|
| `r_presentAdaptiveHistoryCap` | **0.42** |
| stationary feedback | **0.55** |
| motion feedback | **0.28** |
| resolve feedback max | **0.42** |

Mode 4 Temporal Reconstruction keeps higher caps (overlay weight ~0.68).

## Disocclusion

Depth delta + skyline heuristic + reactive. Newly revealed pixels use **current-frame spatial reconstruction** — no long recovery window.

## SMAA role

| Use | Path |
|-----|------|
| Certified zero-history | `r_aaMode 2` |
| Edge-class spatial fallback | Adaptive resolve `spatialCurrentFallback` on reject / difficult pixels |
| Full-frame post-TAA SMAA | **Not** applied in mode 3 (avoids double-blur) |
| Optional cleanup | mode 5 only |

## Adaptive current-frame sampling

- Budget: `r_presentAdaptiveBudget` (default **0.15**) — fraction of difficult pixels.
- Method: edge-aware current neighborhood (not full-frame supersample).
- Toggle: `r_presentAdaptiveSpatial`.
- Debug: `r_debugHistoryRejection 9` / `r_debugAdaptiveSampleMask 1`.

## Weapon / UI

- World reconstruct first; weapon deferred (`r_temporalWeaponAfterTaa 1`).
- Weapon outside `taa_history`; local spatial only via post-draw path.
- UI: output resolution, no world jitter/history/MV/recon blur.

## History ownership

Dedicated `taa_history` ping-pong. **Not** shared with SSR, SSGI, GTAO, volumetrics, shadows, particles, path tracing, weapon, or UI.

Portals/mirrors: camera-cut / separate view family — no main-world history reuse.

## Latency instrumentation

```
present_recon_status
```

Reports:

- `frame_generation = off`
- `interpolated_frames = 0`
- `presentation_source = current_simulation_frame`
- CPU begin / submit / present timestamps (not claimed input-to-photon)

## Motion vectors

```
motion_vector_cert
```

| Item | Value |
|------|-------|
| Units | normalized UV delta |
| Encoding | `out_motion = currUV - prevUV` |
| History UV | `sampleUV - motion` |
| Format | `R16G16_SFLOAT` |
| Invalid | NaN/Inf → matrix fallback; OOB → current only |

Coverage: camera/rigid CERT; skinned PARTIAL; particles/water WEAK (reactive); weapon OUTSIDE; portals RESET.

## Debug views (`r_debugHistoryRejection`)

0 off · 1 MV · 2 reasons · 3 reactive · 4 confidence · 5 disocclusion · 6 history UV · 7 near-weapon · 8 world/reactive · **9 adaptive sample** · **10 curr/hist** · **11 neigh var** · **12 hist delta**

## Image metrics (objective checklist)

Measure vs mode 6 SS reference: edge shimmer, subpixel stability, disocclusion recovery, ghost duration, weapon trail, transparency trail, specular trail, alpha-test stability, blur, oversharpening, extra-sample cost, latency counters.

## Static gate

```
./scripts/raster_ultra_1_5_check.sh
```

## Promotion decision

| Item | Status |
|------|--------|
| SMAA certified path intact | **yes** (Ultra + boot stay mode 2) |
| Adaptive current-frame-first | **yes** (mode 3 overlay) |
| Frame generation | **off** |
| Weapon outside history | **yes** |
| Reactive from OIT/particles | **yes** (1.4 stamp + heuristics) |
| Separate history ownership | **yes** (`taa_history`) |
| Adaptive sampling | **bounded spatial** (not full-frame SS) |
| MV certification | **documented + debug** |
| Promotion to shipping default | **no** — remains opt-in overlay |

## Highest-impact ghosting fix (this milestone)

Adaptive path: tighter depth/luma/velocity reject, reactive hard-cut at **0.65**, history feedback capped at **0.42**, immediate spatial on disocclusion — targets skyline / banner / transparent trails under rapid camera motion without delaying the simulation frame.
