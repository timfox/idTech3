# Dressi (Vulkan differentiable renderer scaffold)

Hardware-agnostic **HardSoftRas** forward path inspired by Takimoto et al., *Dressi* (Eurographics 2022). This engine integration is a **fixed three-stage Vulkan chain** (K-peel raster → blend compute → HDR composite), not the full Dressi-AD JIT / backward optimizer from the paper.

## Quick start

```cfg
seta r_fbo "1"
seta r_dressi "1"
seta r_dressi_demo "1"
vid_restart
dressi_status
```

Or exec `examples/demo_game/mod/demo_dressi.cfg` after `vid_restart`.

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_dressi` | `0` | Master toggle (latched; requires `vid_restart`) |
| `r_dressi_demo` | `0` | Overlay icosphere HardSoftRas demo (latched) |
| `r_dressi_K` | `3` | Depth-peel buffer count (1–5) |
| `r_dressi_r` | `0.01` | Screen-space blur radius (NDC units) |
| `r_dressi_sigma` | `0` | Blending σ; `0` → `r/7` |
| `r_dressi_delta` | `0.01` | Edge mask width |
| `r_dressi_strength` | `0.85` | Composite overlay strength |
| `r_dressi_debug` | `0` | `1` = replace scene with Dressi output |
| `r_dressi_inverseUv` | `0` | Inverse UV compute (scaffold; not wired in demo path) |
| `r_dressi_demoScale` | `8` | Icosphere radius in world units |

## Commands

- `dressi_status` — runtime state (ready, tri count, K, r, σ)

## Pipeline (demo)

1. **HardSoftRas peel** — enlarged triangles, signed distance, `Shift()` depth, K depth peels into a 2D array G-buffer.
2. **Blend** — sigmoid weights + edge mask (paper Eq. 4–6).
3. **Composite** — alpha-blend over HDR color buffer.

## Limitations / future work

- No Dressi-AD computational graph, backward pass, or stage-packing JIT.
- No texture / inverse-UV optimization loop in the demo path.
- Demo mesh is a fixed icosphere in front of the camera, not scene geometry.

## References

- Takimoto et al., *Dressi: A Hardware-Agnostic Differentiable Renderer with Reactive Shader Packing and Soft Rasterization*, Computer Graphics Forum 41(2), 2022.
