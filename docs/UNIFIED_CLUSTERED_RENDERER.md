# Unified Clustered Renderer

The **Unified Clustered Renderer** is the engine’s primary high-fidelity rendering path. It combines deferred opaque shading, GPU-built light lists, Forward+ transparency, order-independent transparency, temporal reconstruction, and visibility-buffer migration within one coordinated frame architecture.

```cfg
r_renderMode 3
```

The renderer is designed around **lighting ownership** rather than a single universal shading pass:

* opaque world geometry uses deferred material evaluation and dynamic lighting
* transparent, blended, weapon, and isolated view surfaces use Forward+
* OIT surfaces use the shared tiled-light infrastructure during accumulation
* HUD and presentation layers remain outside world temporal history
* late shading can replace the conventional deferred consumer without duplicating lighting

This allows each surface category to use the shading method best suited to its visibility, blending, material, and temporal requirements while preserving a common light representation across the frame.

## Architecture

| System | Responsibility |
|--------|----------------|
| GPU light preparation | Packs visible lights and builds screen-space light lists |
| Opaque material capture | Records geometry, normals, material data, and optional classification |
| Deferred lighting | Evaluates opaque world lighting in compute (optional **VRCS** via `r_vrcs` — [VARIABLE_RATE_COMPUTE.md](VARIABLE_RATE_COMPUTE.md)) |
| Forward+ shading | Handles transparent, blended, weapon, and isolated-view surfaces |
| OIT integration | Resolves complex transparency over the opaque lighting result |
| Visibility late shading | Optional exclusive consumer for deferred material data |
| Temporal presentation | Motion vectors, TAA, bloom, neural effects, and final composition |
| UI overlay | HUD and 2D presentation outside world reconstruction history |

The current production implementation uses **2D tiled** 16×16 screen-space light lists shared across deferred, Forward+, and OIT paths. The light-grid abstraction is intentionally structured so **depth-partitioned frustum clusters** can be introduced without replacing the surrounding material, transparency, or frame-composition architecture.

## Design goals

The mode 3 renderer is built to provide:

* high dynamic-light counts with bounded per-pixel light evaluation
* consistent lighting between opaque and transparent surfaces
* independent shading ownership for world, weapon, transparency, and UI passes
* compute-driven opaque lighting
* material classification and visibility-buffer compatibility
* stable temporal behavior across world and first-person rendering
* production OIT support
* explicit fallbacks for debugging and lower-complexity configurations
* a migration path toward depth-clustered and GPU-driven scene rendering

## Rendering model

The renderer should not be described as simply deferred or Forward+.

It is a **unified heterogeneous shading pipeline** in which multiple shading techniques operate over shared scene, light, depth, material, and temporal data.

The product name is **Unified Clustered Renderer**.

The current light assignment implementation is a **2D tiled light grid**. Depth-partitioned clusters are a planned extension of the same architecture rather than a separate renderer.

Also called historically: Hybrid Clustered Deferred Renderer / Deferred + Forward+ Pipeline.

## Enable

Shipping default is **`modern_vulkan.cfg`** (`r_renderMode 3`). Equivalent profile: `exec modern_clustered.cfg`.

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
3. G-buffer capture + visibility fill (optional)
4. Opaque lighting: deferred compute + composite **or** exclusive late-shade (`r_visibilityBufferLateShade`)
5. Transparent draw (`drawSurfFilter` 2) with Forward+ shade, **or** OIT when `r_oit` is on
6. Neural / bloom / TAA / presentation as usual

Depth is **not** cleared between opaque lighting and transparent draws.

Deferred lighting transforms direct-export **world** normals to view space, and can consume the material class map when `r_deferredMaterialClassify` and `r_materialClassify` are on — see [RENDERER_2027.md](RENDERER_2027.md).

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
- Mode 1 deferred overlay: `vulkan_overlay_deferred.cfg`
- Hybrid1 (RTX): [HYBRID_RENDERING1.md](HYBRID_RENDERING1.md) — separate from this lighting mode
