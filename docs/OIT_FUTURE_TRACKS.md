# OIT Future Tracks

This document lists transparency work **outside** the WBOIT production milestone (Spine 1.1 / mode 3 shipping path). None of these are required for current certification.

## Production (shipped in this milestone)

| Track | Status |
|-------|--------|
| WBOIT (`r_oit 1`) | **Production** — weighted blended accum + revealage resolve |
| Mode 3 + Forward+ lit accum | **Production** — shared cluster lists with deferred |
| Lifecycle / corruption hardening | **Production** — frame state, resolve layout, weapon exclusion |
| Static + device B0–B7 matrix | **Production gate** — see [MOMENT_OIT_STOCHASTIC_ALPHA.md](MOMENT_OIT_STOCHASTIC_ALPHA.md) |

## Experimental (in tree, not Spine 1.1 certified)

| Track | Notes |
|-------|-------|
| MBOIT / Moment Transparency (`r_oit 2`) | `modern_vulkan_experimental.cfg`, `vulkan_overlay_mboit.cfg`; promotion blocked until WBOIT soak + parity complete |
| Stochastic alpha (`r_stochasticAlpha` 1–2) | Foliage / hair cards; separate from OIT buckets |
| OIT + TAA without weapon-after | Soft-demote; not cert combo |

## Research (not in this milestone)

| Track | Description |
|-------|-------------|
| Material-class OIT | Per-class accum targets / resolve policies tied to `r_materialClassify` |
| Stochastic → OMM | Order-independent meshes from stochastic alpha history |
| Fog through transparent layers | Volumetric fog composited with multi-layer transmittance |
| Refraction-aware OIT | Distortion + OIT weight coupling beyond current ping-pong |
| Colored transmittance | Wavelength-dependent absorption in accum/resolve |
| MBOIT promotion | Move `r_oit 2` to production after experimental matrix + BRDF parity |
| OIT temporal reconstruction | History of resolved transparent color (explicitly **not** raw accum/reveal) |

## Related

- Production hub: [MOMENT_OIT_STOCHASTIC_ALPHA.md](MOMENT_OIT_STOCHASTIC_ALPHA.md)
- Mode 3 handoff: [UNIFIED_CLUSTERED_RENDERER.md](UNIFIED_CLUSTERED_RENDERER.md)
- Spine 1.1 cert: [RENDERER_SPINE_1.1.md](RENDERER_SPINE_1.1.md)
