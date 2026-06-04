# RenderFormer neural mesh preview (experimental)

**RenderFormer** (*Transformer-based Neural Rendering of Triangle Meshes with Global Illumination*, SIGGRAPH 2025) renders triangle scenes with learned global illumination **without per-scene training**. This engine path is a **Vulkan scaffold** for future editor previews, offline lookdev, and AI-assisted lighting — not a production replacement for Forward+ / RTX.

v1 implements:

1. **Triangle tokens** from BSP world meshes (center, normal, albedo, roughness, metallic, area).
2. **View-independent transport** (`rf_transport.comp`) — attention-like aggregation per grid cell into a latent buffer.
3. **View-dependent decode** (`rf_decode.comp`) — depth → world position → latent lookup + view term → RGBA16F preview.
4. **Composite** (`rf_composite.comp`) — additive blend into HDR scene color.

Weights are **procedural** (not `microsoft/renderformer` checkpoints). An external Python pipeline can be added later (same pattern as FLUX / TRELLIS).

## When to use

| Scenario | Suggested setup |
|----------|-----------------|
| Lookdev / lighting preview | `r_renderformer 1`, `r_fbo 1`, reload map |
| Performance | `r_renderformer_scale 0.5`, coarser `r_renderformer_gridX/Y/Z` |
| Better normals | `r_deferredGBuffer 1`, `r_deferredGBufferFill 1`, `r_renderformer_useGBuffer 1` |
| Memory cap | Lower `r_renderformer_triCap` |

Requires **`r_fbo 1`** and a loaded world (`tr.world`). Runs **after opaque geometry** (with VFGI / NVC), before FSA / WSP.

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_renderformer` | `0` | Master toggle (latched; reload map after change) |
| `r_renderformer_strength` | `1` | Transport/decode/composite scale |
| `r_renderformer_scale` | `1` | Decode resolution (`0.25`–`1`) |
| `r_renderformer_triCap` | `32768` | Max triangle tokens (latched) |
| `r_renderformer_quant` | `16` | Center quantize for dedup (reserved) |
| `r_renderformer_gridX/Y/Z` | `32` | Spatial index / latent grid |
| `r_renderformer_hiddenDim` | `4` | Latent width hint (shader scaffold) |
| `r_renderformer_useGBuffer` | `1` | Prefer deferred normals when available |
| `r_renderformer_skipSky` | `1` | Skip sky depth on composite |
| `r_renderformer_debug` | `0` | Per-frame developer logging |

## Console

- `renderformer_reload` — rebuild triangle tokens + GPU buffers for current map
- `renderformer_status` — active state, triangle/grid memory estimate

## Content

Optional manifest (`maps/<map>.rfm` or `renderformer/<map>.rfm`):

```text
version 1
hiddenDim 4
gridX 32
gridY 32
gridZ 32
quantUnits 16
```

## Pipeline

1. **Map load**: walk `world_t` bmodel surfaces (`SF_FACE`, `SF_TRIANGLES`) → deduped triangle tokens → 3D grid index → upload SSBOs.
2. Opaque world → HDR + depth (+ optional G-buffer).
3. **`rf_transport.comp`**: per grid cell, aggregate up to four triangle tokens → latent SSBO.
4. **`rf_decode.comp`**: screen-space decode from latent + depth/view.
5. **`rf_composite.comp`**: depth-aware additive GI-style blend into scene color.

## Future work

- Import [microsoft/renderformer](https://huggingface.co/microsoft/renderformer) via out-of-process inference (`cl_renderformer_*`).
- Dynamic meshes (MD3/MDM), skinned entities, USD hooks.
- Temporal accumulation, denoise with FSA, hybrid RTX validation path.

## References

- [RenderFormer project](https://microsoft.github.io/renderformer/) (SIGGRAPH 2025)
- [VERTEX_FEATURES_NEURAL_GI.md](VERTEX_FEATURES_NEURAL_GI.md) — complementary vertex-feature GI
- [RENDERERS.md](RENDERERS.md)
