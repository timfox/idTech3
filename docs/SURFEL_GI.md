# Surfel GI (GIBS)

**GIBS** (Global Illumination Based on Surfels) caches indirect diffuse in a pool of world-space surfels, updates them with **VK_KHR_ray_query** against the shared RTX TLAS, then resolves to a screen irradiance buffer and composites into the HDR scene color.

Inspired by SurfelGI / SIGGRAPH-style surfel GI; revived on the modern chocolate RTX path (`USE_VULKAN_RTX`).

## Requirements

- Build: `./scripts/compile_engine.sh vulkan rtx`
- GPU: KHR acceleration structure + RT pipeline + **ray query**
- Runtime: `r_fbo 1`, deferred G-buffer normals (`r_deferredGBufferFill 1` recommended), and a ready world/entity TLAS (`r_rtx` / `r_hybrid1` / `r_surfelGi` latch enables RT device features)

## Enable

```
exec demo_surfel_gi.cfg
vid_restart
```

Or manually:

```
seta r_fbo 1
seta r_surfelGi 1
seta r_deferredGBuffer 1
seta r_deferredGBufferFill 1
seta r_rtxEntities 1
vid_restart
```

Console: **`surfel_gi_status`**.

## Cvars

| Cvar | Default | Role |
|------|---------|------|
| `r_surfelGi` | 0 | Master (latched) |
| `r_surfelGi_max` | 16384 | Surfel capacity (latched) |
| `r_surfelGi_radius` | 0.35 | Spawn radius |
| `r_surfelGi_updateRate` | 4 | Update stride (frames) |
| `r_surfelGi_samples` | 4 | Hemisphere samples per update |
| `r_surfelGi_intensity` | 1.0 | Irradiance scale in update/resolve |
| `r_surfelGi_strength` | 0.85 | Composite strength vs albedo |
| `r_surfelGi_spawn` | 1024 | Spawn attempts per frame |
| `r_surfelGi_blend` | 0.15 | Temporal blend for irradiance |
| `r_surfelGi_rayDist` | 512 | Max GI ray distance |
| `r_surfelGi_debug` | 0 | 0=blend, 1=show irradiance, 2=confidence mask |
| `r_surfelGi_skipSky` | 1 | Skip sky pixels in resolve/composite |
| `r_surfelGi_maxAge` | 240 | Mark surfel stale after this many updates |
| `r_surfelGi_hash` | 1 | Fixed-bucket spatial hash for resolve (0=strided fallback) |
| `r_surfelGi_cellSize` | 64 | World units per hash cell |

## Pipeline

1. **Spawn** — stratify screen UVs from depth + G-buffer normals → new or recycled surfels
2. **Update** — ray-query hemisphere samples into TLAS; world albedo + geometric normal SSBOs on instance 0 hits; miss ≈ sky; stale recycle
3. **Hash** — clear + scatter active surfels into 4096×8 bucket grid
4. **Resolve** — 3×3×3 neighboring cells → RGBA16F irradiance
5. **Composite** — `albedo * irradiance * strength` added to `vk.color_image`

Runs after geometry (with NIV-class overlays) via `vk_surfel_gi_apply_after_geometry`.

## Files

- `renderers/vulkan/extensions/rtx/vk_surfel_gi.c`
- `renderers/vulkan/shaders/glsl/surfel_gi/*`
- Embedded SPIR-V: `vk_surfel_gi_spirv.inc` (from `scripts/compile_shaders.sh`)

## Status

v1: spawn/update/resolve/composite + world albedo/normal hits + stale recycle + spatial hash resolve. Still open: Hybrid1 channel fusion, denser surfel budgeting, entity (customIndex≠0) attribute SSBOs.
