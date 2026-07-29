# 2027 GPU-Driven Hybrid Visibility Renderer

North-star architecture for the idTech3 Vulkan renderer.

**Target name:** A GPU-driven hybrid visibility renderer using virtualized meshlet geometry, clustered compute shading, Forward+ transparency, reservoir-sampled ray tracing, and neural reconstruction.

This is more than “deferred plus Forward+.” Shipping today uses those as foundation layers; 2027 work converges GPU-driven rasterization, selective path tracing, and neural reconstruction/compression.

## Mode spine vs 2027 target

| Layer | Role |
|-------|------|
| **`r_renderMode 3`** | Unified Clustered — **Spine shipping default** (`modern_vulkan.cfg` → `modern_vulkan_stable.cfg`) |
| **`r_renderMode 3`** | [Unified Clustered](UNIFIED_CLUSTERED_RENDERER.md) — unified heterogeneous shading / lighting ownership (2D tiles + optional Z-slices). Opt-in via `modern_clustered.cfg`; **spine** for 2027 layers. Path ownership: [RENDERER_PATH_OWNERSHIP.md](RENDERER_PATH_OWNERSHIP.md) |
| **2027 target** | Mode 3 + visibility buffer + meshlets + reservoir RT + neural reconstruction |

Do **not** invent further `r_renderMode` values beyond Spine 1.2 without a certification gate. Spine 1.2 adds opt-in **`r_renderMode 4`** (Selective Hybrid) and **`5`** (Path-Traced Reference) — see [RENDERER_SPINE_1.2.md](RENDERER_SPINE_1.2.md). Sidecars that are not part of those tiers still stack on mode 3.

## Target frame graph

```text
1. GPU instance and meshlet culling
2. Virtualized geometry visibility pass
3. Compact visibility/depth buffer
4. Compute material classification
5. Clustered light-list generation
6. Deferred compute shading for opaque pixels
7. Forward+ shading for transparency, hair and view models
8. Sparse ray queries for shadows, GI and reflections
9. ReSTIR temporal/spatial path reuse
10. Neural denoising and ray reconstruction
11. Neural texture/material decompression
12. Sparse volumetric integration
13. Effect-specific resolution reconstruction
14. Final temporal/neural super-resolution
```

## Phase 1 (landed)

Compact **visibility-buffer production path** coexisting with the classic G-buffer:

| Cvar | Role |
|------|------|
| `r_visibilityBuffer` | Latch: allocate ID + bary + class RTs (production default in `modern_vulkan.cfg`; needs `r_fbo`, `r_renderMode` 1/2/3) |
| `r_visibilityBufferFill` | After opaque (mode 3) or geometry: fill packed draw/prim IDs + bary data; production default prefers PrimID MRT (`2`) |
| `r_visibilityBufferDebug` | 0=off, 1=drawId, 2=primId, 3=bary, 4=material class, 5=late-shade scaffold (albedo×class) |
| `r_materialClassify` | Compute class map from G-buffer material + depth (production default; needs visbuf latch; **not** Morton fill) |
| `r_deferredMaterialClassify` | Deferred lighting consumes class map (production default **1**) |

**Phase 1.5 (fill + late-shade preview):**

- Fill uses Morton locality + depth buckets when `r_visibilityBufferFill 1`, or true PrimID/drawId MRT when fill=2 and non-MSAA deferred export is live.
- Debug mode **5** samples G-buffer albedo × class tint as a late-shade scaffold.

**Deferred lighting notes (post–Phase 1):**

- Direct MRT normals are **world-space**; depth-fill normals are **view-space**. `deferred_lighting.comp` transforms world→view when `normalsAreWorld=1`.
- With `r_deferredMaterialClassify 1` + classify fill, specialized opaque dispatch uses the class map (EMPTY=sky; LAYERED/TRANSMISSION tune; EMISSIVE skips additive). ALPHA_TEST is reserved for real cutouts and is not inferred from low confidence.
- **VRCS:** `r_vrcs 1` wraps deferred lighting compute with variable-rate primaries + deblock — see [VARIABLE_RATE_COMPUTE.md](VARIABLE_RATE_COMPUTE.md).

Production profile:

```
exec modern_vulkan.cfg
vid_restart
```

Explicit overlay/re-apply path:

```
exec vulkan_overlay_visibility_2027.cfg
vid_restart
```

Demo: `exec demo_visibility_2027.cfg`. Console: `visibility_buffer_status`, `renderer_status`.

**Phase 1 encoding note:** `r_visibilityBufferFill 1` is depth-derived (tile draw id + depth prim proxy + intra-tile bary). `r_visibilityBufferFill 2` prefers true `gl_PrimitiveID` + monotonic drawId MRT (UV bary weights) when deferred direct export is non-MSAA; falls back to depth proxy otherwise. Neural/Hybrid1 consumers still read the classic G-buffer.

**Exclusive late-shade:** `r_visibilityBufferLateShade 1` (default 0) runs opaque lighting once from PrimID + G-buffer MRTs + Forward+ tiles and **skips** classic deferred lighting (no dual path). It remains staged until graph-owned opaque pass execution is promoted; production currently uses the class-map deferred consumer.

## Phase ladder

| Phase | Focus |
|-------|--------|
| **P1** | Visibility foundation + material-class stub (this doc / cvars above) |
| **P1.5** | Visbuf Morton/depth fill + late-shade debug mode 5 |
| **P2** | Virtual geometry meshlets — **default stable profile request** with `r_virtualGeometry`, `r_meshletsMdiDraw`, and `r_meshletsLod`; persistent IBO + production mesh-shader pipelines remain follow-up ([MESHLETS.md](MESHLETS.md)) |
| **P3** | Reservoir-sampled hybrid path — ReSTIR DI on Hybrid1, NVC/FSA; **bindless A.1c** bary UV + PrimUv ([RTX_HIT_SHADER_UV.md](RTX_HIT_SHADER_UV.md)) |
| **P4** | Material-classified OIT — mode 3 + MBOIT overlay; `r_oitClassify 1` two-bucket (alpha-blend vs additive); further class paths remain follow-up ([MOMENT_OIT_STOCHASTIC_ALPHA.md](MOMENT_OIT_STOCHASTIC_ALPHA.md)) |
| **P5** | Neural material/texture reconstruction (chocolate scaffold; no mandatory vendor SDK) |
| **P6** | Heterogeneous resolution, sparse volumetrics, OMM, character skin/hair paths |

## Fifteen pillars (today → target)

| # | Pillar | Today | Target |
|---|--------|-------|--------|
| 1 | Neural shaders / materials | NIV/NDGI/VFGI scaffolds | In-shader decode + appearance LOD |
| 2 | Ray reconstruction / ReSTIR | Hybrid1 + NVC scaffold | Reservoir-sampled hybrid path layer |
| 3 | Visibility buffer | Classic G-buffer + P1 sidecar | Compact prim/bary/depth + late shade |
| 4 | GPU-driven / Work Graphs | Virtual geometry meshlets + indexed MDI | Production mesh shaders / WG |
| 5 | Virtualized meshlets | MD3 meshlets + **MDI GPU draw** + **screen LOD** + sector stream | Continuous cluster LOD streaming |
| 6 | Unified Clustered spine | Mode 3 EXISTS — 2D tiles today; depth clusters planned extension | Spine for all layers |
| 7 | Stochastic alpha | `r_stochasticAlpha` | Temporally stable coverage + OMM |
| 8 | Classified OIT | Global WBOIT/MBOIT + **mode 3 overlay** | Per-material-class paths |
| 9 | Neural texture compression | VT / BC7 | Learned latent + decoder |
| 10 | Multidimensional LOD | Stream LOD + scales | Geo/mat/ray/rate/appearance |
| 11 | Material classify | Opaque/transparent split + P1 compute + **deferred consumer** | Specialized shade dispatch |
| 12 | Heterogeneous resolution | Per-effect scales | Unified adaptive sample budgets |
| 13 | Sparse volumetrics | Froxel fog + VDB | Sparse hybrid volumetric lighting |
| 14 | Digital humans | PBR slots + stochastic cards | Strand hair, SSS, tear-film |
| 15 | Coherence scheduling | WPT scaffold | Ray/material sort, SER-like |

## Related

- [UNIFIED_CLUSTERED_RENDERER.md](UNIFIED_CLUSTERED_RENDERER.md)
- [FORWARD_PLUS_PIPELINE_AUDIT.md](FORWARD_PLUS_PIPELINE_AUDIT.md)
- [NEURAL_RENDERER_PHASES.md](NEURAL_RENDERER_PHASES.md)
- [MESHLETS.md](MESHLETS.md)
- [HYBRID_RENDERING1.md](HYBRID_RENDERING1.md)
- [RENDERERS.md](RENDERERS.md)
- [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md)
