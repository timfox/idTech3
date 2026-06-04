# Forget Superresolution, Sample Adaptively (FSA, experimental)

**FSA** implements an engine-side version of *Forget Superresolution, Sample Adaptively (when Path Tracing)* (Feb 2026): below **1 sample per pixel**, place path samples where they matter and **denoise** the result instead of relying on uniform low-resolution rendering plus super-resolution.

## When to use

| Scenario | Suggested setup |
|----------|-----------------|
| Sub-1-SPP RTX demo | `r_fbo 1`, `r_rtx 1`, `r_rtxDemo 1`, `r_fsa 1`, `r_fsa_budget 0.25` |
| Many dynamic lights (muzzle flashes) | `r_forwardPlus 1` + `r_fsa_dynamicLightWeight 1` |
| Glossy / contact / silhouettes | `r_deferredGBufferFill 1`, `r_fsa_useGBuffer 1` |
| Lower cost importance | `r_fsa_scale 0.5` |

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_fsa` | `0` | Master toggle (latched; reload map after change) |
| `r_fsa_budget` | `0.25` | Target samples per pixel (&lt;1 allowed); scales stochastic RTX probability |
| `r_fsa_strength` | `1` | Importance feature scale + denoise blend |
| `r_fsa_scale` | `1` | Importance map resolution (0.25–1) |
| `r_fsa_specularWeight` | `1` | Low-roughness / glossy regions |
| `r_fsa_silhouetteWeight` | `1` | Depth-edge silhouettes |
| `r_fsa_contactWeight` | `1` | Normal + depth contact hints |
| `r_fsa_dynamicLightWeight` | `1` | Forward+ tile luminance hotspots |
| `r_fsa_useGBuffer` | `1` | Deferred normals/material when available |
| `r_fsa_rtxAdaptive` | `1` | Stochastic RTX weighted by importance (`rtx_demo.rgen`) |
| `r_fsa_denoise` | `1` | Guided denoise after RTX blit |
| `r_fsa_skipSky` | `1` | Skip sky pixels on denoise |
| `r_fsa_debug` | `0` | Developer logging |

Pair with `r_rtxSamples` (max cap per pixel when adaptive is off or as upper bound in uniform mode).

## Console

- `fsa_reload` — re-init FSA resources for current map
- `fsa_status` — budget, adaptive RTX, target resolution

## Pipeline

1. Opaque world → depth (+ optional G-buffer).
2. **`fsa_importance.comp`**: glossy, silhouette, contact, HDR flash, Forward+ hotspots → RGBA16F importance.
3. **`rtx_demo.rgen`** (when `r_fsa_rtxAdaptive 1`): stochastic trace where `hash < importance × budget × cap`; else legacy uniform multi-sample loop.
4. RTX blit → HDR color.
5. **`fsa_denoise.comp`**: edge-aware fill on low-importance pixels (preserve specular/silhouette traces).

## Limitations (v1)

- Requires **Vulkan + FBO**; adaptive RTX requires **`USE_VULKAN_RTX`** build and `r_rtxDemo 1`.
- **Not full path-traced GI** — uses existing RTX demo primary rays + screen-feature importance.
- No learned denoiser weights yet (hand-tuned bilateral-style pass).
- Does **not** replace TAA/upscale; intentionally avoids “render quarter-res + super-res”.

## References

- [NEURAL_VISIBILITY_CACHE.md](NEURAL_VISIBILITY_CACHE.md) (Forward+ many-light refine)
- [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md) (ReSTIR / RTGI roadmap)
