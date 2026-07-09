# CuRast — CUDA/Vulkan software rasterization (experimental)

**CuRast** (*Cuda-Based Software Rasterization for Billions of Triangles*, Schütz, Lipp, Kristmann & Wimmer, [arXiv:2604.21749](https://arxiv.org/abs/2604.21749)) is a **3-stage software rasterizer** for dense opaque meshes using **atomicMin visibility buffers** (28-bit depth + 36-bit global triangle index in the paper).

The upstream implementation is **CUDA** ([m-schuetz/CuRast](https://github.com/m-schuetz/CuRast)). This engine ships a **Vulkan compute scaffold** plus an **analytical benchmark model** from paper Tables 2–3.

## Paper vs engine

| CuRast (paper) | Engine scaffold |
|----------------|-----------------|
| Stage 1: 1 thread/triangle, bbox raster (<128 px) | `curast_stage1.comp` |
| Stage 2: 32 threads/triangle (medium, <4096 px) | Not wired (v1) |
| Stage 3: 64 threads / 64×64 tile (large tris) | Not wired (v1) |
| 64-bit atomicMin visibility buffer | 32-bit packed R32_UINT (18-bit depth + 16-bit tri id) |
| Resolve + world-space mip estimation | `curast_resolve.comp` (flat false-color) |
| Billions of triangles, Zorah dataset | Procedural micro-triangle grid (up to 65536 tris) |
| Table 2 CUDA vs Vulkan timings | `curast_model` |

## Build & enable

Standard Vulkan build:

```bash
./scripts/compile_engine.sh vulkan
```

In-game:

```
r_curast 1
vid_restart
curast_render 4
curast_status
```

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_curast` | `0` | Master toggle (latched; `vid_restart`) |
| `r_curast_tris` | `8192` | Procedural triangle count (64–65536) |
| `r_curast_width` | `640` | Framebuffer width |
| `r_curast_height` | `480` | Framebuffer height |
| `r_curast_debug` | `0` | Developer logging |
| `cl_curast_model` | `1` | Analytical benchmark commands |

## Console — model (always on)

```
curast_model_status
curast_api
curast_model [zorah|sponza|lantern|lantern_inst|komainu|venice] [4090|4070|5090]
curast_stages [scene]
```

## Console — raster (renderer)

```
curast_status
curast_render [N]
curast_partition
curast_reset
```

## Pipeline (engine v1)

1. **Clear** — visibility buffer to `0xFFFFFFFF`
2. **Stage 1** — one thread per triangle, bbox raster, `imageAtomicMin`
3. **Resolve** — decode triangle id, write RGBA8 color buffer

Runs on a one-shot command buffer; output is not composited into the main frame yet.

`curast_partition` performs a paper-style routing analysis on the current procedural mesh and reports how many triangles would go to:

- **Stage 1**: small triangles (`<128` pixel bbox area)
- **Stage 2**: medium triangles (`<4096` pixel bbox area)
- **Stage 3**: large triangles
- **nearPlane**: triangles that would require special handling before screen-space stage routing

This does not execute stages 2 and 3 yet, but it makes the paper’s 3-stage split visible in the runtime scaffold instead of treating every triangle as stage 1 by default.

## Benchmark model

Examples from Table 2 (RTX 4090):

- **Zorah closeup** (13.6B vis tris): CuRast **74.9 ms** vs VK-PIP **1778 ms** (~23.7×)
- **Sponza closeup**: CuRast **0.27 ms** vs VK-ID **0.04 ms** (Vulkan wins on low-poly scenes)
- **Lantern instanced** (3.1B tris, 5090): CuRast **9.95 ms** vs VK-ID **125.7 ms** (~12.6×)

`curast_stages zorah` prints Table 3 stage breakdown (Stage1 dominates on dense data).

## Limitations (engine v1)

- No stage 2/3 queues, instancing, compression, or JPEG textures
- No integration with map draw / photogrammetry loaders
- Visibility packing is 32-bit (demo scale), not paper 64-bit
- Transparency and multi-mesh scenes not supported

## See also

- [VKSPLAT.md](VKSPLAT.md) — Vulkan compute 3DGS training
- [MOBILE_GAUSSIAN_SPLATTING.md](MOBILE_GAUSSIAN_SPLATTING.md) — splat inference
- [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md)

## References

- Schütz et al., arXiv:2604.21749 — CuRast
- Karis et al., SIGGRAPH 2021 — Nanite visibility buffers
- Liu et al., I3D 2010 — FreePipe atomicMin rasterization
