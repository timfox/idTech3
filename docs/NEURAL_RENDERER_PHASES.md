# Neural renderer phases (roadmap hub)

Living map of Vulkan **chocolate** (shipping-available, cvars off by default) vs **scaffold** (research) modules.

| Tier | Meaning | Build |
|------|---------|--------|
| **Chocolate** | Always linked (or flag-gated product module); cvars default **0** | Default `game` profile; no `USE_EXPERIMENTAL_RENDERERS` |
| **Scaffold** | Research / paper path | `USE_EXPERIMENTAL_RENDERERS=ON` (`full` / `research`) |
| **Done** | Production-oriented default path | — |

See also [RENDERERS.md](RENDERERS.md), [PRODUCTION_GAP_PLAN.md](PRODUCTION_GAP_PLAN.md), [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md), [RENDERER_2026_ARCHITECTURE_PASS.md](RENDERER_2026_ARCHITECTURE_PASS.md).

---

## Chocolate (graduated)

| Feature | Status | Gate | Cvars | Docs |
|---------|--------|------|-------|------|
| **Forward+ + TAA look** | Chocolate | always | `r_renderMode 2`, `r_taa`, `r_taaMotionVectors` | [demo_idtech8_look.cfg](../examples/demo_game/mod/demo_idtech8_look.cfg) |
| **Temporal upscale** | Chocolate | always | `r_upscale` 1\|2, `upscale_status` | [RENDERERS.md](RENDERERS.md) |
| **Open-world stream LOD** | Chocolate | always | `r_bspStreamLod`, `bsp_stream_status` | [OPEN_WORLD.md](OPEN_WORLD.md), [DISTRICTS.md](DISTRICTS.md) |
| **Virtual texture scaffold** | Chocolate | always | `r_vt`, `vt_*` | [VIRTUAL_TEXTURE.md](VIRTUAL_TEXTURE.md) |
| **Meshlets (cull + compact draw)** | Chocolate | always | `r_meshlets`, `meshlet_status` | [MESHLETS.md](MESHLETS.md) |
| **Mobile-GS / WebSplatter / SqueezeMe** | Chocolate | always linked | `r_mgs`, `r_wsp`, `r_squeezeme` | [MOBILE_GAUSSIAN_SPLATTING.md](MOBILE_GAUSSIAN_SPLATTING.md), [WEB_SPLATTER.md](WEB_SPLATTER.md), [SQUEEZEME.md](SQUEEZEME.md) |
| **Hybrid1 + Raygun** | Chocolate RT tier | `USE_VULKAN_RTX` | `r_hybrid1`, `r_raygun`, `r_rtxDemo` | [HYBRID_RENDERING1.md](HYBRID_RENDERING1.md), [RAYGUN.md](RAYGUN.md) |
| **Arc Blanc ocean** | Chocolate | `USE_ARC_BLANC` | `r_arcBlanc` | [ARC_BLANC.md](ARC_BLANC.md) |

CMake: [`cmake/renderers/VulkanExtensionSources.cmake`](../cmake/renderers/VulkanExtensionSources.cmake) — chocolate lists vs `VK_EXPERIMENTAL_RENDERER_SRCS`.

---

## Phase 1 — Practical wins (scaffold)

| Feature | Status | Cvars / commands | Docs |
|---------|--------|------------------|------|
| **Wavefront path experiment** | Scaffold | `r_wpt`, `wpt_status` | [WAVEFRONT_PATH_TRACING.md](WAVEFRONT_PATH_TRACING.md) |
| **Neural Irradiance Volume (NIV)** | Scaffold + asset I/O | `r_niv`, `niv_reload`, `niv_status` | [NEURAL_IRRADIANCE_VOLUME.md](NEURAL_IRRADIANCE_VOLUME.md) |
| **Neural six-way fog/smoke (NSLM)** | Scaffold + asset I/O | `r_nslm`, `r_volumetricFog 1`, `nslm_*` | [NEURAL_SIXWAY_LIGHTMAPS.md](NEURAL_SIXWAY_LIGHTMAPS.md) |
| **Temporal lightmap compression (NDGI)** | Scaffold (CPU decode) | `r_ndgi`, `ndgi_*` | [NEURAL_DYNAMIC_GI.md](NEURAL_DYNAMIC_GI.md) |
| **FSA + RTX adaptive** | Scaffold | `r_fsa`, `r_rtx`, `r_fsa_rtxAdaptive` | [FORGET_SUPERRESOLUTION_FSA.md](FORGET_SUPERRESOLUTION_FSA.md) |

**Phase 1 asset formats** (via `vk_neural_io.c`):

| Magic | File | Use |
|-------|------|-----|
| `NIV1` | `.nivb` | NIV MLP weights |
| `NIV2` | `.bin` | NIV feature volume |
| `NSL1` | `.nslb` | NSLM weights |
| `NSL2` | `.bin` | NSLM froxel volume |
| `NDG1` | `.ndgib` | NDGI weights (existing) |

**Phase 1 next steps:** GPU `ndgi_decompress.comp` → lightmap storage bind; hardware WPT waves on `vk_rtx` TLAS; offline trainers for volumes/weights.

---

## Phase 2 — Signature renderer tech (scaffold)

| Feature | Status | Cvars | Docs |
|---------|--------|-------|------|
| **Neural Image-Space Tessellation (NIST)** | Scaffold + `NIS1` load | `r_nist`, `nist_*` | [NEURAL_IMAGE_SPACE_TESSELLATION.md](NEURAL_IMAGE_SPACE_TESSELLATION.md) |
| **ReSTIR + neural visibility (NVC)** | Scaffold + `NVC1` load | `r_nvc`, `r_forwardPlus 1`, `nvc_*` | [NEURAL_VISIBILITY_CACHE.md](NEURAL_VISIBILITY_CACHE.md) |
| **Variable-rate textures (VT)** | Scaffold | `r_ndgi_vt`, `r_ndgi_bc` | [NEURAL_DYNAMIC_GI.md](NEURAL_DYNAMIC_GI.md) |
| **Vertex-attached neural GI (VFGI)** | Scaffold + `VFG1` load | `r_vfgi`, `vfgi_*` | [VERTEX_FEATURES_NEURAL_GI.md](VERTEX_FEATURES_NEURAL_GI.md) |

Static textures: **BC7/KTX2** via `tr_image_ktx2.c` (not neural).

**Weight magics:** `NIS1` (scalar), `NVC1` (scalar), `VFG1` (RGB), `NDG1` (RGB).

---

## Phase 3 — Future-looking R&D

| Feature | Status | Cvars | Docs |
|---------|--------|-------|------|
| **SqueezeMe / MGS / WSP** | **Chocolate** (see table above) | `r_squeezeme`, `r_mgs`, `r_wsp` | graduated out of experimental stub blob |
| **Gaussian ray tracing (GRTX)** | Scaffold (RTX + experimental) | `r_grtx`, `r_grtxDemo` | [GAUSSIAN_RAY_TRACING_GRTX.md](GAUSSIAN_RAY_TRACING_GRTX.md) |
| **VUDA CUDA–Vulkan** | Scaffold | `r_vuda`, `cl_vuda` (build `vuda`) | [VUDA.md](VUDA.md) |
| **RenderFormer preview** | Scaffold | `r_renderformer`, `renderformer_*` | [RENDERFORMER.md](RENDERFORMER.md) |

---

## Suggested demo presets

Shipping look (no experimental):

```cfg
exec demo_idtech8_look.cfg
```

Research comment blocks remain in `examples/demo_game/mod/demo_sp_slice.cfg`. Chocolate RT:

```bash
./scripts/compile_engine.sh vulkan rtx
# then: exec demo_hybrid1.cfg
```

Latched cvars need `vid_restart` or map reload where noted in per-feature docs.

---

## Frame hook order (Vulkan)

After opaque geometry (`tr_backend.c`):

1. Deferred G-buffer + lighting  
2. NIV → NIST → NVC → VFGI → RenderFormer (scaffold)  
3. **WPT** (scaffold)  
4. FSA importance (scaffold)  
5. WSP or MGS (chocolate)  
6. Hybrid1 / Raygun / RTX demo (chocolate when `USE_VULKAN_RTX`)  

NDGI updates BSP lightmaps in `R_NDGI_FrameUpdate()` (not in this chain).

---

## Implementation index

| Module | Sources |
|--------|---------|
| Shared I/O | `vk_neural_io.c`, `vk_neural_io.h` |
| WPT | `vk_wpt.c`, `shaders/glsl/wpt/*` |
| NIV / NSLM / NDGI / NIST / NVC / VFGI / FSA / RF | `renderers/vulkan/extensions/neural/` |
| Chocolate splats / RT / ocean | `extensions/splats/`, `extensions/rtx/vk_hybrid1.c`, `vk_raygun.c`, `extensions/scaffold/vk_arc_blanc*.c` |

Build shaders after GLSL edits:

```bash
./scripts/compile_shaders.sh --apply
./scripts/compile_engine.sh vulkan
```
