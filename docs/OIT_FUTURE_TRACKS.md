# OIT Future Tracks

This document lists transparency work **outside** the WBOIT production milestone (Spine 1.1 / mode 3 shipping path). None of these are required for current certification.

## Production (shipped in this milestone)

| Track | Status |
|-------|--------|
| WBOIT (`r_oit 1`) | **Production** — weighted blended accum + revealage resolve |
| Mode 3 + Forward+ lit accum | **Production** — shared cluster lists with deferred |
| Lifecycle / corruption hardening | **Production** — frame state, resolve layout, weapon exclusion |
| Static + device B0–B7 matrix | **Production gate** — see [MOMENT_OIT_STOCHASTIC_ALPHA.md](MOMENT_OIT_STOCHASTIC_ALPHA.md) |
| Fog through transparent layers | **In progress (mode 1 shipping)** — [WBOIT_FOG_LAYERS.md](WBOIT_FOG_LAYERS.md); `r_oitFogMode 1` default |

## Experimental (in tree, not Spine 1.1 certified)

| Track | Notes |
|-------|-------|
| MBOIT / Moment Transparency (`r_oit 2`) | `modern_vulkan_experimental.cfg`, `vulkan_overlay_mboit.cfg`; promotion blocked until WBOIT soak + parity complete |
| WBOIT fog mode 2 (moments) | Weighted fog moments — research-ish; accum falls back toward mode 1 until validated ([WBOIT_FOG_LAYERS.md](WBOIT_FOG_LAYERS.md)) |
| Stochastic alpha (`r_stochasticAlpha` 1–2) | Foliage / hair cards; separate from OIT buckets |
| OIT + TAA without weapon-after | Soft-demote; not cert combo |

## Research (not in this milestone)

| Track | Description |
|-------|-------------|
| Material-class OIT | Per-class accum targets / resolve policies tied to `r_materialClassify` |
| Stochastic → OMM | Order-independent meshes from stochastic alpha history |
| Fog mode 3 enhanced approximation | Experimental `r_oitFogMode 3`; not Spine 1.1 certified |
| Refraction-aware OIT | Distortion + OIT weight coupling beyond current ping-pong |
| Colored transmittance | Wavelength-dependent absorption in accum/resolve |
| MBOIT promotion | Move `r_oit 2` to production after experimental matrix + BRDF parity |
| OIT temporal reconstruction | History of resolved transparent color (explicitly **not** raw accum/reveal) |

## Related

- Production hub: [MOMENT_OIT_STOCHASTIC_ALPHA.md](MOMENT_OIT_STOCHASTIC_ALPHA.md)
- Mode 3 handoff: [UNIFIED_CLUSTERED_RENDERER.md](UNIFIED_CLUSTERED_RENDERER.md)
- Spine 1.1 cert: [RENDERER_SPINE_1.1.md](RENDERER_SPINE_1.1.md)
