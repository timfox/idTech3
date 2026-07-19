# Radiance Cache GI (RcGI)

World-space **spatial-hash radiance cache** plus **cascaded irradiance probes**, screen-space final gather (2-band SH), denoise/upscale, and optional Hybrid1 fusion. Chocolate RTX path (`USE_VULKAN_RTX`).

## Requirements

- Build: `./scripts/compile_engine.sh vulkan rtx`
- GPU: KHR acceleration structure + RT pipeline + **ray query**
- Runtime: `r_fbo 1`, deferred G-buffer (`r_deferredGBufferFill 1` recommended), shared TLAS (`r_rcgi` latch enables RT device features)

## Enable

```
exec demo_rcgi.cfg
vid_restart
```

Or manually:

```
seta r_fbo 1
seta r_rcgi 1
seta r_deferredGBuffer 1
seta r_deferredGBufferFill 1
seta r_rtxEntities 1
vid_restart
```

Console: **`rcgi_status`**.

## Cvars

| Cvar | Default | Role |
|------|---------|------|
| `r_rcgi` | 0 | Master (latched) |
| `r_rcgi_quality` | 1 | Gather resolution: `0` = ¼, `1` = ½ |
| `r_rcgi_rays` | 32 | Visibility rays per probe this frame (1–64) |
| `r_rcgi_cellSize` | 0.25 | Radiance-cache base cell size (world units) |
| `r_rcgi_strength` | 0.85 | Composite / Hybrid1 fusion strength |
| `r_rcgi_hybrid1Fusion` | 1 | Hand irradiance to Hybrid1; skip RcGI scene composite |
| `r_rcgi_debug` | 0 | `0`=blend, `1`/`2`=show irradiance, `3`=boosted |

## Pipeline

1. **Light grid** — cascaded 12³×4 world cells; pack dynamic lights overlapping each cell
2. **Sample** — interleaved cascade of probes; ray-query visibility into TLAS; insert hits into spatial-hash cache
3. **Cache shade** — light-grid lookup + sun term on active cache cells
4. **Volume update** — octahedral probe atlas tiles (temporal blend)
5. **Final gather** — ½ or ¼ res; one cosine ray/pixel; screen cache → world cache → atlas fallback; store SH2 + irradiance
6. **Denoise** — separable bilateral (depth/normal weights)
7. **Upscale** — 4-tap poisson + temporal history → full-res irradiance
8. **Composite** — `albedo * irradiance * strength` **unless** Hybrid1 fusion is active

Runs after geometry via `vk_rcgi_apply_after_geometry` (alongside Surfel GI). When both are on, Surfel skips scene composite; Hybrid1 prefers RcGI irradiance for diffuse fusion.

## Hybrid1 fusion

With **`r_hybrid1 1`** + **`r_rcgi 1`** + **`r_rcgi_hybrid1Fusion 1`** (default):

- RcGI still runs sample/cache/gather/upscale
- RcGI does **not** write scene color
- Hybrid1 keeps shadow + specular RT; skips Hybrid1 diffuse RT while fused
- Hybrid1 composite adds RcGI irradiance × albedo

See [HYBRID_RENDERING1.md](HYBRID_RENDERING1.md).

## Files

- `renderers/vulkan/extensions/rtx/vk_rcgi.c`
- `renderers/vulkan/shaders/glsl/rcgi/*`
- Embedded SPIR-V: `vk_rcgi_spirv.inc` (from `scripts/compile_shaders.sh`)

## Status

v1 chocolate scaffold: light grid, ray-query sample, spatial-hash cache, probe atlas, SH2 gather, denoise/upscale, composite + Hybrid1 fusion. Specular probe re-fit and froxel SH for transparencies remain follow-ups.
