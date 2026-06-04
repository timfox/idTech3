# Neural Image Space Tessellation (experimental)

**Neural Image Space Tessellation (NIST)** smooths **low-poly silhouettes** in screen space using G-buffer depth and normals, giving a tessellation-like look **without** changing mesh topology or adding GPU geometry cost. Inspired by *Neural Image Space Tessellation* (Feb 2026): cost is **constant per frame** and **decoupled from scene complexity**.

Ideal for a **retro geometry, modern lighting** look: Quake/PS2-style meshes with softened edges at full framerate.

## When to use

| Scenario | Suggested setup |
|----------|-----------------|
| Low-poly world + HDR | `r_fbo 1`, `r_nist 1`, optional deferred G-buffer |
| Sharpest silhouettes | `r_renderMode 1`, `r_deferredGBuffer 1`, `r_deferredGBufferFill 1`, `r_nist_useGBuffer 1` |
| Performance | `r_nist_scale 0.5` (half-res refine, full-res composite) |

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_nist` | `0` | Master toggle (latched; reload map after change) |
| `r_nist_strength` | `1` | Silhouette blend strength in refine pass |
| `r_nist_scale` | `1` | Refine resolution (`0.5` ≈ half cost) |
| `r_nist_edgeThreshold` | `0.002` | Depth-edge scale for silhouette detection |
| `r_nist_radius` | `4` | Inward color gather steps (1–8) |
| `r_nist_depthTolerance` | `0.001` | Depth match for interior samples |
| `r_nist_featureDim` | `4` | MLP input features |
| `r_nist_hiddenDim` | `8` | MLP hidden width |
| `r_nist_useGBuffer` | `1` | Use deferred normals when available |
| `r_nist_skipSky` | `1` | Skip sky depth on composite |
| `r_nist_debug` | `0` | Developer logging |

Requires **`r_fbo 1`** and depth buffer.

## Console

- `nist_reload` — rebuild weights for current map
- `nist_status` — MLP size and target resolution

## Content

Optional manifest (`maps/<map>.nist` or `nist/<map>.nist`):

```text
version 1
featureDim 4
hiddenDim 8
weightsPath nist/mymap.nistb
```

**Weights** (`.nistb`): binary `NIS1` header + `W1, b1, W2, b2` (scalar blend output; layout similar to NDGI `NDG1` but single output neuron). v1 ships **procedural default** weights on map load.

## Pipeline

1. Opaque world → HDR color + depth (+ optional G-buffer fill).
2. **`nist_refine.comp`**: edge/silhouette features → tiny MLP → refined color + blend weight (RGBA16F RT, optional half-res).
3. **`nist_composite.comp`**: blend refined into scene color at full resolution.
4. Bloom / fog / TAA as today.

## Limitations (v1)

- Vulkan + FBO only.
- No in-tree **trainer**; export `.nistb` from offline bake.
- Does not replace MSAA or mesh LOD — **screen-space** edge softening only.
- Best with `r_deferredGBufferFill 1`; falls back to depth-derived normals.

## See also

- [NEURAL_IRRADIANCE_VOLUME.md](NEURAL_IRRADIANCE_VOLUME.md) — surface indirect
- [NEURAL_SIXWAY_LIGHTMAPS.md](NEURAL_SIXWAY_LIGHTMAPS.md) — volumetric silhouettes
- Deferred G-buffer: `vk_deferred_gbuffer.c`
