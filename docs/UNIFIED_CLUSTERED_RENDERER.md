# Unified Clustered Renderer

The **Unified Clustered Renderer** (`r_renderMode 3`) is the engine’s high-fidelity **opt-in** lighting path. It combines deferred opaque shading, GPU-built light lists, Forward+ transparency, order-independent transparency, temporal reconstruction, and visibility-buffer migration within one coordinated frame architecture.

Spine 1.0 shipping default remains **Forward+ mode 2** (`modern_vulkan.cfg` → `modern_vulkan_stable.cfg`). Mode 3 is enabled via `modern_clustered.cfg` or the overlay below — see [RENDERER_SPINE_1.0.md](RENDERER_SPINE_1.0.md). Spine 1.1 opt-in cert (mode 3 + WBOIT + Temporal Reconstruction + weapon-after): [RENDERER_SPINE_1.1.md](RENDERER_SPINE_1.1.md).

Surface-class ownership (opaque deferred vs Forward+ transparent/weapon/OIT) is defined in [RENDERER_PATH_OWNERSHIP.md](RENDERER_PATH_OWNERSHIP.md). Visibility-buffer late shade is an **opt-in sidecar** (`r_visibilityBuffer`), not `r_renderMode 4` (mode 4 remains Tier B Selective Hybrid).

Shared cluster aliases: `r_clusterZSlices` → `r_forwardPlusZSlices`, `r_clusterDebug` → `r_forwardPlusDebug`, `r_clusterTileSize` = 16.

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

The current implementation uses a shared Forward+ **light grid**: 16×16 screen tiles, optionally expanded by **logarithmic Z-slices** (`r_forwardPlusZSlices`; default **1** = 2D-only; clustered profiles set **8**). Deferred, Forward+, and OIT consume the same cluster index lists (`forward_plus_cluster.glsl`).

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

The production light assignment path supports:

* **2D tiled** lists when `r_forwardPlusZSlices 1` (legacy / diagnostic fallback)
* **Depth-partitioned frustum clusters** (depth-partitioned) when `r_forwardPlusZSlices` is **2–16** (logarithmic by default via `r_forwardPlusZSliceMode 1`)

Cluster layout is `tileXY + slice * tilesX * tilesY`, shared by deferred opaque, Forward+ transparent, OIT, FSA, and NVC consumers. Mode 3 latches `r_forwardPlusOverflowShade 1` so lights 32–63 shade on transparent paths the same way deferred already does. Clustered profiles enable `r_forwardPlusZSlices 8`; set **1** for an explicit 2D tiled comparison.

Also called historically: Hybrid Clustered Deferred Renderer / Deferred + Forward+ Pipeline.

## Enable

Opt-in profile: `exec modern_clustered.cfg` (or overlay below). Spine stable default stays mode 2.

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
- **Presentation AA**: shipping `modern_vulkan.cfg` uses **SMAA 1x** (`r_aaMode 2`, `r_taa 0`) with `r_taaMotionVectors 1` scaffolding. In-game HUD/menu StretchPics render to the **UI overlay** and are alpha-composited **after** tonemap (`uiOverlayContentValid` → `overlay_compose`), so SMAA/Temporal Reconstruction never blur or discard 2D. Main-menu / no-world UI draws into `color_image` via the post_bloom fallback and skips spatial AA for crisp text. Opt-in Temporal Reconstruction: `exec vulkan_overlay_temporal_recon.cfg`. Bisect with `renderer_clustered_safe` (AA off).

## OIT + mode 3

When `r_oit` 1/2 is on with mode 3, the backend runs **`vk_oit_pass` instead of** the Forward+ transparent shade pass (`drawSurfFilter=2`). Moments/accum/resolve still composite over the deferred opaque base.

**WBOIT (`r_oit 1`) + `r_oitForwardPlus 1` (default):** accumulation samples Forward+ tile lights (set 2) using world-space position from the object→world push matrix. **MBOIT (`r_oit 2`) + `r_oitForwardPlus 1`:** moments pass stays unlit; accum samples the same tile lists on set 4.

```
exec vulkan_overlay_oit_clustered.cfg
vid_restart
```

Or demo: `exec demo_oit_clustered.cfg` (adds `r_stochasticAlpha 2` + TAA). Keep `r_ext_multisample 0`.

With Temporal Reconstruction (`r_aaMode` 4/5), OIT reveal coverage stamps a full-res R8 **reactive mask** so glass/smoke prefer the current frame (`r_temporalReactiveMask 1`). See [HDR_GAPS.md](HDR_GAPS.md) §6.8.

## Late-shade (optional exclusive)

`r_visibilityBufferLateShade 1` (latched) + `r_visibilityBufferFill 2` + non-MSAA: opaque lighting runs the **late-shade** consumer only (G-buffer MRTs + Forward+ tiles). Classic `vk_deferred_lighting_apply_after_geometry` is skipped — no dual lighting path. Debug mode 5 previews class×albedo. Neural/Hybrid1 still read the G-buffer until migrated.

## Related

- Moment OIT / stochastic alpha: [MOMENT_OIT_STOCHASTIC_ALPHA.md](MOMENT_OIT_STOCHASTIC_ALPHA.md)
- 2027 north-star (visibility buffer on this spine): [RENDERER_2027.md](RENDERER_2027.md)
- Forward+ audit: [FORWARD_PLUS_PIPELINE_AUDIT.md](FORWARD_PLUS_PIPELINE_AUDIT.md)
- Modes overview: [RENDERERS.md](RENDERERS.md)
- Mode 1 deferred overlay: `vulkan_overlay_deferred.cfg`
- Hybrid1 (RTX): [HYBRID_RENDERING1.md](HYBRID_RENDERING1.md) — separate from this lighting mode
