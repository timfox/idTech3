# Raster Ultra 1.7 — Physical Atmosphere + Clouds + Weather + Volumetric Lighting

Continuation of [RASTER_ULTRA_1.6.md](RASTER_ULTRA_1.6.md). **RT remains completely disabled.**

**Certification:** experimental / opt-in. Boot stays `modern_vulkan.cfg`. Ultra base does **not** force physical sky — use the overlay. Classic maps keep authored skyboxes by default (`r_skyOwner 0`).

## Enable

```
exec modern_raster_ultra.cfg
exec vulkan_overlay_raster_ultra_1_7_atmosphere.cfg
vid_restart
```

Commands: `sky_owner_status`, `weather_status`, `volumetric_clouds_status`

## Signal ownership

| Signal | Owner |
|--------|-------|
| Visible sky radiance | **Exclusive** `r_skyOwner` — classic \| physical \| HDR \| solid |
| Sun direction | `tr.sunDirection` (q3map_sun / map); atmosphere cvars only if unset |
| Global / local fog | Froxel stack (`r_volumetricFog`) + map fog volumes |
| Volumetric cloud coverage | Weather controller + `r_volumetricClouds*` |
| Cloud shadows | Modulate **sun** volumetric / sun visibility only |
| Weather particles | Client `Particles_EmitWeather` (intensity hooks via weather state) |
| Environment irradiance | Existing probes / HDR IBL; weather sunVisibility scales atmosphere |
| Exposure | Post-fog luminance (unchanged); lightning must not poison history |

**Dual sky is forbidden:** classic skybox draw is suppressed when `r_skyOwner != 0`; physical atmosphere paints only when `r_skyOwner == 1`.

## Physical atmosphere

Existing Nishita/Preetham march (`atmosphere.frag`) hardened:

- Sky-owner gate
- Normalized `tr.sunDirection`
- Weather aerosol → Mie scale; sun intensity × weather sunVisibility
- Turbidity packed in push constants

LUT bake (transmittance / multiscatter / sky-view / aerial volume) remains iterative — per-pixel march is the shipping Ultra 1.7 path; LUTs register as future spine resources.

## Aerial perspective / froxels

Reuse froxel volumetrics with:

- `r_fog_shadows 1` under Ultra overlay (sun cascades + local shadows)
- Weather `fogDensityScale`
- Height fog / local box-sphere-capsule / VDB Woodcock unchanged
- Indoor: `r_weatherIndoor 1` suppresses outdoor precip/cloud shadows

## Volumetric clouds

`vk_volumetric_clouds.*`:

- Coverage from weather or override
- Altitude / thickness / wind offset
- **Dedicated history** (reject on camera cut, weather change, sun-dir jump) — not world TAA
- Cloud shadows → `vk_volumetric_clouds_sun_shadow_factor()` into froxel sun intensity

Full ray-marched cloud density (shape/erosion noise) is scaffolded; coverage-driven shadowing and history policy are active.

## Weather controller

Presets: clear, cloudy, overcast, rain, storm, snow, dust, fog.

Smooth blend via `r_weatherTransition`. Outputs coverage, precipitation, fog scale, aerosol, wetness/puddle rates, sun visibility, lightning probability. `r_weatherSunDim` and `r_weatherShadowDim` control how strongly that visibility dims canonical world sun radiance and directional shadow strength.

Dynamic mode (`r_weatherDynamic 1`) advances through deterministic preset transitions after `r_weatherDynamicMinTime`..`r_weatherDynamicMaxTime` seconds, with `r_weatherVolatility` biasing toward rain/storm states. `r_weatherSeed` makes the sequence repeatable. Storm/rain lightning uses `r_weatherLightning` and `r_weatherLightningScale`; the flash factor is folded into canonical sun radiance through the day/night lighting spine.

## Precipitation

Intensity hooks for rain/snow (`vk_weather_precipitation`, wetness rate). Client weather particle emitters remain the draw path; GPU Ultra particles stay FX. Reactivity: stamp via existing reactive mask when precip systems draw.

## Classic compatibility

- Default `r_skyOwner 0` → classic skybox
- Overlay required for physical sky
- Mode 3 deferred / Forward+ transparent / clustered lights / SMAA / adaptive recon overlays unchanged
- Portals: weather outdoor by default; indoor cvar override

## Validation

```
./scripts/raster_ultra_1_7_check.sh
```

Manual: clear noon, sunset, rain preset, indoor suppress, classic map without overlay, `vid_restart`, weather transition soak.

## Promotion decision

| Item | Status |
|------|--------|
| Physical atmosphere without RT | **yes** (owner-gated) |
| Legacy skyboxes compatible | **yes** (default owner) |
| Dual-sky fixed | **yes** |
| Froxels + raster shadows | **yes** (overlay `r_fog_shadows 1`) |
| Weather data-driven | **yes** |
| Cloud shadows on sun | **yes** |
| Full cloud ray march | **scaffolded** |
| LUT bake | **future** |
| Promote to Ultra default | **no** — overlay only |
| Boot unchanged | **yes** |

## Highest-impact fix

**Dual sky:** classic skybox and additive atmosphere could both contribute. Exclusive `r_skyOwner` suppresses classic draw when physical owns sky, and atmosphere returns immediately when it does not own sky.
