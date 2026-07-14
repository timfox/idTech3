# Neural Irradiance Volume (experimental)

**Neural Irradiance Volume (NIV)** is a compact, GPU-friendly replacement for dense irradiance probe grids. Inspired by *Real-time Rendering with a Neural Irradiance Volume* (Feb 2026): ~1–5 MB scene data, ~1 ms class decode at 1080p on consumer GPUs, **G-buffer + depth only** (no runtime ray tracing or denoising).

This fork integrates NIV as an **additive indirect pass** after world geometry, augmenting (not replacing) light grids, SH probes, and optional DDGI-style paths.

## When to use

| Scenario | Suggested setup |
|----------|-----------------|
| Indoor/outdoor static GI | `r_niv 1`, ship `niv/<map>.niv` + trained volume |
| With deferred G-buffer | `r_renderMode 1`, `r_deferredGBuffer 1`, `r_deferredGBufferFill 1`, `r_niv_useGBuffer 1` |
| Forward + FBO only | `r_niv 1`, `r_fbo 1` — uses scene color as albedo, depth-only world pos |
| RTGI too expensive | NIV indirect + existing direct lights |

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_niv` | `0` | Master toggle (latched; reload map after change) |
| `r_niv_strength` | `1` | Indirect scale in shade pass |
| `r_niv_scale` | `1` | Decode resolution (`0.5` ≈ half cost, bilinear upsample on composite) |
| `r_niv_gridX` / `Y` / `Z` | `32` / `16` / `32` | Feature volume resolution (max 64×32×64) |
| `r_niv_featureDim` | `4` | Features per voxel (packed in RGBA16F 3D texture) |
| `r_niv_hiddenDim` | `16` | MLP hidden width |
| `r_niv_useGBuffer` | `1` | Prefer deferred normals when G-buffer fill is active |
| `r_niv_normalAtten` | `0.6` | Normal-facing attenuation for indirect GI leak reduction (`0` off, `1` max) |
| `r_niv_ao` | `0.75` | Couple indirect GI to SSAO/HBAO when available (`0` off, `1` full) |
| `r_niv_skipSky` | `1` | Skip sky depth pixels on composite |
| `r_niv_debug` | `0` | Developer logging |

## Console

- `niv_reload` — rebuild volume/weights for current map
- `niv_status` — grid size, approximate memory

## Content

Manifest (`maps/<map>.niv` or `niv/<map>.niv`):

```text
version 1
gridX 40
gridY 24
gridZ 40
featureDim 4
hiddenDim 16
worldMin -2048 -2048 -256
worldMax 2048 2048 1024
volumePath niv/mymap_vol.bin
weightsPath niv/mymap.nivb
```

Without a manifest, the engine uses **map light-grid bounds** (when available) and a **procedural** feature volume for testing.

**Weights** (`.nivb`): binary `NIV1` header + `W1, b1, W2, b2` floats (same layout as [NEURAL_DYNAMIC_GI.md](NEURAL_DYNAMIC_GI.md) `NDG1`).

**Volume** (future `.bin`): offline-trained RGBA16F voxel features; v1 ships procedural fill only.

## Pipeline

1. Opaque world geometry → depth (+ optional G-buffer fill).
2. **`niv_shade.comp`**: per-pixel world position → trilinear 3D feature sample → tiny MLP → irradiance RT.
3. **`niv_composite.comp`**: `scene += irradiance * albedo` (full res), with optional G-buffer normal attenuation and SSAO/HBAO coupling to reduce indirect-light leaks.
4. Direct lights / emissive / volumetrics run as today.

## Memory budget (typical)

| Component | 32×16×32 RGBA16F |
|-----------|------------------|
| Feature volume | ~1.0 MB |
| MLP weights | &lt; 4 KB |
| Irradiance RT (1080p RGBA16F) | ~16 MB transient |

Tune grid down or `r_niv_scale 0.5` for tighter budgets.

## Limitations (v1)

- Vulkan + FBO required (`r_fbo 1`).
- No in-tree **trainer**; export volume/weights from your offline bake.
- Does not remove `r_shLighting` / light grid — **adds** indirect on surfaces.
- Coexists with [NEURAL_DYNAMIC_GI.md](NEURAL_DYNAMIC_GI.md) (temporal **lightmaps**) for different use cases.

## Related

- [RENDERERS.md](RENDERERS.md) — deferred G-buffer cvars
- [FORWARD_PLUS_PIPELINE_AUDIT.md](FORWARD_PLUS_PIPELINE_AUDIT.md)
- [PRODUCTION_GAP_PLAN.md](PRODUCTION_GAP_PLAN.md)
