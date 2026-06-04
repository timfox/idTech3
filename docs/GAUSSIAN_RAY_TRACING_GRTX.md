# Gaussian Ray Tracing (GRTX, experimental)

**GRTX** is a Vulkan KHR ray-tracing path for **3D Gaussian primitives**, inspired by *Efficient Ray Tracing for 3D Gaussian-Based Rendering* (Jan 2026). v1 uses **streamlined AABB proxy geometry** (one BLAS of box meshes) and closest-hit shading keyed by `gl_PrimitiveID / 12`, aligned with the paper’s idea of hardware-friendly acceleration structures before analytic Gaussian hits.

Long-term goal: a single Vulkan renderer mixing **triangle meshes**, **photogrammetry splats**, **Gaussian impostors**, and **RT effects** in one TLAS.

## When to use

| Scenario | Suggested setup |
|----------|-----------------|
| RTX GPU + FBO | `./scripts/compile_engine.sh vulkan rtx`, then `r_grtx 1` + `vid_restart` |
| Demo overlay | `r_grtxDemo 1`, `r_fbo 1`, load any map (procedural Gaussians rebuild on map load) |
| Blend with raster | `r_grtxComposite 0.35` (HDR color sampled in raygen) |

## Build

- CMake: **`-DUSE_VULKAN_RTX=ON`** (or `./scripts/compile_engine.sh vulkan rtx`)
- Shaders: `./scripts/compile_shaders.sh --apply` regenerates **`vk_grtx_spirv.inc`** from `shaders/glsl/grtx/*`

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_grtx` | `0` | Master toggle (latched). `1`–`3` = hit visualization modes in `grtx_gaussian.rchit` |
| `r_grtxDemo` | `1` | `1` = trace + blit each frame; `0` = no GRTX GPU work |
| `r_grtxMaxGaussians` | `256` | Procedural demo count (max 4096) |
| `r_grtxComposite` | `0.35` | Raster HDR blend into trace (`grtx_trace.rgen`) |
| `r_grtxSamples` | `1` | Primary rays per pixel (1–8) |
| `r_grtxSigmaScale` | `3` | AABB size multiplier from Gaussian scale |
| `r_grtx_debug` | `0` | Developer logging |

Requires **`r_fbo 1`**, depth buffer, and RT-capable GPU (same device path as `r_rtx`).

## Console

- `grtx_status` — active state, Gaussian count, extent

## Pipeline (v1)

1. Map load → procedural `grtxGaussian_t` SSBO + packed box verts/indices → **BLAS** + **TLAS** (single instance).
2. After main (or post-bloom) pass → `grtx_trace.rgen`: depth-based primary rays, trace TLAS, optional multi-sample + raster composite.
3. `grtx_gaussian.rchit`: `primitiveId / 12` → Gaussian index → color/opacity.
4. Blit RT RGBA16F into `color_image`.

## Content (future)

Optional manifest `maps/<map>.grtx` / `grtx/<map>.grtx` for `.ply` / splat assets is reserved; v1 ignores manifests and fills a procedural grid from the world light-grid origin.

## Limitations (v1)

- **Proxy boxes only** — not analytic 3D Gaussian intersection.
- **Separate from `r_rtx` demo** — can run after BSP RTX blit when both enabled.
- No merged TLAS with world BSP or entity meshes yet.
- Vulkan + `USE_VULKAN_RTX` only.

## See also

- [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md) — RTX roadmap
- [RENDERERS.md](RENDERERS.md) — renderer overview
- Triangle RTX demo: `vk_rtx.c`, `rtx_demo.*`
