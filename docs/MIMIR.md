# Mímir — CUDA/Vulkan interop visualization (experimental)

**Mímir** (*A real-time interactive visualization library for CUDA programs*, Carter, Hitschfeld & Navarro, [arXiv:2504.20937](https://arxiv.org/abs/2504.20937)) maps simulation data in GPU memory to Vulkan graphics resources without per-frame host transfers. The upstream library is a standalone C++ API (`allocLinear`, `createView`, `display` / `prepareViews` / `updateViews`).

This engine ships a **Vulkan compute scaffold** aligned with the paper’s Brownian point-cloud benchmark (§4.1), plus an **analytical benchmark model** (Figs. 5–9). Full upstream Mímir (Slang shaders, Lines/Voxels views, editor window) is not embedded.

## Architecture (paper §3)

| Layer | Upstream Mímir | Engine scaffold |
|-------|----------------|-----------------|
| Compute | CUDA kernels on interop memory | Vulkan `mimir_brownian.comp` (default) or CUDA import when `USE_MIMIR_CUDA` |
| Graphics | Vulkan + Slang → SPIR-V | Vulkan compute disc splat `mimir_splat.comp` |
| Sync | `prepareViews` / `updateViews` | `mimir_prepare` / `mimir_update` + `r_mimir_sync` |

## Build

Default (Vulkan-only Brownian + splat):

```bash
./scripts/compile_engine.sh vulkan
```

CUDA/Vulkan external-memory import (`r_mimir_cuda 1`):

```bash
./scripts/compile_engine.sh vulkan mimir
```

Requires Linux + NVIDIA driver + `libcudart`. Enables `VK_KHR_external_memory_fd` and dlopens CUDA for buffer import validation.

## Runtime

```
r_mimir 1
vid_restart
mimir_step 60
mimir_status
```

Paper-aligned aliases:

```
mimir_display 60
mimir_display_async 60
```

Optional explicit sync (paper Listing 2):

```
mimir_prepare
mimir_step 1
mimir_update
```

## Cvars

| Cvar | Default | Role |
|------|---------|------|
| `r_mimir` | `0` | Master toggle (latched; `vid_restart`) |
| `r_mimir_points` | `4096` | Point count (64–1M) |
| `r_mimir_width` / `r_mimir_height` | `640` / `480` | Offscreen splat target |
| `r_mimir_sync` | `1` | `prepareViews`/`updateViews` gate |
| `r_mimir_sigma` | `0.35` | Brownian step scale |
| `r_mimir_cuda` | `0` | Export/import position buffer via CUDA (needs `mimir` build) |
| `r_mimir_debug` | `0` | Per-step logging |
| `cl_mimir_model` | `1` | Paper benchmark commands |

## Console — model (always when `cl_mimir_model 1`)

```
mimir_model_status
mimir_api
mimir_model [N]
mimir_interop [N] [interop|ram|opengl]
mimir_sync
```

## Console — renderer

```
mimir_status
mimir_step [N]
mimir_display [N]
mimir_display_async [N]
mimir_reset
mimir_prepare
mimir_update
```

`mimir_display` wraps the paper-style synchronized `prepareViews` → work → `updateViews` flow.
`mimir_display_async` runs the same workload without requiring the explicit sync gate, mapping more directly to the upstream `displayAsync` idea.

## Paper benchmarks (model)

`mimir_model` prints interop vs RAM vs OpenGL at FHD (RTX 2070 SUPER, §4.1):

- Up to **9×** FPS vs host RAM path (Fig. 9)
- Up to **12×** faster total visualization time
- **~1.5×** less GPU memory than RAM copy workflow

`mimir_sync` lists synced vs unsynced FPS at N=1e6 (Fig. 6).

## v1 limitations

- **View types**: Markers-only (disc splat); Lines, Voxels, mesh indexing not wired
- **Display**: Offscreen compute target; not composited to the main framebuffer
- **CUDA kernel**: Import path validates interop; Brownian runs on Vulkan compute in v1
- **Slang**: Upstream uses Slang modules; engine uses precompiled GLSL compute shaders
- **NVIDIA-only interop**: Matches paper; OpenCL/AMD planned upstream

## References

- Paper: [arXiv:2504.20937](https://arxiv.org/abs/2504.20937)
- Related engine work: [VUDA.md](VUDA.md) (spatial multiplexing), [CURAST.md](CURAST.md) (compute viz scaffolds)
