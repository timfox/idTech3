# Unified Clustered Renderer

**Unified Clustered Renderer** — hybrid deferred and Forward+ shading (`r_renderMode 3`).

Also called: Hybrid Clustered Deferred Renderer / Deferred + Forward+ Pipeline.

## What it is

| Pass | Role |
|------|------|
| G-buffer | Opaque albedo / normal / material |
| Forward+ tile cull | Shared 16×16 light lists (same SSBOs as mode 1/2) |
| Deferred lighting | Opaque conventional geometry (compute + composite) |
| Forward+ fragment shade | Transparent / blend surfaces after deferred |

Avoid calling this simply “deferred” (hides Forward+) or “deferred forward” (sounds contradictory).

## Enable (opt-in)

Shipping default remains **`modern_vulkan.cfg`** (`r_renderMode 2`).

```
exec vulkan_overlay_unified_clustered.cfg
vid_restart
```

Or demo: `exec demo_unified_clustered.cfg`.

Console: `renderer_status` prints a `unified` row when mode 3 is active.

## Frame order

1. Forward+ light pack + tile cull (optional depth cull after opaque prepass)
2. Opaque draw (`drawSurfFilter` 1) with hybrid handoff (no Forward+ add; unlit primary)
3. G-buffer capture + visibility fill (optional) + deferred lighting compute + composite
4. Transparent draw (`drawSurfFilter` 2) with Forward+ shade
5. Neural / bloom / TAA as usual

Depth is **not** cleared between deferred composite and transparent draws.

Deferred lighting transforms direct-export **world** normals to view space, and can optionally consume the material class map (`r_deferredMaterialClassify` default 0 + `r_materialClassify`) — see [RENDERER_2027.md](RENDERER_2027.md).

## OIT + mode 3

OIT (`r_oit` 1=WBOIT / 2=MBOIT) runs **after** deferred composite on the mode 3 spine (see backend: OIT after deferred when `r_fbo` 1). Transparent Forward+ draws still use the shared tile lists; moments/accum then resolve over the deferred opaque base.

```
exec vulkan_overlay_oit_clustered.cfg
vid_restart
```

Or demo: `exec demo_oit_clustered.cfg` (adds `r_stochasticAlpha 2` + TAA). Residual risk remains for exotic MSAA / portal paths — keep `r_ext_multisample 0`.

## Related

- Moment OIT / stochastic alpha: [MOMENT_OIT_STOCHASTIC_ALPHA.md](MOMENT_OIT_STOCHASTIC_ALPHA.md)
- 2027 north-star (visibility buffer on this spine): [RENDERER_2027.md](RENDERER_2027.md)
- Forward+ audit: [FORWARD_PLUS_PIPELINE_AUDIT.md](FORWARD_PLUS_PIPELINE_AUDIT.md)
- Modes overview: [RENDERERS.md](RENDERERS.md)
- Mode 1 deferred-only overlay: `vulkan_overlay_deferred.cfg`
- Hybrid1 (RTX): [HYBRID_RENDERING1.md](HYBRID_RENDERING1.md) — separate from this lighting mode
