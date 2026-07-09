# VUDA — CUDA-Vulkan spatial multiplexing (experimental)

**VUDA** (*Breaking CUDA-Vulkan Isolation for Spatial Sharing of Compute and Graphics on the Same GPU*, Xu et al., [arXiv:2605.01352](https://arxiv.org/abs/2605.01352)) enables **concurrent** CUDA compute and Vulkan rendering on one NVIDIA GPU instead of strict time-sliced isolation between graphics and compute contexts.

This engine implements a **practical interop + scheduling layer** aligned with that research, plus an **analytical throughput model** for embodied-AI simulation (ManiSkill-style workloads).

## Research vs engine

| Research VUDA (driver-level) | Engine implementation |
|----------------------------|-------------------------|
| Channel redirection into Vulkan TSG | Frame **compute window** after `vkQueueSubmit`; `vuda_bind_stream` (CUDA streams) |
| Page-table grafting (UVM kernel module) | **KHR external memory fd** import + graft **cost model** (`vuda_model_graft`) |
| Paper API (Table 1) | Documented via `vuda_api`; runtime bind via `vuda_bind_stream` |
| Zero-copy unified VA | Exported Vulkan buffers → `cudaImportExternalMemory` |

Full channel redirection and page-table grafting require proprietary NVIDIA driver interfaces; the model commands quantify expected gains from the paper’s evaluation.

## Paper programming interface (Table 1)

| Interface | Engine mapping |
|-----------|----------------|
| `CUstream_bind(s)` | `vuda_bind_stream <0\|1\|2>` (physics / neural / inference) |
| `CUstream_unbind(s)` | `vuda_unbind_stream <slot>` |
| `step_async()` | `vuda_step_async [heartbeat\|physics\|neural\|inference] [bytes]` |
| `wait_step()` | `vuda_wait_step` |
| `render_async()` / `wait_render()` | Vulkan submit + `vuda_wait_render` (`cudaWaitExternalSemaphoresAsync` on render timeline) |

## When to use

| Scenario | Setup |
|----------|--------|
| In-process neural / physics on same GPU as Vulkan | `./scripts/compile_engine.sh vulkan vuda`, `r_vuda 1`, `cl_vuda 1`, `vid_restart` |
| Pipelined spatial mux (mode 2) | `r_vuda_mode 2` — skip frame-begin CUDA wait; overlap sim/render phases |
| Model paper speedups (no GPU) | `cl_vuda_model 1`, `vuda_maniskill`, `vuda_model_datagen` |
| FLUX / TRELLIS | Keep out-of-process (`cl_flux_external 1`) unless migrating kernels in-process |

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
| `r_vuda_mode` | `1` | `1`=interop fd, `2`=spatial_mux (pipelined) |
| `r_vuda_slotMb` | `64` | Per-slot shared buffer size (MiB) |
| `r_vuda_mux` | `1` | Open compute window after queue submit |
| `r_vuda_computeMs` | `2` | Client CUDA budget per frame (ms) |
| `r_vuda_coStreamMask` | `7` | Bitmask: `1` physics, `2` neural, `4` inference |
| `r_vuda_syncCuda` | `1` | Vulkan waits on CUDA timeline at frame begin (mode 1) |
| `cl_vuda` | `0` | Client scheduler + CUDA import |
| `cl_vuda_auto_job` | `1` | Run CUDA job when compute window opens |
| `cl_vuda_model` | `1` | Analytical model console commands |
| `cl_vuda_overlap` | `0.85` | Overlap efficiency for throughput model |

## Console — model (always available)

```
vuda_model_status
vuda_api
vuda_model_datagen [sim_ms] [render_ms] [batch]
vuda_model_rl [mlp|vla] [infer_ms] [sim_ms] [render_ms] [batch]
vuda_model_graft [num_2mb_buffers]
vuda_maniskill [batch]
```

## Console — runtime (`USE_VUDA` build)

- `vuda_status` — CUDA backend, interop, bound streams, timelines
- `vuda_reload` — Re-import Vulkan FDs into CUDA
- `vuda_run` — One-shot heartbeat job
- `vuda_bind_stream` / `vuda_unbind_stream` — Paper `CUstream_bind` / `unbind`
- `vuda_step_async [kind] [bytes]` — Queue a paper-style async CUDA step using the imported render timeline
- `vuda_wait_step` — Notify Vulkan that the queued CUDA step has completed
- `vuda_wait_render` — Wait on the current render timeline before launching CUDA work

These commands are a scaffold-level mapping of the paper API. The current CUDA job path still uses the engine's placeholder kernels and timeline synchronization instead of the paper's driver-level channel redirection.

## Pipeline

1. **Device init**: enable external memory/semaphore fd + timeline.
2. **Map load / `r_vuda`**: allocate exportable device-local buffers + timeline semaphores; export POSIX fds.
3. **Frame**: Vulkan renders → `vkQueueSubmit` → signal render timeline → **compute window** → CUDA waits on timeline → kernels → signal CUDA timeline → present.
4. **Mode 2**: skip blocking CUDA wait at frame begin to allow pipelined overlap (paper inter-step / inter-trajectory parallelism).

## Throughput model

Data generation (inter-step): `sim(k+1)` overlaps `render(k)` when spatial sharing is effective.

\[
\text{speedup} \approx \frac{T_{sim} + T_{render}}{T_{sim} + T_{render} - \eta \min(T_{sim}, T_{render})}
\]

RL uses inter-trajectory batching; VLA mode treats inference as off-device. See `vuda_maniskill` for a sweep aligned with paper Figures 7–9.

## Limitations

- **Linux NVIDIA only**; no Windows WSL interop in this path yet.
- Jobs are **placeholder** (`cudaMemset` on shared slot) — replace with real physics/neural kernels.
- Does **not** patch NVIDIA UVM / RM drivers (page-table graft is modeled, not executed).
- Research **channel redirection** is approximated via timeline sync + compute windows + stream binding.

## References

- Xu et al., arXiv:2605.01352 — VUDA design and ManiSkill evaluation
- [NEURAL_RENDERER_PHASES.md](NEURAL_RENDERER_PHASES.md)
- [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md)
