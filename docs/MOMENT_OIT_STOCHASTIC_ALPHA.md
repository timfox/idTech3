# Moment-Based OIT & Stochastic Alpha

## Moment Transparency / MBOIT (`r_oit 2`)

Order-independent transparency for **glass, smoke, particles, translucent surfaces, and overlapping transparent layers**.

| Mode | Technique |
|------|-----------|
| `r_oit 0` | Off (sorted alpha blend) |
| `r_oit 1` | WBOIT (weighted blended) |
| `r_oit 2` | **MBOIT / Moment Transparency** (Sharpe HPG 2018 / Münstermann I3D 2018 style) |

Requires `r_fbo 1` and `vid_restart`.

### Algorithm (mode 2)

1. Opaque geometry (depth write)
2. **Moments pass** — additive optical depth `d = -log(1-α)` and power moments `d·(z,z²,z³,z⁴)`
3. **Accum pass** — reconstruct transmittance `T(z)` from moments (Cantelli/MSM-style + β=0.25 overestimation), WBOIT-style weighted color + revealage
4. **Resolve** — composite onto opaque background

```
seta r_fbo 1
seta r_oit 2
vid_restart
```

Demo: `exec demo_mboit.cfg` (or `vulkan_overlay_mboit.cfg`).

### With Unified Clustered (`r_renderMode 3`)

Pair OIT with deferred opaque. Both WBOIT and MBOIT accum use Forward+ tile lights via `r_oitForwardPlus 1` (default; MBOIT moments pass stays unlit).

`r_oitClassify 1` splits transparent draws into alpha-blend (MBOIT/WBOIT) vs additive particles/smoke (WBOIT, no moments), compositing additive last. Default `0` keeps a single global bucket. Hair cards stay on `r_stochasticAlpha`, not OIT.

```
exec vulkan_overlay_oit_clustered.cfg
vid_restart
```

Demo: `exec demo_oit_clustered.cfg`. See [UNIFIED_CLUSTERED_RENDERER.md](UNIFIED_CLUSTERED_RENDERER.md). Set `r_oitForwardPlus 0` to restore unlit MBOIT/WBOIT accum.

### Temporal Reconstruction

When `r_taa` / `r_aaMode` 4–5 is active, OIT revealage stamps a dedicated R8 **reactive mask** (not `oit_reveal` itself) so Temporal Reconstruction prefers the current frame on glass/smoke (`r_temporalReactiveMask 1`). Forward+ transparent and stochastic survivors also stamp via `gen_frag`. Raw OIT accum/reveal are never temporally blended — resolve into HDR first via `texelFetch` + NEAREST with an explicit accum→resolve barrier (prevents horizontal scanline tears from BY_REGION races). First-person weapons are deferred past world TAA (`r_temporalWeaponAfterTaa 1`). See [HDR_GAPS.md](HDR_GAPS.md) §6.8.

### Resolve equation (WBOIT / MBOIT accum)

McGuire/Bavoil composite (linear HDR):

```
C_avg = accum.rgb / max(accum.a, eps)
C_out = C_avg * (1 - revealage) + C_bg * revealage
```

Clears: accum `vec4(0)`, revealage `1`. Depth test uses reversed-Z `GREATER_OR_EQUAL` (no depth write). Debug: `r_oitDebug` 1–11; NaN/Inf → magenta.

## Stochastic Alpha-Clipped Materials (`r_stochasticAlpha`)

Hashed / temporal alpha clip for **foliage, grates, hair cards, fabric holes, and decals** (shader `alphaFunc`).

| Mode | Behavior |
|------|----------|
| `0` | Hard `alphaFunc` discard (default) |
| `1` | Screen-space interleaved gradient noise hash |
| `2` | Temporal hash (frame-seeded; requires Temporal Reconstruction — auto-falls back to mode 1 when TAA is off) |

```
seta r_stochasticAlpha 2
seta r_taa 1
```

No `vid_restart` required (push-constant driven). Applies to `GT0` / `LT128` / `GE128`. If mode 2 is set while `r_taa` is off, the backend pushes mode 1 so coverage is not frozen without history.

## Related

- Existing WBOIT: [RENDERERS.md](RENDERERS.md)
- Unified Clustered transparent path: [UNIFIED_CLUSTERED_RENDERER.md](UNIFIED_CLUSTERED_RENDERER.md)
