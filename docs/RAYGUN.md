# Raygun (Hirsch & Thoman, arXiv:2001.09792)

Vulkan KHR ray tracing demo inspired by the open-source [Raygun](https://github.com/W4RH4WK/Raygun) engine paper: primary visibility rays with **shadow**, **reflection**, and **refraction** secondary traces over the shared world TLAS, plus optional **FXAA** compute (paper §2.1).

## Enable (RTX build)

```bash
./scripts/compile_engine.sh vulkan rtx   # USE_VULKAN_RTX=ON
```

In-game (latched — **`vid_restart`** after toggling):

```
r_fbo 1
r_raygun 1
r_rtxDemo 1          // world BSP TLAS (required)
vid_restart
```

Optional (exec `demo_raygun.cfg` in the demo mod):

```
r_raygun_reflection 1
r_raygun_refraction 1
r_raygun_shadow 1
r_raygun_fxaa 1
r_raygun_ior 1.45
```

`r_rtx 1` is **not** required when only `r_raygun 1` is set (device RT extensions enable from `r_raygun` alone).

## Pipeline

1. **Primary trace** — depth-reconstructed camera rays (`raygun.rgen`), same invViewProj / render-target extent as `r_rtx` demo.
2. **Closest hit** — material from `gl_PrimitiveIDEXT & 3`:
   - 0: matte gray
   - 1: red diffuse
   - 2: mirror (reflection trace)
   - 3: glass (refraction trace)
3. **Shadow trace** — separate hit group; sun direction fixed `(0.35, 0.75, 0.55)`.
4. **FXAA** (optional) — edge-aware 4-neighbor blur (`raygun_fxaa.comp`).
5. **Composite** — blit into FBO `color_image` (blend via `r_raygun_composite`).

When `r_raygun 1`, the legacy `r_rtx` demo pass is **skipped** (Raygun owns RT output). Hybrid1 takes priority if both are enabled.

## Cvars

| Cvar | Default | Role |
|------|---------|------|
| `r_raygun` | 0 | Master toggle (latched) |
| `r_raygun_fxaa` | 1 | FXAA compute after trace |
| `r_raygun_reflection` | 1 | Mirror material reflection traces |
| `r_raygun_refraction` | 1 | Glass material refraction traces |
| `r_raygun_shadow` | 1 | Sun shadow rays |
| `r_raygun_ior` | 1.45 | Glass index of refraction |
| `r_raygun_composite` | 1 | Blit weight to scene color (1=full replace) |
| `r_raygun_samples` | 1 | Primary rays per pixel (1–4) |

## Console

| Command | Role |
|---------|------|
| `raygun_status` | Print active state and cvar values |

## Files

- Host: `vk_raygun.c`, `vk_raygun.h`
- Shaders: `shaders/glsl/raygun/*`
- SPIR-V embed: `vk_raygun_spirv.inc` (from `scripts/compile_shaders.sh`)

## Reference

Alexander Hirsch, Peter Thoman — *Running on Raygun*, arXiv:2001.09792, 2020.
