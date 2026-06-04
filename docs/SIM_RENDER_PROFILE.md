# Simulation render profile (AMBF-Vulkan alignment)

This engine fork maps key ideas from Allison et al., *Towards a Modern and Lightweight Rendering Engine for Dynamic Robotic Simulations* ([arXiv:2410.05095](https://arxiv.org/abs/2410.05095), CC BY 4.0) onto the existing Vulkan forward renderer. It is **not** a port of the standalone AMBF-Vulkan process or AVI shared-memory plugin; it adopts the paper’s **rendering stack choices** inside idTech3.

## Paper pipeline vs this engine

| AMBF-Vulkan (paper) | idTech3 Vulkan |
|---------------------|----------------|
| Vulkan main pass (PBR, MSAA) | `r_fbo`, `r_pbr`, `r_ext_multisample` |
| Post pass (FXAA) | `r_ext_fxaa` (new single-pass; alternative to SMAA) |
| Reinhard tonemap in main/post | `r_tonemap 1` (gamma pass) |
| Ray-traced hard shadows | `r_rtx 1` when built with `USE_VULKAN_RTX` |
| Draw sort by material/VB | Existing `drawSurf` sort (`tr_main.c` / `tr_backend.c`) |
| Vertex pulling (glTF) | `tr_gltf_topo.c` |
| ImGui debug overlay | Optional `r_studio_tools` / inspector |
| External sim IPC (AVI) | Out of scope (use game/server networking or custom mod IPC) |

## Quick start

**Lightweight (AMBF paper style, no volumetrics):**
```bash
sim_render_profile 1
vid_restart
```

**Simulation with accurate volumetrics:**
```bash
sim_render_profile 2
vid_restart
```

Profile **1** applies:

- FBO + HDR16 + PBR
- MSAA 4× + FXAA (SMAA off)
- Reinhard tonemap (`r_tonemap 1`)
- Volumetric fog, bloom, SSAO, SSR, TAA off

Profile **2** adds:

- `r_volumetricFog 1`, quality **3**, **64** composite march steps
- Physical composite (`r_volumetricFogCompositeMode 0`: `C = C_scene·T + L_in-scatter`)
- Shadowed froxels (`r_fog_shadows 1`), temporal blend **0.88**
- Tighter transmittance early-out (`r_volumetricFogTransmittanceCutoff 0.002`)
- Post chain: main → volumetric composite → FXAA → Reinhard gamma

## Volumetric accuracy (engine defaults)

Recent composite improvements (no extra cvars):

- **Depth-aligned froxel sampling**: march segments map view depth to froxel Z (matches compute froxel layout) with Z-interpolation between slice bounds.
- **MSAA depth resolve**: nearest-surface depth per pixel (min for reversed-Z) instead of sample 0 only.

| Cvar | Accurate default | Role |
|------|------------------|------|
| `r_volumetricFogCompositeMode` | `0` | Physical in-scatter composite |
| `r_volumetricFogSteps` | `48` | Per-pixel march steps (profile 2 → 64) |
| `r_volumetricFogTransmittanceCutoff` | `0.002` | Integration early-out |
| `r_volumetricFogDepthMode` | `1` | Reversed-Z depth decode |

## Cvars

| Cvar | Role |
|------|------|
| `r_simRenderProfile` | `0` off, `1` AMBF lightweight, `2` volumetric accurate |
| `r_ext_fxaa` | `1` enables FXAA post pass (requires `r_fbo 1`, latched). Mutually exclusive with `r_ext_smaa`. |
| `r_fxaa_subpix` | Sub-pixel quality (default `0.75`) |
| `r_postAaAfterBloom` | `1` re-runs FXAA/SMAA after bloom so gamma samples the final HDR image |
| `r_simRenderProfileAutoApply` | `0` set `1` to re-apply profile cvars every `vid_restart` |
| `r_simRenderDebug` | `0` off, `1` console stats (~1 Hz), `2` ImGui HUD (`r_imgui 1`) |

## Commands

| Command | Role |
|---------|------|
| `sim_render_profile 1\|2` | Apply AMBF lightweight or volumetric simulation stack |
| `sim_render_debug 0\|1\|2` | Toggle simulation render debug overlay |
| `volumetric_accurate` | Apply accurate froxel integration cvars without full sim profile |
| `volumetric_integration 0\|1\|2` | Froxel march, screen analytical approx, or screen ray march + sun shadows |

## Screen-space integration (reference-style)

Inspired by screen-space volumetric shaders (Henyey–Greenstein phase, height falloff, sun shadow map):

| `r_volumetricFogIntegration` | Behavior |
|------------------------------|----------|
| `0` | Froxel compute + 3D texture march (default; sim profile 2) |
| `1` | Single-sample analytical approximate (fast) |
| `2` | Per-pixel ray march with sun shadow map; **skips froxel compute** |

Tune with `r_volumetricFogDensity`, `r_volumetricFogHeightFalloff`, `r_volumetricFogAniso`, `r_volumetricFogSteps` (mode 2).

## RTX path trace profiling (Nsight)

For **megakernel vs wavefront** GPU path tracing (`r_pathtrace`), use the checklist in [PATHTRACE_ARCH_BENCHMARK.md](PATHTRACE_ARCH_BENCHMARK.md): fixed camera, `r_pathtrace_bounces 4`, compare `r_pathtrace_arch` with Nsight Graphics GPU Trace (divergence, `vkCmdTraceRaysKHR` count, memory).

## References

- Paper: [arXiv:2410.05095](https://arxiv.org/abs/2410.05095)
- Upstream AMBF-Vulkan: https://github.com/AMBF-Vulkan-repositories/AMBF-Vulkan
- Path trace benchmark: [PATHTRACE_ARCH_BENCHMARK.md](PATHTRACE_ARCH_BENCHMARK.md)
