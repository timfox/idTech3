# Path trace architecture benchmark (megakernel vs wavefront)

Experimental **Phase C6** comparison: one shared RTX world TLAS, two schedulers (`r_pathtrace_arch`). Not production SP lighting — use for RTX-lab profiling and Nsight studies.

## Prerequisites

- Build: `./scripts/compile_engine.sh vulkan rtx` (`USE_VULKAN_RTX=ON`)
- GPU: Vulkan 1.2+ with `VK_KHR_ray_tracing_pipeline` and acceleration structures
- Console (before `vid_restart`):

```text
r_rtx 1
r_rtxDemo 1
r_pathtrace 1
r_pathtrace_arch megakernel   // or wavefront
r_pathtrace_bounces 4
r_pathtrace_samples 1
vid_restart
```

`r_pathtrace` shares the **same world BLAS/TLAS** as the RTX demo (`vk_rtx.c`). `r_rtxDemo 0` skips demo pipeline init and leaves no TLAS for pathtrace.

## Cvars

| Cvar | Default | Notes |
|------|---------|--------|
| `r_pathtrace` | `0` | Master toggle (latched) |
| `r_pathtrace_arch` | `megakernel` | `megakernel` or `wavefront` (latched) |
| `r_pathtrace_bounces` | `4` | 1–8 path depth |
| `r_pathtrace_samples` | `1` | 1–64 (megakernel averages; wavefront primary path) |
| `r_pathtrace_denoise` | `0` | Depth-guided 3×3 blur on trace buffer (`pt_denoise.comp`); not OIDN/ReSTIR |
| `r_pathtrace_debug` | `0` | `1`=bounce heatmap, `2`=wave alive count (developer) |
| `r_pathtrace_composite` | `1` | Blit trace target to HDR color |

Startup logs: `[VK][PathTrace] Ready arch=...` when init succeeds.

## Architecture

- **Megakernel:** one `vkCmdTraceRaysKHR` per frame; bounce loop in `pt_mega.rgen`.
- **Wavefront:** seed + one trace dispatch per bounce (`pt_wave.rgen`); optional `pt_wave_compact.comp` alive count when `r_pathtrace_debug 2`.
- **Hit:** Lambert-style albedo stub in `pt_hit.rchit` (fair baseline, not production PBR).
- **Denoise:** `pt_denoise.comp` when `r_pathtrace_denoise 1` (depth-guided 3×3 on trace target).
- **Composite:** `pt_composite.comp` when `r_pathtrace_composite` &lt; 1; full blit when `1`.
- **Host:** `vk_pathtrace.c` — shares RTX TLAS with `r_rtxDemo`.

## Fixed capture recipe (manual)

1. Load a stable map; fixed camera (no movement).
2. `r_pathtrace_bounces 4`, `r_pathtrace_samples 1`, 1920×1080 (or your test res).
3. Run **megakernel** 60 s → note `r_speeds` / GPU ms.
4. `r_pathtrace_arch wavefront` + `vid_restart` → repeat.
5. Record delta; wavefront is not required to win by 16% on all GPUs.

## Nsight Graphics checklist

Capture one frame per arch with **GPU Trace** or **Range Profiler**:

| Metric | Megakernel | Wavefront | Notes |
|--------|------------|-----------|--------|
| Shader divergence / warp occupancy | | | RT raygen hotspots |
| `vkCmdTraceRaysKHR` count / frame | 1 | `bounces+1` | Queue pressure proxy |
| Raygen / closest-hit time | | | |
| L2 / memory throughput | | | Path queue SSBO (wave) |
| Bounce heatmap (`r_pathtrace_debug 1`) | optional | — | |
| Alive rays (`r_pathtrace_debug 2`) | — | per-bounce log | |

**Denoiser cost:** compare frame time with `r_pathtrace_denoise 0` vs `1` (spatial pass wired).

## CI

No GPU required: `renderer_regression_check.sh` verifies `vk_pathtrace.c` and shader sources; SPIR-V embedded via `compile_shaders.sh` → `vk_pathtrace_spirv.inc`.

## References

- [RENDERERS.md](RENDERERS.md) — cvar table
- [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md) — Phase C6 pointer vs `r_rtx` demo
- [SIM_RENDER_PROFILE.md](SIM_RENDER_PROFILE.md) — Nsight workflow
- [PRODUCTION_GAP_PLAN.md](PRODUCTION_GAP_PLAN.md) — SP-first plan context

## Observed results (fill on reference GPU)

| GPU | Map | Arch | Frame GPU ms | Notes |
|-----|-----|------|--------------|-------|
| _e.g. RTX 4070_ | _demo map_ | megakernel | _TBD_ | |
| _same_ | _same_ | wavefront | _TBD_ | |

_Wavefront vs megakernel delta is hardware- and scene-dependent; document actual numbers here after Nsight capture._
