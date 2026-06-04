# WebSplatter — cross-device Gaussian splatting (experimental)

**WebSplatter** (*Cross-Device Efficient Gaussian Splatting in WebGPU*, Feb 2026) targets **WebGPU compute** for splatting on browsers and heterogeneous devices. This engine implements the same **portable architecture** on **Vulkan**: tile bins, bounded workgroups, and push-constant-friendly stages so a future **WGSL** path can share logic with minimal rewrites.

Use it when you want **MGS-style splats** but structured for **WebGPU limits** (not the per-splat 1D dispatch used by Mobile-GS).

## WebGPU alignment (Vulkan path)

| Constraint | Engine choice |
|------------|----------------|
| Workgroup ≤ 256 invocations | Tile draw: **16×16** local size |
| Storage atomics | `wsp_tile_bin.comp` per-tile `atomicAdd` |
| Bounded tile lists | **16** splats / tile, **24** tiles / splat bbox |
| 2D screen dispatch | Tile draw: `dispatch(tileCols, tileRows, 1)` |
| Push constants | All stages &lt; 128 B (WGSL `minUniformBufferOffsetAlignment` friendly) |
| No RTX | Pure compute + composite |

## When to use

| Scenario | Suggested setup |
|----------|-----------------|
| Browser / WASM target (future) | Prototype on Vulkan with `r_wsp`; port shaders to WGSL |
| Android / tile-friendly GPU | `r_wsp 1`, `r_fbo 1` |
| Desktop cross-check vs MGS | `r_wsp 3` vs `r_mgs 3` (only one active: **WSP wins** if both enabled) |

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_wsp` | `0` | Tier: `0`=off, `1`=mobile, `2`=balanced, `3`=high (latched) |
| `r_wsp_strength` | `0.85` | Splat / composite strength |
| `r_wsp_maxSplats` | `0` | Override count (`0` = tier default) |
| `r_wsp_scale` | `0` | Override accum scale (`0` = tier 0.25/0.5/1) |
| `r_wsp_focal` | `512` | Projected radius focal scale |
| `r_wsp_skipSky` | `1` | Skip sky pixels on composite |
| `r_wsp_depthTest` | `1` | Occlude behind scene depth |
| `r_wsp_debug` | `0` | Developer logging |

Requires **`r_fbo 1`**.

### Tier defaults

| Tier | Gaussians | Accum scale | Footprint cap |
|------|-----------|-------------|---------------|
| 1 | 64 | 0.25 | 12px |
| 2 | 256 | 0.5 | 24px |
| 3 | 1024 | 1.0 | 48px |

## Console

- `wsp_status` — tier, tiles, splat count, extent

## Pipeline

1. **`wsp_clear_tiles.comp`** — zero tile counts  
2. **`wsp_prepare.comp`** — 3D Gaussians → screen splats (shared 48 B layout with MGS/GRTX)  
3. **`wsp_tile_bin.comp`** — atomically assign splats to 16×16 tiles  
4. **`wsp_tile_draw.comp`** — per-tile 16×16 workgroup raster into RGBA16F accum  
5. **`wsp_composite.comp`** — blend into HDR `color_image` after world geometry  

## vs Mobile-GS (`r_mgs`)

| | **WebSplatter (`r_wsp`)** | **Mobile-GS (`r_mgs`)** |
|--|---------------------------|------------------------|
| Dispatch | 2D tiles + bin atomics | 1D per-splat footprint |
| WebGPU mapping | Direct | Indirect (similar buffers) |
| Coexistence | Takes precedence if both on | Fallback when WSP off |

## Limitations (v1)

- Procedural Gaussians only; `.wsp` manifest reserved  
- Tile list overflow drops splats silently when &gt; 16 / tile  
- Vulkan only; WGSL port not in-tree  
- No full 3D covariance projection  

## See also

- [MOBILE_GAUSSIAN_SPLATTING.md](MOBILE_GAUSSIAN_SPLATTING.md)  
- [GAUSSIAN_RAY_TRACING_GRTX.md](GAUSSIAN_RAY_TRACING_GRTX.md)  
- [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md) — WebAssembly + WebGPU roadmap  
