# VkSplat — Vulkan compute 3DGS training (experimental)

**VkSplat** (*High-Performance 3DGS Training in Vulkan Compute*, Chen, Ibrahim & Liu, Eurographics 2026 / [arXiv:2605.00219](https://arxiv.org/abs/2605.00219)) is an end-to-end **3D Gaussian Splatting training** pipeline in **Vulkan compute**, cross-vendor and free of CUDA/PyTorch.

This engine implements a **training scaffold** aligned with the paper’s stage breakdown. Full fidelity training (radix sort, dual raster backward, fused SSIM, MCMC densify) matches the upstream [vksplat](https://github.com/harry7557558/vksplat) repository; here we ship integrated compute passes + an analytical benchmark model.

## Paper vs engine

| VkSplat (paper) | Engine scaffold |
|-----------------|-----------------|
| Scan-line exact tile culling | `vksplat_project_fwd` + `vksplat_tile_cull` (32-bit tile-depth keys) |
| Adaptive raster backward + Thompson sampling | `r_vksplat_bwdMode` (scheduler stub; per-Gaussian path in shader roadmap) |
| Fused proj-backward + Adam | `vksplat_adam.comp` |
| Fused L1+SSIM loss | Placeholder Adam nudge (full loss kernel planned) |
| 3.3× speed, 33% VRAM vs GSplat | `vksplat_model` (Table 2 timings) |

## Build & enable

Standard Vulkan build (no extra flag):

```bash
./scripts/compile_engine.sh vulkan
```

Shaders compile via `scripts/compile_shaders.sh` during CMake.

In-game:

```
r_vksplat 1
vid_restart
vksplat_train_step 4
vksplat_status
```

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_vksplat` | `0` | Master toggle (latched; `vid_restart`) |
| `r_vksplat_gaussians` | `4096` | Training Gaussian count (64–8192) |
| `r_vksplat_lr` | `0.01` | Adam learning rate |
| `r_vksplat_bwdMode` | `0` | Raster backward: 0=Thompson auto, 1=per-Gaussian, 2=shared-mem |
| `r_vksplat_debug` | `0` | Developer logging |
| `cl_vksplat_model` | `1` | Analytical benchmark commands |

## Console — model (always on)

```
vksplat_model_status
vksplat_api
vksplat_model [default|mcmc]
vksplat_quality [default|mcmc]
```

## Console — training (renderer)

```
vksplat_status
vksplat_train_step [N]
vksplat_reset
```

## Training step pipeline

1. **Projection forward** — world Gaussians → screen ellipse + tile count  
2. **Tile cull / key fill** — 32-bit tile-depth sort keys (§4.4)  
3. **Raster forward** — 16×16 tile dispatches, alpha compositing  
4. **Fused Adam** — parameter update on Gaussian SSBO  

Runs on a one-shot command buffer (`vk_begin_command_buffer`); does not require an active map draw.

## Benchmark model

`vksplat_model` prints GSplat vs VkSplat totals from paper Table 2:

- Default densify: **1384s → 412s** (~3.36×), VRAM **4.56 → 3.01 GiB**
- MCMC: **995s → 285s** (~3.49×), VRAM **1.37 → 0.93 GiB**

## Limitations (engine v1)

- Not a full NeRF/Colmap trainer — procedural seed Gaussians only  
- No radix sort pass yet (keys written, sort TODO)  
- Raster backward + SSIM loss are stubs  
- Cross-vendor validation (RX 7800 XT) documented in paper, not CI-tested here  

## See also

- [MOBILE_GAUSSIAN_SPLATTING.md](MOBILE_GAUSSIAN_SPLATTING.md) — inference splats (`r_mgs`)  
- [WEB_SPLATTER.md](WEB_SPLATTER.md) — tile-binned splat compositing  
- [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md)

## References

- Chen et al., Eurographics 2026 — VkSplat  
- Kerbl et al., SIGGRAPH 2023 — 3D Gaussian Splatting  
- Ye et al., JMLR 2025 — gsplat baseline
