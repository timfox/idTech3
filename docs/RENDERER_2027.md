# 2027 GPU-Driven Hybrid Visibility Renderer

North-star architecture for the idTech3 Vulkan renderer.

**Target name:** A GPU-driven hybrid visibility renderer using virtualized meshlet geometry, clustered compute shading, Forward+ transparency, reservoir-sampled ray tracing, and neural reconstruction.

This is more than “deferred plus Forward+.” Shipping today uses those as foundation layers; 2027 work converges GPU-driven rasterization, selective path tracing, and neural reconstruction/compression.

## Mode spine vs 2027 target

| Layer | Role |
|-------|------|
| **`r_renderMode 2`** | Shipping default — Forward+ primary |
| **`r_renderMode 3`** | [Unified Clustered](UNIFIED_CLUSTERED_RENDERER.md) — deferred opaque + Forward+ transparent (shared tiles). **Spine** for 2027 layers |
| **2027 target** | Mode 3 + visibility buffer + meshlets + reservoir RT + neural reconstruction |

Do **not** invent `r_renderMode 4` for this architecture. Opt-in sidecars stack on mode 3.

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

Compact **visibility-buffer sidecar** coexisting with the classic G-buffer:

| Cvar | Role |
|------|------|
| `r_visibilityBuffer` | Latch: allocate ID + bary + class RTs (needs `r_fbo`, `r_renderMode` 1/2/3) |
| `r_visibilityBufferFill` | After opaque (mode 3) or geometry: compute fill of packed draw/prim IDs + bary proxies |
| `r_visibilityBufferDebug` | 0=off, 1=drawId, 2=primId, 3=bary, 4=material class |
| `r_materialClassify` | Compute class map from G-buffer material + depth |

Enable:

```
exec vulkan_overlay_visibility_2027.cfg
vid_restart
```

Demo: `exec demo_visibility_2027.cfg`. Console: `visibility_buffer_status`, `renderer_status`.

**Phase 1 encoding note:** fill is depth-derived (tile draw id + depth prim proxy + intra-tile bary). True `gl_PrimitiveID` / instance MRT export is a follow-up; Neural/Hybrid1 consumers still read the classic G-buffer.

## Phase ladder

| Phase | Focus |
|-------|--------|
| **P1** | Visibility foundation + material-class stub (this doc / cvars above) |
| **P2** | GPU-driven meshlets — real `vkCmdDrawIndexedIndirect` ([MESHLETS.md](MESHLETS.md)) |
| **P3** | Reservoir-sampled hybrid path — ReSTIR DI on Hybrid1, NVC/FSA, [RTX_HIT_SHADER_UV.md](RTX_HIT_SHADER_UV.md) |
| **P4** | Material-classified OIT — glass/smoke/hair/particles ([MOMENT_OIT_STOCHASTIC_ALPHA.md](MOMENT_OIT_STOCHASTIC_ALPHA.md)) |
| **P5** | Neural material/texture reconstruction (chocolate scaffold; no mandatory vendor SDK) |
| **P6** | Heterogeneous resolution, sparse volumetrics, OMM, character skin/hair paths |

## Fifteen pillars (today → target)

| # | Pillar | Today | Target |
|---|--------|-------|--------|
| 1 | Neural shaders / materials | NIV/NDGI/VFGI scaffolds | In-shader decode + appearance LOD |
| 2 | Ray reconstruction / ReSTIR | Hybrid1 + NVC scaffold | Reservoir-sampled hybrid path layer |
| 3 | Visibility buffer | Classic G-buffer + P1 sidecar | Compact prim/bary/depth + late shade |
| 4 | GPU-driven / Work Graphs | Meshlets CPU cull | Indirect draws → mesh shaders / WG |
| 5 | Virtualized meshlets | MD3 meshlets + sector stream | Continuous cluster LOD streaming |
| 6 | Hybrid clustered | Mode 3 EXISTS | Spine for all layers |
| 7 | Stochastic alpha | `r_stochasticAlpha` | Temporally stable coverage + OMM |
| 8 | Classified OIT | Global WBOIT/MBOIT | Per-material-class paths |
| 9 | Neural texture compression | VT / BC7 | Learned latent + decoder |
| 10 | Multidimensional LOD | Stream LOD + scales | Geo/mat/ray/rate/appearance |
| 11 | Material classify | Opaque/transparent split + P1 compute | Specialized shade dispatch |
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
