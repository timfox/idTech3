# Neural Visibility Cache (experimental)

**Neural Visibility Cache (NVC)** improves real-time direct lighting for **many dynamic lights** by combining **Forward+** tile lists with a **small neural visibility cache** and a **simplified ReSTIR DI** reservoir pass. Inspired by *Neural Visibility Cache for Real-Time Light Sampling* (Jun/Aug 2025): candidate weights use learned visibility, with extra emphasis at **depth disocclusions** (neon, torches, muzzle flashes, sparks, streetlights, emergency strobes).

This is a **post-geometry additive refine** — it does not replace `r_forwardPlus` PBR shading; it adds neural-weighted direct energy where temporal/screen techniques are weakest.

## When to use

| Scenario | Suggested setup |
|----------|-----------------|
| Many dynamic `dlight`s | `r_forwardPlus 1`, `r_forwardPlusShade 1`, `r_nvc 1` |
| Horror / neon / weapon FX | `r_nvc_disocclusionBoost 1.5`–`2.5` |
| Performance | `r_nvc_scale 0.5` (half-res cache + ReSTIR, full-res composite) |
| Debug cache only | `r_nvc_restirMode 0` |

Requires **`r_fbo 1`**, **`r_forwardPlus 1`**, and packed Forward+ SSBOs (tile cull runs as today).

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_nvc` | `0` | Master toggle (latched; reload map after change) |
| `r_nvc_strength` | `1` | Direct lighting refine scale |
| `r_nvc_scale` | `1` | Cache/ReSTIR resolution (`0.5` ≈ half cost) |
| `r_nvc_disocclusionBoost` | `1.5` | Extra candidate weight at depth edges |
| `r_nvc_reservoirM` | `4` | Reservoir update rounds per pixel (1–16) |
| `r_nvc_restirMode` | `1` | `0` = cache only, `1` = ReSTIR refine + composite |
| `r_nvc_featureDim` | `4` | MLP input features |
| `r_nvc_hiddenDim` | `8` | MLP hidden width |
| `r_nvc_useGBuffer` | `1` | Prefer deferred normals when G-buffer fill is active |
| `r_nvc_skipSky` | `1` | Skip sky depth on composite |
| `r_nvc_debug` | `0` | Developer logging |

## Console

- `nvc_reload` — rebuild weights for current map
- `nvc_status` — active state, Forward+, target resolution

## Content

Optional manifest (`maps/<map>.nvc` or `nvc/<map>.nvc`):

```text
version 1
featureDim 4
hiddenDim 8
weightsPath nvc/mymap.nvcb
```

**Weights** (`.nvcb`): binary `NVC1` header + `W1, b1, W2, b2` (scalar visibility output; same packing as NIST/NIV). v1 ships **procedural default** weights on map load.

## Pipeline

1. Forward+ packs lights + tile cull (before or after opaque depth prepass, per `r_forwardPlusDepthCull`).
2. Opaque world → HDR color + depth (+ optional G-buffer fill).
3. **`nvc_cache.comp`**: depth/normal features → visibility cache RGBA16F (`vis`, `confidence`, `disocclusion`, motion proxy).
4. **`nvc_restir.comp`**: read tile light list + cache → weighted reservoir pick → direct refine RGBA16F.
5. **`nvc_composite.comp`**: additive blend into scene color (full resolution, bilinear upsample if `r_nvc_scale < 1`).

## Limitations (v1)

- Vulkan + FBO + Forward+ only; **no full ReSTIR temporal/spatial reuse** yet.
- **No ray-traced visibility** — neural cache approximates visibility from screen features.
- Post-pass may **overlap** Forward+ energy at disocclusions; tune `r_nvc_strength` / `r_nvc_disocclusionBoost`.
- Future: bind cache into `gen_frag` candidate generation and `USE_VULKAN_RTX` ReSTIR DI when available.

## References

- [FORWARD_PLUS_PIPELINE_AUDIT.md](FORWARD_PLUS_PIPELINE_AUDIT.md)
- [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md) (planned ReSTIR DI/GI)
