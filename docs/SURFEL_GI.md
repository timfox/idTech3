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

## Pipeline

1. **Spawn** — stratify screen UVs from depth + G-buffer normals → new surfels
2. **Update** — ray-query hemisphere samples into TLAS; miss ≈ sky ambient
3. **Resolve** — weighted gather of nearby surfels → RGBA16F irradiance
4. **Composite** — `albedo * irradiance * strength` added to `vk.color_image`

Runs after geometry (with NIV-class overlays) via `vk_surfel_gi_apply_after_geometry`.

## Files

- `renderers/vulkan/extensions/rtx/vk_surfel_gi.c`
- `renderers/vulkan/shaders/glsl/surfel_gi/*`
- Embedded SPIR-V: `vk_surfel_gi_spirv.inc` (from `scripts/compile_shaders.sh`)

## Status

v1: working spawn/update/resolve/composite scaffold. Still open: spatial hash for resolve, material/albedo hit shading, surfel recycling/culling, Hybrid1 channel fusion.
