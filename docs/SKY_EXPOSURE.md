# HDR sky and eye adaptation (Source-style)

**Commands:** `sky_exposure_status`, `auto_exposure_status`, `exposure_histogram_status`,
`sun_render_status`  
**Core cvar:** `r_exposure_auto 1`  
**Docs:** [HDR_SUN_EXPOSURE.md](HDR_SUN_EXPOSURE.md)

## Behavior

HDR sky writes scene-linear radiance into SceneHDR. The luminance compute pass
meters the full frame (including sky) with:

| Control | Role |
|---------|------|
| `r_exposureSkyWeight` | Weight for bright upper-frame samples (sky), ~0.55 |
| `r_autoExposure_highPercent` | Trim hottest sun percentiles (~0.05) |
| Soft reject | Samples > trimmed median + 4 stops (sun core) |
| `r_autoExposure_centerWeight` | Center bias |
| `r_autoExposure_speedDown` | Faster when exposure must **decrease** (look at sky) |
| `r_autoExposure_speedUp` | Slower when exposure must **increase** (look away) |
| `r_exposure_auto_target` | Desired mid-grey (~0.18 Source-like) |
| `r_autoExposure_min` / `max` | HDR clamp (0.05 … 12 for sky maps) |
| `r_bloomThresholdEVRelative` | Bloom threshold vs exposed luminance |

Adapted exposure (`vk.adaptedExposure`) is applied once in the shared post chain
before tonemap — sky is **not** exposed a second time.

## Map enable

`SkyboxHDR_ConfigureFromMap` / `SkyboxHDR_EnableEyeAdaptation` configures the
curve; `r_skyboxHDR_autoExposure 1` (default for HDR maps) turns AE on
(e.g. `surf_aztec` mapscript).

See also [HDR_SKY_RENDERING.md](HDR_SKY_RENDERING.md), [COLOR_PIPELINE.md](COLOR_PIPELINE.md).
