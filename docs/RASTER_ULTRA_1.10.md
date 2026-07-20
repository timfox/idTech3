# Raster Ultra 1.10 — HDR Presentation + Cinematic Camera + Professional Color Pipeline

Continuation of [RASTER_ULTRA_1.9.md](RASTER_ULTRA_1.9.md). **RT remains completely disabled.**

**Certification:** experimental / opt-in. Boot stays `modern_vulkan.cfg`. Ultra base does **not** force HDR display, auto-exposure, or cinematic DOF/MB — use the overlay.

Post-processing must **not** conceal incorrect lighting or material energy.

## Enable

```
exec modern_raster_ultra.cfg
exec vulkan_overlay_raster_ultra_1_10_hdr_presentation.cfg
vid_restart
```

Commands: `present_color_status`, `exposure_histogram_status`, `cinematic_camera_status`, `capture_pipeline_status`, `color_grade_status`

## Scene-linear contract

| Term | Definition |
|------|------------|
| Working space | Scene-linear, Rec.709 primaries, relative radiance |
| Pre-exposure | `r_pre_exposure_scale` applied once before tonemap |
| Display | Separate transform (SDR sRGB / HDR10 PQ / scRGB when WSI allows) |
| Double gamma | Forbidden — Policy A in `vk_device.c` + `apply_srgb_gamma` |

Module: `vk_present_color.*`

## Exposure

`vk_exposure_histogram.*` hardens `r_exposure_auto`:

- Meter modes: average / center / spot / histogram
- EV compensation + min/max EV
- Asymmetric adaptation (darken faster)
- Soft sky weighting
- Reset on camera cut, map change, focus recovery
- Fixed-exposure mode

Local exposure (`r_localExposure*`) stays off in stable profile until certified.

## Display transforms

`r_presentColor`:

| Value | Mode |
|-------|------|
| 0 | SDR sRGB (certified) |
| 1 | SDR wide (policy fallback) |
| 2 | HDR10 PQ when ST2084 available |
| 3 | scRGB when EXTENDED_SRGB_LINEAR available |

Paper white / peak / UI reference white nits configurable. Unavailable HDR requests keep SDR.

## Tonemap / grade

- Filmic (3) and AgX (4) via `r_tonemap` / `r_presentTonemapPreference`
- Grade ownership: `vk_color_grade` → existing `r_grade_*` + LUT
- Bloom remains energy-knee controlled (`r_bloomKnee`) — not a lighting substitute

## Cinematic camera

`vk_cinematic_camera.*`: physical focal length, sensor, aperture, focus, shutter angle, ISO, anamorphic.

- Optional DOF / motion blur (opt-in; low-latency disables MB)
- **UI never blurred** (compose after tonemap)
- Weapon exclusion default on
- **No frame generation** — current-frame motion vectors only

## Presentation order

```
world temporal recon → weapon → bloom → (optional DOF/MB) → tonemap → grade → UI → display
```

SMAA / adaptive recon remain before luminance/gamma. Classic SDR fallback when HDR WSI absent.

## Capture

`vk_capture_pipeline.*`:

| `r_captureColorSpace` | Meaning |
|----------------------|---------|
| 0 | Display SDR (default screenshots) |
| 1–3 | HDR/linear intents — **block silent 8-bit SDR** when `r_captureBlockHdrToSdr 1` |

Deterministic capture can freeze grain + fixed exposure.

## Validation

```
./scripts/raster_ultra_1_10_check.sh
```

Manual: dark→bright doorway, AgX vs filmic, HDR10 request on SDR monitor (fallback), screenshot with colorSpace 1 (blocked), cinematic DOF without HUD blur, `vid_restart`.

## Promotion decision

| Item | Status |
|------|--------|
| Scene-linear contract documented | **yes** |
| Histogram metering | **yes** |
| HDR10/scRGB probe + select | **yes** (fallback to SDR) |
| AgX / filmic | **yes** (existing gamma.frag) |
| Capture HDR→SDR guard | **yes** |
| Cinematic camera + UI exclusion | **yes** |
| Full PQ encode in gamma.frag | **partial** — WSI select live; PQ OETF expansion continues |
| Local exposure certified | **no** — keep off |
| Promote to Ultra default | **no** — overlay only |
| Boot unchanged | **yes** |

## Highest-impact fix

Screenshots could silently encode HDR-intent modes into 8-bit SDR. Ultra 1.10 **blocks silent HDR→SDR** and documents display vs scene-linear capture spaces. Second: exposure history now **resets on cut/map** via histogram controller so incompatible maps do not pump.

Next: [RASTER_ULTRA_1.11.md](RASTER_ULTRA_1.11.md) — Reference Lab + automated visual certification (no new techniques).
