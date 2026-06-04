# Wavefront path tracing experiment (WPT)

Experimental **wave-scheduled** path queue for lookdev and FSA/RTX pairing — not production GI.

## Idea

Classic path tracers batch rays by bounce (wavefront). This module:

1. **`wpt_enqueue.comp`** — wave 0: build per-pixel ray records from depth (primary rays).
2. **`wpt_wave.comp`** — waves 1..N: screen-space march + diffuse bounce (proxy for extension rays).
3. **`wpt_composite.comp`** — accumulate ray radiance into HDR.

Pair with **`r_fsa 1`** + **`r_wpt_fsaBridge 1`** + **`r_rtx 1`** for hardware trace experiments after the importance pass.

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_wpt` | `0` | Master toggle (latched) |
| `r_wpt_strength` | `0.35` | Bounce radiance scale |
| `r_wpt_scale` | `0.5` | Reserved (v1 runs full-res queue) |
| `r_wpt_bounces` | `1` | Extension waves (`0`–`2`) |
| `r_wpt_stepScale` | `1` | Screen-space march distance scale |
| `r_wpt_useGBuffer` | `1` | Deferred normals for bounces |
| `r_wpt_skipSky` | `1` | Skip sky on composite |
| `r_wpt_fsaBridge` | `1` | Document FSA+RTX pairing |
| `r_wpt_debug` | `0` | Developer logging |

## Console

- `wpt_status` — active state and ray queue size

## Requirements

- `r_fbo 1`, loaded map, Vulkan renderer

## Limitations (v1)

- Bounces are **screen-space proxies**, not TLAS path tracing.
- No ray sorting or megakernel; queue is one SSBO per pixel.
- Future: KHR RT waves, merge with `vk_rtx` world BLAS, sorted queues per direction bucket.

## References

- [NEURAL_RENDERER_PHASES.md](NEURAL_RENDERER_PHASES.md) — Phase 1
- [FORGET_SUPERRESOLUTION_FSA.md](FORGET_SUPERRESOLUTION_FSA.md)
