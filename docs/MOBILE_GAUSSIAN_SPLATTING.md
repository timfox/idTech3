# Mobile-GS — Gaussian splatting for mobile-class GPUs (experimental)

**Mobile-GS** (*Real-time Gaussian Splatting for Mobile Devices*, Mar 2026) motivates **tiered cost**, **reduced-resolution splat buffers**, and **bounded per-splat footprints** so photogrammetry-style Gaussians can run on phones, tablets, and WebGPU-class devices—not only desktop RTX.

This engine path is **Vulkan compute** (no `USE_VULKAN_RTX`). It complements **[GRTX](GAUSSIAN_RAY_TRACING_GRTX.md)** (desktop ray-traced AABB proxies) and future **WebGPU** export of the same splat record layout.

## When to use

| Scenario | Suggested setup |
|----------|-----------------|
| Android / low-end GPU | `r_mgs 1`, `r_fbo 1`, optional `r_renderScale` |
| Vista / background splats | Tier 1–2, raise `r_mgs_strength` slightly |
| Desktop preview | `r_mgs 3` for up to 1024 splats at full accum scale |

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_mgs` | `0` | Master tier (latched): `0`=off, `1`=mobile, `2`=balanced, `3`=high |
| `r_mgs_strength` | `0.85` | Splat alpha / composite weight |
| `r_mgs_maxSplats` | `0` | Override splat count (`0` = tier default 64/256/1024) |
| `r_mgs_scale` | `0` | Override accum resolution scale (`0` = tier 0.25/0.5/1) |
| `r_mgs_focal` | `512` | Projected screen-radius focal scale |
| `r_mgs_skipSky` | `1` | Skip sky depth on composite |
| `r_mgs_depthTest` | `1` | Occlude splats behind scene depth |
| `r_mgs_debug` | `0` | Developer logging |

Requires **`r_fbo 1`** and depth buffer.

### Tier defaults

| `r_mgs` | Max splats | Accum scale | Max footprint (px) |
|---------|------------|-------------|-------------------|
| 1 mobile | 64 | 0.25 | 12 |
| 2 balanced | 256 | 0.5 | 24 |
| 3 high | 1024 | 1.0 | 48 |

## Console

- `mgs_status` — tier, splat count, accum extent

## Pipeline

1. Map load → procedural `mgsGaussian_t` SSBO (same layout as GRTX for future `.ply` / manifest).
2. **`mgs_prepare.comp`**: project Gaussians → screen splat records.
3. **`mgs_splat.comp`**: one dispatch per splat, bounded bbox, Gaussian falloff into RGBA16F accum.
4. **`mgs_composite.comp`**: depth-aware blend into HDR `color_image` (after world geometry, with NIST).

## Android / WebGPU notes

- **Android**: Built with the normal Vulkan renderer; use tier **1** first. Pair with `cl_mobilefog` / `r_volumetricFog 0` on very weak devices.
- **WebGPU**: See **[WEB_SPLATTER.md](WEB_SPLATTER.md)** (`r_wsp`) for the tile-binned Vulkan path aligned with WebGPU compute limits; WGSL port still out-of-tree.

## Content (future)

Manifest `maps/<map>.mgs` / `mgs/<map>.mgs` reserved for asset paths; v1 uses procedural Gaussians near the world light-grid origin.

## Limitations (v1)

- **Brute per-splat footprint** (no tile sorting yet)—cost scales with splat count × footprint².
- **Isotropic screen splats** (no full 3D covariance projection).
- **No alpha sorting** across splats (overdraw approximated via accumulation).
- Vulkan only; no gameplay changes.

## See also

- [GAUSSIAN_RAY_TRACING_GRTX.md](GAUSSIAN_RAY_TRACING_GRTX.md) — desktop RTX proxies
- [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md) — renderer roadmap
- Client mobile fog: `src/client/cl_mobilefog.c`
