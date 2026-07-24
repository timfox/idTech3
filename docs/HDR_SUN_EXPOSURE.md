# HDR Sun + Source-style Auto Exposure

**Goal:** Keep automatic eye adaptation, but meter from a percentile-clipped
SceneHDR histogram so the tiny sun core cannot force the whole frame’s EV.

## Authoritative sun

| Policy | Meaning |
|--------|---------|
| `SUN_SOURCE_CUBEMAP` (`r_sunSource 1`) | EXR/HDR sky already contains the visible sun (Surf aztec) |
| `SUN_SOURCE_PROCEDURAL` | Render one analytical HDR disc (~0.27°) |
| Do not | Stack full-radiance cubemap sun + procedural disc |

Glow belongs to **bloom** (and optional atmosphere), not an enlarged disc.
Diffraction (`r_sunDiffraction`, default **0**) and lens flare (`r_lensFlare`,
default **0**) are optional lens artifacts — not physical sun energy, and not
fed into metering.

## Metering pipeline

```
SceneHDR (pre-bloom, pre-UI)
  → log2 luminance (16×16 sparse)
  → bitonic sort + low/high percentile trim
  → soft reject samples > median + 4 stops (sun core)
  → sky-weighted average (r_exposureSkyWeight)
  → target EV → asymmetric temporal adaptation → adaptedExposure
  → single application at tonemap
```

| Cvar | Role |
|------|------|
| `r_exposure_auto` | Enable AE |
| `r_autoExposure_lowPercent` / `highPercent` | Percentile clip (~5%/5%) |
| `r_exposureSkyWeight` | Broad sky influence (~0.55) |
| `r_autoExposure_speedDown` | Darken into bright (faster) |
| `r_autoExposure_speedUp` | Brighten into dark (slower) |
| `r_bloomThresholdEVRelative` | Bloom knee vs exposed luma |

## Commands

`sun_render_status`, `sun_composition_status`, `sun_contributor_validate`,
`sun_source_status`, `auto_exposure_status`, `auto_exposure_validate`,
`auto_exposure_reset` / `freeze` / `unfreeze`, `exposure_contract_status`,
`bloom_exposure_status`, `lens_flare_status`, `eye_adaptation_exposure_test`

## Hard rules

- Keep automatic exposure (do not ship permanent fixed EV for HDR sky maps).
- Do not exclude the entire sky from metering.
- Do not let a few sun-core pixels dictate target EV.
- Do not clamp SceneHDR to 0–1 before bloom/tonemap.
- Do not apply exposure twice.
- Do not change WBOIT equations.

See also [SKY_EXPOSURE.md](SKY_EXPOSURE.md), [HDR_SKY_RENDERING.md](HDR_SKY_RENDERING.md).
