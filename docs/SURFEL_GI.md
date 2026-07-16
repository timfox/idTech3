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
| `r_surfelGi_hybrid1Fusion` | 1 | When Hybrid1 is active: skip Surfel scene composite; Hybrid1 adds `albedo * irradiance * strength` (avoids double diffuse GI). Set `0` for legacy double-add / Surfel-only composite testing. |
| `r_surfelGiDensity` | 0 | Density preset: 0=custom, 1=sparse, 2=balanced, 3=dense (live; sets spawn/radius/cell/minSep) |
| `r_surfelGi_adaptiveSpawn` | 1 | Scale spawn attempts by pool fill (boost when sparse, ease when near capacity) |
| `r_surfelGi_minSep` | 0.22 | Reject new spawns within this world distance of an existing surfel (`0`=off) |

## Pipeline

1. **Spawn** — stratify screen UVs from depth + G-buffer normals → new or recycled surfels; optional **min-separation** + **adaptive attempt count**
2. **Update** — ray-query hemisphere samples into TLAS; world albedo + geometric normal SSBOs on instance 0 hits; miss ≈ sky; stale recycle
3. **Hash** — clear + scatter active surfels into 4096×**16** bucket grid
4. **Resolve** — 3×3×3 neighboring cells → RGBA16F irradiance
5. **Composite** — `albedo * irradiance * strength` added to `vk.color_image` **unless** Hybrid1 fusion is active (then irradiance is handed to Hybrid1 composite)

Runs after geometry (with NIV-class overlays) via `vk_surfel_gi_apply_after_geometry`.

## Density budgeting

| Preset | spawn | radius | cell | minSep |
|--------|-------|--------|------|--------|
| 1 sparse | 512 | 0.55 | 96 | 0.45 |
| 2 balanced | 1024 | 0.35 | 64 | 0.22 |
| 3 dense | 3072 | 0.22 | 40 | 0.14 |

`seta r_surfelGiDensity 3` for denser coverage without raising latched `r_surfelGi_max`. Capacity itself still needs `vid_restart` when changed.

## Hybrid1 fusion

With **`r_hybrid1 1`** + **`r_surfelGi 1`** + **`r_surfelGi_hybrid1Fusion 1`** (default):

- Surfel still spawn/update/hash/resolve
- Surfel **does not** write scene color
- Hybrid1 keeps shadow + specular RT; skips Hybrid1 diffuse RT while fused
- Hybrid1 composite adds Surfel irradiance × albedo (`hybrid1_status` reports `surfelFusion=1`; debug mode **5** shows irradiance)

See `docs/HYBRID_RENDERING1.md`.

## Files

- `renderers/vulkan/extensions/rtx/vk_surfel_gi.c`
- `renderers/vulkan/shaders/glsl/surfel_gi/*`
- Embedded SPIR-V: `vk_surfel_gi_spirv.inc` (from `scripts/compile_shaders.sh`)

## Status

v1: spawn/update/resolve/composite + world albedo/normal hits + entity (customIndex==1) albedo/normal SSBOs + stale recycle + spatial hash resolve + **Hybrid1 channel fusion** (`r_surfelGi_hybrid1Fusion`) + **density budgeting** (`r_surfelGiDensity`, adaptive spawn, min-sep, 16-slot hash buckets). World/entity pack-time UV thumbs via `r_rtxWorldUvSample` / `r_rtxEntityUvSample`. Still open: UV-sampled textures in hit shaders.
