# Vertex Features for Neural Global Illumination (experimental)

**Vertex Features Neural GI (VFGI)** stores compact learned features on **unique world mesh vertices** instead of a dense 3D feature grid. A coarse spatial index (default 48³ cells, up to four vertex indices per cell) drives screen-space lookup: depth → world position → nearest vertex features → small MLP decode → additive indirect irradiance composite.

Inspired by *Vertex Features for Neural Global Illumination* (Aug 2025, updated 2026): memory is typically **~⅕ or less** versus grid-based neural GI at similar perceived quality, and the representation maps naturally onto **triangle engines** (BSP world surfaces, static meshes, future USD entities).

## When to use

| Scenario | Suggested setup |
|----------|-----------------|
| Static / BSP-heavy levels | `r_vfgi 1`, `r_fbo 1`, reload map |
| Better normals at decode | `r_deferredGBuffer 1`, `r_deferredGBufferFill 1`, `r_vfgi_useGBuffer 1` |
| Performance | `r_vfgi_scale 0.5` (half-res decode, full-res composite) |
| Memory cap | Lower `r_vfgi_vertCap` or coarser `r_vfgi_gridX/Y/Z` |

Requires **`r_fbo 1`** and a loaded world (`tr.world`). Does not replace lightmaps or Forward+; it adds a **post-geometry indirect refine**.

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_vfgi` | `0` | Master toggle (latched; reload map after change) |
| `r_vfgi_strength` | `1` | Decode irradiance scale |
| `r_vfgi_scale` | `1` | Decode resolution (`0.25`–`1`) |
| `r_vfgi_featureDim` | `4` | MLP input features per vertex |
| `r_vfgi_hiddenDim` | `16` | MLP hidden width |
| `r_vfgi_vertCap` | `524288` | Max unique world vertices (latched) |
| `r_vfgi_quant` | `8` | World-space vertex quantize (units) for dedup |
| `r_vfgi_gridX/Y/Z` | `48` | Spatial index resolution |
| `r_vfgi_useGBuffer` | `1` | Prefer deferred normals when G-buffer fill is active |
| `r_vfgi_skipSky` | `1` | Skip sky depth on composite |
| `r_vfgi_debug` | `0` | Developer logging |

## Console

- `vfgi_reload` — rebuild vertex index + weights for current map
- `vfgi_status` — active state, vertex/grid memory estimate

## Content

Optional manifest (`maps/<map>.vfgi` or `vfgi/<map>.vfgi`):

```text
version 1
featureDim 4
hiddenDim 16
gridX 48
gridY 48
gridZ 48
quantUnits 8
```

**Weights** (`.vfgb`, future): binary header + `W1, b1, W2, b2` for RGB irradiance decode. v1 ships **procedural default** weights and **procedural per-vertex features** extracted from BSP triangles on map load.

## Pipeline

1. **Map load**: walk `world_t` BSP face triangles → quantize/dedup vertices → 4-float features → build 3D grid index → upload SSBOs.
2. Opaque world → HDR color + depth (+ optional G-buffer fill).
3. **`vfgi_decode.comp`**: per-pixel depth → world pos → grid cell → nearest vertex feature → MLP → RGBA16F irradiance.
4. **`vfgi_composite.comp`**: depth-aware additive blend into scene color (bilinear upsample if `r_vfgi_scale < 1`).

## Engine fit

- **BSP / static geometry**: primary v1 path (`vk_vfgi_world.c`).
- **USD / entity meshes**: same vertex+index pattern; hook when entity render meshes are enumerated (not wired in v1).
- **Compared to NIV**: NIV uses a 3D probe grid; VFGI uses **vertices + sparse index** — see `vfgi_status` for KB estimates.

## Limitations (v1)

- Vulkan + FBO only; features are **procedural placeholders**, not trained offline weights.
- World extraction is **BSP surfaces only** (not MD3/MDM models in view).
- No temporal accumulation or probe updates per frame.
- Future: `.vfgb` weight load, bind features in vertex shader, hybrid with RTX/NDGI.

## References

- [NEURAL_IRRADIANCE_VOLUME.md](NEURAL_IRRADIANCE_VOLUME.md)
- [NEURAL_DYNAMIC_GI.md](NEURAL_DYNAMIC_GI.md)
- [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md)
