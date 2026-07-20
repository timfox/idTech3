# Raster Ultra 1.11 — Rendering Reference Lab + Automated Visual Certification

Continuation of [RASTER_ULTRA_1.10.md](RASTER_ULTRA_1.10.md). **No new rendering techniques.** RT remains off.

**Certification:** experimental / opt-in. Boot stays `modern_vulkan.cfg`. Ultra base does **not** force the lab — use the overlay with `modern_raster_reference.cfg` for material/lighting baselines.

## Enable

```
exec modern_raster_reference.cfg
exec vulkan_overlay_raster_ultra_1_11_reference_lab.cfg
vid_restart
```

Commands: `reference_lab_status`, `reference_lab_scenes`

Host automation:

```
./scripts/raster_ultra_1_11_check.sh
./scripts/raster_ultra_lab/run_all.sh
```

## Objective

Turn Raster Ultra 1.0–1.10 into **measurable, reproducible, promotable** technology via:

- deterministic lab mode (`vk_reference_lab`)
- fixed scene catalog + camera bookmarks
- lighting decomposition intents
- spatial supersample reference modes (2×/4×/8×, no temporal)
- host image metrics (RMSE / PSNR / SSIM)
- artifact detectors
- lifecycle + combination matrices
- evidence reports with reproduction commands

## Reference scenes

`reference_lab_scenes` lists 23 scenes (material spheres → weapon/UI). Each has map/cfg hints and up to three bookmarks. GPU geometry may use external `rtest_*.bsp` pack ([samples/renderer_regression](samples/renderer_regression/README.md)) or demo cfgs.

## Deterministic execution

When `r_referenceLab 1`:

- fixed seed (`r_referenceLabSeed`)
- film grain / CA off
- optional freeze exposure + capture deterministic
- spatial SS modes force `r_taa 0` / `r_aaMode 0`
- material/lighting/presentation modes disable bloom + auto-exposure

## Metrics

```
python3 scripts/raster_ultra_lab/metrics/compare_frame.py --ref a.ppm --test b.ppm
python3 scripts/raster_ultra_lab/metrics/detect_artifacts.py capture.ppm
```

Reports: absolute error, RMSE, PSNR, SSIM, edge error, perceptual RGB Δ proxy. Thresholds: `scripts/raster_ultra_lab/baselines/thresholds.json` (noise-tolerant; do not block tiny nondeterministic deltas outside thresholds).

## Artifact detection

Black/solid frames, horizontal bands, checkerboard corruption, black crush, overexposure heuristics on captures. Engine-side NaN/inf/descriptor/generation mismatches remain covered by existing validation layers + status commands.

## Lifecycle matrix

Static wiring for cut/map/exposure resets + optional GPU `vid_restart` when `RASTER_ULTRA_LAB_GPU=1`.

Documented cases: cold boot, map load, map switch, profile switch, resize, minimize, restore, fullscreen, monitor change, focus loss, focus regain, `vid_restart`, shader reload, material reload, streaming pressure, allocation failure, clean shutdown.

## Combination matrix

- Mandatory: boot must not force lab; reference profile no TAA/RT/bloom; overlay RT-off + grain off
- Pairwise: Ultra 1.6–1.11 overlays each keep `r_hybrid1 0`
- Forbidden: OIT+TAA in lab overlay
- Three-way: documented as nightly (materials × shadows × presentation) under release certification

## CI tiers

| Tier | Scope |
|------|-------|
| **Per-commit** | `raster_ultra_1_11_check.sh` + lab static matrices (wired into `renderer_regression_check` optionally) |
| **Pre-merge** | material parity, shadows, GI, reflections, water, transparency, AA, lifecycle |
| **Nightly** | pairwise Ultra overlays, weather/GI/transparency/streaming soak |
| **Release** | all profiles, multi-GPU, X11/Wayland, SDR/HDR, long soak |

## Temporal sequences

Catalog: `scripts/raster_ultra_lab/lab_temporal_sequences.sh` — covers:

- fast_camera_pan / slow_camera_pan
- camera_cut
- disocclusion
- moving_weapon / moving_reflective_object / moving_shadow_caster
- particles / water / foliage
- emissive_flicker
- weather_transition / exposure_transition
- portal_view

Host metrics consume multi-frame captures when `RASTER_ULTRA_LAB_GPU=1`. Measure temporal variance, luminance stability, ghost-trail duration, disocclusion recovery, shadow shimmer, reflection flicker, cloud-history error, LOD popping, water-edge artifact score against `thresholds.json`.

## Performance baselines

Store GPU-class notes under `scripts/raster_ultra_lab/baselines/` (`gpu_classes.json` template: CPU/GPU ms, memory, draws, triangles, meshlets, cluster occupancy, shadow pages, probes, particles, froxels, texture residency, streaming bandwidth, pipeline count, shader cache hit rate). Populate from `havenrp_renderer_status` / pass timers on Tier B runners.

## Reports

`lab_report.sh` writes Markdown with git revision, reproduction commands, JSON metric dumps, CI tier table, blind spots.

## Promotion decision

| Item | Status |
|------|--------|
| Reference scenes + bookmarks | **yes** |
| Deterministic lab mode | **yes** |
| Spatial / material / lighting / presentation modes | **yes** (intent + pins) |
| Image metrics | **yes** (host Python) |
| Artifact detection | **yes** |
| Lifecycle + combination matrices | **yes** (static; GPU optional) |
| CI tiers defined | **yes** |
| Full GPU golden pack in-repo | **no** — external rtest pack |
| Promote lab to boot | **no** |
| Boot unchanged | **yes** |

## Highest-impact outcome

Promotion is no longer anecdote-only: Ultra features gain a **shared deterministic lab**, **image metrics**, and **matrix automation** so shipping decisions are evidence-based.
