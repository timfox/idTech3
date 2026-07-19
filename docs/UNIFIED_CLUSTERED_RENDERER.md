# Unified Clustered Renderer

**Unified Clustered Renderer** — hybrid deferred and Forward+ shading (`r_renderMode 3`).

Also called: Hybrid Clustered Deferred Renderer / Deferred + Forward+ Pipeline / **tiled hybrid** (2D 16×16 Forward+ tiles — **not** frustum Z-clusters).

## What it is

| Pass | Role |
|------|------|
| G-buffer | Opaque albedo / normal / material |
| Forward+ tile cull | Shared **2D** 16×16 light lists (same SSBOs as mode 1/2; no Z slices) |
| Deferred lighting | Opaque conventional geometry (compute + composite) |
| Forward+ fragment shade | Transparent / blend surfaces after deferred |

**Product lock:** this is a **tiled hybrid**, not true clustered shading with depth slices. Z-clusters remain a 2027 follow-up.

Avoid calling this simply “deferred” (hides Forward+) or “deferred forward” (sounds contradictory).

## Enable (opt-in)

Shipping default is **`modern_vulkan.cfg`** (`r_renderMode 3` Unified Clustered). Equivalent profile: `exec modern_clustered.cfg`.

```
exec vulkan_overlay_unified_clustered.cfg
vid_restart
```

Or demo: `exec demo_unified_clustered.cfg`.

Console: `renderer_status` prints a `unified` row when mode 3 is active.

For a safer debugging baseline that disables TAA/SMAA/FXAA/OIT and keeps MSAA off:

```cfg
renderer_clustered_safe
vid_restart
```

Or use the safe overlay directly: `exec vulkan_overlay_unified_clustered_safe.cfg`.

## Frame order

1. Forward+ light pack + tile cull (optional depth cull after opaque prepass)
2. Opaque draw (`drawSurfFilter` 1) with hybrid handoff (no Forward+ add; keep lightmap/vertex primary as static-lit base)
3. G-buffer capture + visibility fill (optional) + deferred lighting compute + composite
4. Transparent draw (`drawSurfFilter` 2) with Forward+ shade
5. Neural / bloom / TAA as usual

Depth is **not** cleared between deferred composite and transparent draws.

Deferred lighting transforms direct-export **world** normals to view space, and can optionally consume the material class map (`r_deferredMaterialClassify` default 0 + `r_materialClassify`) — see [RENDERER_2027.md](RENDERER_2027.md).

## Weapon / HUD pass contract

- **`RDF_NOWORLDMODEL`** views (first-person weapon, banners): opaque + transparent **Forward+ only** — no G-buffer capture, visibility fill, or deferred lighting. Opaque hybrid handoff is disabled so weapons keep normal Forward+ shade.
- **HUD / StretchPic**: after world deferred/visbuf ends the main pass, `vk_prepare_2d` heals a missing render pass when `doneWorldScene && !inRenderPass` by beginning the UI overlay (or `post_bloom` fallback). Without that, 2D draws are dropped.
- **TAA**: shipping `modern_vulkan.cfg` enables `r_taa 1` + `r_taaMotionVectors 1`. HUD stays in the UI overlay (outside TAA history). Weapon views must not force a world-history reset every frame (`r_temporalCpuSkinPrev 1`; first-person projection sticky compare). Bisect with `renderer_clustered_safe` (TAA off).

## OIT + mode 3

When `r_oit` 1/2 is on with mode 3, the backend runs **`vk_oit_pass` instead of** the Forward+ transparent shade pass (`drawSurfFilter=2`). Moments/accum/resolve still composite over the deferred opaque base.

**WBOIT (`r_oit 1`) + `r_oitForwardPlus 1` (default):** accumulation samples Forward+ tile lights (set 2) using world-space position from the object→world push matrix. **MBOIT (`r_oit 2`) + `r_oitForwardPlus 1`:** moments pass stays unlit; accum samples the same tile lists on set 4.

```
exec vulkan_overlay_oit_clustered.cfg
vid_restart
```

Or demo: `exec demo_oit_clustered.cfg` (adds `r_stochasticAlpha 2` + TAA). Keep `r_ext_multisample 0`.

## Late-shade (optional exclusive)

`r_visibilityBufferLateShade 1` (latched) + `r_visibilityBufferFill 2` + non-MSAA: opaque lighting runs the **late-shade** consumer only (G-buffer MRTs + Forward+ tiles). Classic `vk_deferred_lighting_apply_after_geometry` is skipped — no dual lighting path. Debug mode 5 previews class×albedo. Neural/Hybrid1 still read the G-buffer until migrated.

## Related

- Moment OIT / stochastic alpha: [MOMENT_OIT_STOCHASTIC_ALPHA.md](MOMENT_OIT_STOCHASTIC_ALPHA.md)
- 2027 north-star (visibility buffer on this spine): [RENDERER_2027.md](RENDERER_2027.md)
- Forward+ audit: [FORWARD_PLUS_PIPELINE_AUDIT.md](FORWARD_PLUS_PIPELINE_AUDIT.md)
- Modes overview: [RENDERERS.md](RENDERERS.md)
- Mode 1 deferred-only overlay: `vulkan_overlay_deferred.cfg`
- Hybrid1 (RTX): [HYBRID_RENDERING1.md](HYBRID_RENDERING1.md) — separate from this lighting mode
