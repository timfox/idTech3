# VUDA — CUDA-Vulkan spatial multiplexing (experimental)

**VUDA** (*Breaking CUDA-Vulkan Isolation for Spatial Sharing of Compute and Graphics on the Same GPU*, May 2026) enables **concurrent** CUDA compute and Vulkan rendering on one NVIDIA GPU instead of strict time-sliced isolation between graphics and compute contexts.

This engine implements a **practical interop + scheduling layer** aligned with that research direction:

| Research VUDA (driver-level) | Engine v1 (today) |
|----------------------------|-------------------|
| Channel redirection into Vulkan TSG | Frame **compute window** after `vkQueueSubmit`, before present |
| Page-table grafting (kernel module) | **KHR external memory fd** — CUDA imports Vulkan device memory |
| Annotated CUDA streams API | `r_vuda_coStreamMask`, job kinds (neural / physics / inference) |
| Zero-copy unified VA | Exported Vulkan buffers → `cudaImportExternalMemory` |

Full channel redirection and page-table grafting require proprietary driver interfaces and are documented here as the **target path** when NVIDIA exposes stable cross-API spatial sharing APIs.

## When to use

| Scenario | Setup |
|----------|--------|
| In-process neural / physics on same GPU as Vulkan | Build with **`USE_VUDA`**, `r_vuda 1`, `cl_vuda 1`, `vid_restart` |
| FLUX / TRELLIS still out-of-process | Keep `cl_flux_external 1`; use VUDA for future in-process CUDA kernels |
| Debug interop | `vuda_reload`, `vuda_run`, `vuda_status` |

Requires **Linux + NVIDIA**, **libcudart**, and Vulkan extensions:

- `VK_KHR_external_memory` + `_fd`
- `VK_KHR_external_semaphore` + `_fd`
- `VK_KHR_timeline_semaphore`

## Build

```bash
./scripts/compile_engine.sh vulkan vuda
```

CMake: `-DUSE_VUDA=ON` (disabled on Windows/macOS by default).

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_vuda` | `0` | Master toggle (latched; `vid_restart`) |
| `r_vuda_mode` | `1` | `0`=off, `1`=interop, `2`=reserved spatial mux |
| `r_vuda_slotMb` | `64` | Per-slot shared buffer size (MiB) |
| `r_vuda_mux` | `1` | Open compute window after queue submit |
| `r_vuda_computeMs` | `2` | Client CUDA budget per frame (ms) |
| `r_vuda_coStreamMask` | `7` | Bitmask: `1` physics, `2` neural, `4` inference |
| `r_vuda_syncCuda` | `1` | Vulkan waits on CUDA timeline at frame begin |
| `cl_vuda` | `0` | Client scheduler + CUDA import |
| `cl_vuda_auto_job` | `1` | Run neural-stage job when compute window opens |

## Console

- `vuda_status` — CUDA backend, interop, slot sizes
- `vuda_reload` — Re-import Vulkan FDs into CUDA
- `vuda_run` — One-shot heartbeat job (memset on shared buffer)

## Pipeline

1. **Device init**: enable external memory/semaphore fd + timeline; resolve `GetMemoryFdKHR` / `WaitSemaphoresKHR`.
2. **Map load / `r_vuda`**: allocate `VUDA_MAX_SLOTS` exportable device-local buffers + timeline semaphores; export POSIX fds.
3. **Frame**: Vulkan renders → `vkQueueSubmit` → **compute window** (signal `cuda_wait` timeline) → client `CL_VUDA_Frame` runs CUDA → signals `cuda_signal` → present.
4. **Future**: bind FLUX in-process, VFGI feature uploads, physics CUDA, without copying through host RAM.

## Content

Optional manifest `vfgi/<map>.vuda` or `maps/<map>.vuda` (reserved for co-stream weights / slot layout).

## Limitations (v1)

- **Linux NVIDIA only**; no Windows WSL interop in this path yet.
- Jobs are **placeholder** (`cudaMemset` on shared slot) — replace with real kernels.
- Does **not** replace separate-process FLUX/TRELLIS (still recommended for stability).
- Research-grade **spatial channel sharing** not available in open-source drivers; interop + scheduling is the supported production approach today.

## References

- [VERTEX_FEATURES_NEURAL_GI.md](VERTEX_FEATURES_NEURAL_GI.md) — vertex neural features (Vulkan)
- [FORGET_SUPERRESOLUTION_FSA.md](FORGET_SUPERRESOLUTION_FSA.md) — adaptive RTX sampling
- [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md)
- arXiv: *VUDA: Breaking CUDA-Vulkan Isolation…* (2605.01352)
