# Neural renderer phases (roadmap hub)

Living map of **experimental Vulkan neural / RT modules** in this fork. Status: **Scaffold** = wired end-to-end with procedural defaults; **Done** = production-oriented path exists.

See also [RENDERERS.md](RENDERERS.md), [PRODUCTION_GAP_PLAN.md](PRODUCTION_GAP_PLAN.md), [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md).

---

## Phase 1 — Practical wins

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

## Phase 2 — Signature renderer tech

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
| **SqueezeMe avatars** | Demo | `r_squeezeme` 1 | [SQUEEZEME.md](SQUEEZEME.md) (arXiv:2412.15171) |
| **Mobile-GS splatting** | Demo | `r_mgs` 1–3 | [MOBILE_GAUSSIAN_SPLATTING.md](MOBILE_GAUSSIAN_SPLATTING.md) |
| **Gaussian ray tracing (GRTX)** | Demo (RTX) | `r_grtx`, `r_grtxDemo` | [GAUSSIAN_RAY_TRACING_GRTX.md](GAUSSIAN_RAY_TRACING_GRTX.md) |
| **WebSplatter** | Demo | `r_wsp` (overrides MGS) | [WEB_SPLATTER.md](WEB_SPLATTER.md) |
| **VUDA CUDA–Vulkan** | Scaffold | `r_vuda`, `cl_vuda` (build `vuda`) | [VUDA.md](VUDA.md) |
| **RenderFormer preview** | Scaffold | `r_renderformer`, `renderformer_*` | [RENDERFORMER.md](RENDERFORMER.md) |

---

## Suggested demo presets

In `examples/demo_game/mod/demo_sp_slice.cfg` (comment blocks):

```cfg
// Phase 1 — GI + volumetrics + WPT
// set r_fbo 1
// set r_niv 1
// set r_nslm 1
// set r_volumetricFog 1
// set r_ndgi 1
// set r_wpt 1
// set r_fsa 1
// set r_fsa_budget 0.25

// Phase 2 — screen + lights + vertices
// set r_nist 1
// set r_forwardPlus 1
// set r_nvc 1
// set r_vfgi 1
// set r_deferredGBuffer 1
// set r_deferredGBufferFill 1

// Phase 3 — splats / interop / mesh neural
// set r_mgs 2
// set r_renderformer 1
// ./scripts/compile_engine.sh vulkan vuda
// set r_vuda 1
// set cl_vuda 1
```

Latched cvars need `vid_restart` or map reload where noted in per-feature docs.

---

## Frame hook order (Vulkan)

After opaque geometry (`tr_backend.c`):

1. Deferred G-buffer + lighting  
2. NIV → NIST → NVC → VFGI → RenderFormer  
3. **WPT** (wavefront queue)  
4. FSA importance  
5. WSP or MGS  
6. Later: RTX demo / GRTX, FSA denoise  

NDGI updates BSP lightmaps in `R_NDGI_FrameUpdate()` (not in this chain).

---

## Implementation index

| Module | Sources |
|--------|---------|
| Shared I/O | `vk_neural_io.c`, `vk_neural_io.h` |
| WPT | `vk_wpt.c`, `shaders/glsl/wpt/*` |
| NIV / NSLM / NDGI / NIST / NVC / VFGI / FSA / RF | `vk_*.c` under `src/renderers/vulkan/` |

Build shaders after GLSL edits:

```bash
./scripts/compile_shaders.sh --apply
./scripts/compile_engine.sh vulkan
```
