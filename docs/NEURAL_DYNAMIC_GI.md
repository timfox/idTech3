# Neural Dynamic GI (experimental)

**Neural Dynamic GI** compresses many **temporal baked lightmap** states (day/night, weather, flickering fixtures, seasonal variants) into a small **neural feature atlas** plus optional **BC-style codebook blocks**. At runtime the Vulkan renderer **decompresses** only the virtual-texture **pages** needed and patches the merged BSP lightmap atlases—no separate lightmap stack per time slice.

Inspired by *Random-Access Neural Compression for Temporal Lightmaps* (Apr/May 2026); this fork ships a practical engine integration, not a training toolchain.

## Use cases

| Scenario | `r_ndgi_time` / `timeKeys` |
|----------|---------------------------|
| Day / night cycle | `r_ndgi_cycle 1` or keys `0 0.5 1` |
| Emergency strobe | Fast cycle or scripted `r_ndgi_time` |
| Weather (overcast ↔ sun) | Slow blend between states |
| Seasonal baked GI | Discrete `timeKeys` per season |

Static world geometry keeps BSP lightmap UVs; only irradiance in the atlas changes.

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_ndgi` | `0` | Master toggle (latched; requires map reload) |
| `r_ndgi_time` | `0` | Normalized blend time in `[0,1]` |
| `r_ndgi_cycle` | `0` | Auto-advance time over `r_ndgi_cyclePeriod` |
| `r_ndgi_cyclePeriod` | `120` | Cycle length in seconds |
| `r_ndgi_blend` | `1` | Scales temporal blend strength |
| `r_ndgi_vt` | `1` | Virtual texture: decode dirty pages only |
| `r_ndgi_bc` | `0` | Prefer BC-style codebook feature blocks |
| `r_ndgi_compute` | `0` | GPU decompress (reserved; CPU path today) |
| `r_ndgi_states` | `4` | Override state count when no manifest |
| `r_ndgi_debug` | `0` | Developer logging |

## Console

- `ndgi_reload` — re-read manifest/features for current map
- `ndgi_status` — print active manifest / VT layout

## Content layout

Manifest (first match wins):

1. `maps/<map>.ndgi`
2. `ndgi/<map>.ndgi`

Example:

```text
version 1
states 4
featureWidth 64
featureHeight 64
featureDim 4
hiddenDim 16
vtPagesX 2
vtPagesY 2
featurePath textures/ndgi/mymap_features.tga
weightsPath ndgi/mymap.ndgib
timeKeys 0 0.33 0.66 1
bc 0
```

**Feature atlas** (`featurePath`): RGBA TGA; states are stacked vertically (slice per state).

**Weights** (`weightsPath`): binary `NDG1` header + packed `W1, b1, W2, b2` floats (see `vk_ndgi.c`).

**BC-style** (`bc 1`): optional `.ndgbc` codebook blocks (8×uint8 palette + 16×3-bit indices per 4×4 tile); document format in manifest comments.

If no manifest exists, the engine generates **procedural** features for demos.

## Runtime path

1. `RE_LoadWorldMap` → `R_NDGI_OnMapLoad( baseName )` after lightmaps.
2. Each frame `RE_RenderScene` → `R_NDGI_FrameUpdate()` when active.
3. CPU: sample features → tiny MLP → `vk_upload_image_data` into merged lightmap cells.
4. Existing `sampleLightmap()` in world shaders unchanged.

## SP slice / demo

```cfg
set r_ndgi 1
set r_ndgi_cycle 1
set r_ndgi_cyclePeriod 90
set r_ndgi_vt 1
```

Ship `ndgi/<map>.ndgi` in your mod `.pk3` when you have a BSP with lightmaps (`r_vertexLight 0`).

## Limitations (v1)

- **Vulkan only**; requires loaded world lightmaps.
- **No offline trainer** in-tree—export features/weights from your bake pipeline.
- `r_ndgi_compute 1` compiles `ndgi_decompress.comp` but falls back to CPU until storage images are wired to lightmap images.
- Dynamic objects still use vertex lighting / probes; NDGI targets **static surfaces**.

## Related docs

- [RENDERERS.md](RENDERERS.md) — Vulkan post/TAA
- [PRODUCTION_GAP_PLAN.md](PRODUCTION_GAP_PLAN.md) — SP slice checklist
- [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md) — RTX / DDGI (separate from NDGI)
