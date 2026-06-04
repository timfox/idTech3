# Neural Six-way Lightmaps (experimental)

**Neural Six-way Lightmaps (NSLM)** upgrades classic **six-way lightmaps** (axis-weighted radiance for smoke, fog, and particles) with a compact **3D neural feature volume** and tiny **MLP**. The froxel pass runs inside the existing **volumetric fog** pipeline: after temporal filtering, it **adds** view- and sun-aware scattering into `froxel_volume` before composite — no full volumetric path tracing.

Inspired by *Real-time Neural Six-way Lightmaps* (Apr 2026): believable swamp fog, dust, mist, magic, and muzzle smoke with camera motion and light changes at game-engine cost.

## When to use

| Scenario | Suggested setup |
|----------|-----------------|
| Swamp / outdoor mist | `r_volumetricFog 1`, `r_nslm 1`, tune `r_nslm_strength` |
| Indoor smoke stacks | Same + authored `nslm/<map>.nslm` when you ship trained features |
| With NDGI / NIV | NSLM affects **volumetrics only**; surface GI stays separate |

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_nslm` | `0` | Master toggle (latched; reload map after change) |
| `r_nslm_strength` | `1` | Scattering scale added into froxels |
| `r_nslm_sixWaySharpness` | `2` | Six-way basis exponent from view direction |
| `r_nslm_gridX` / `Y` / `Z` | `32` / `16` / `32` | Feature volume resolution (max 64×32×64) |
| `r_nslm_featureDim` | `4` | Features per voxel (RGBA16F 3D texture) |
| `r_nslm_hiddenDim` | `16` | MLP hidden width |
| `r_nslm_debug` | `0` | Developer logging |

Requires **`r_volumetricFog 1`** and froxel resources (same as standard volumetric fog).

## Console

- `nslm_reload` — rebuild volume/weights for current map
- `nslm_status` — grid size, approximate memory

## Content

Manifest (`maps/<map>.nslm` or `nslm/<map>.nslm`):

```text
version 1
gridX 32
gridY 16
gridZ 32
featureDim 4
hiddenDim 16
worldMin -2048 -2048 -256
worldMax 2048 2048 1024
volumePath nslm/mymap_vol.bin
weightsPath nslm/mymap.nslb
```

Without a manifest, the engine uses **map light-grid bounds** (when available) and a **procedural** feature volume for testing.

**Weights** (`.nslb`): binary `NSL1` header + `W1, b1, W2, b2` floats (same layout as [NEURAL_DYNAMIC_GI.md](NEURAL_DYNAMIC_GI.md) `NDG1`).

**Volume** (future `.bin`): offline-trained RGBA16F voxel features; v1 ships procedural fill only.

## Pipeline

1. Standard volumetric froxel passes (density, lights, clamp, **temporal**).
2. **`nslm_froxel.comp`**: per froxel — sample feature volume, six-way basis from view, MLP decode, **add** RGB into scatter volume.
3. Froxel composite / fullscreen fog as today.

## Memory budget (typical)

| Component | 32×16×32 RGBA16F |
|-----------|------------------|
| Feature volume | ~1.0 MB |
| MLP weights | &lt; 4 KB |

## Limitations (v1)

- Vulkan volumetric path only (`r_volumetricFog 1`).
- No in-tree **trainer**; export volume/weights from your offline bake.
- Does not replace froxel lighting — **modulates** scatter after temporal pass.
- Particle/sprite six-way atlases: future work; froxel hook is the MVP.

## See also

- [NEURAL_IRRADIANCE_VOLUME.md](NEURAL_IRRADIANCE_VOLUME.md) — surface indirect
- [NEURAL_DYNAMIC_GI.md](NEURAL_DYNAMIC_GI.md) — temporal baked lightmaps
- Volumetric fog: `docs/VOLUMETRIC_FOG_ENHANCEMENTS.md`, `vk_volumetric_*.c`
