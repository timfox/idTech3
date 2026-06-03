# Double-precision point visualization (Vulkan)

Implements the core experiment from [arXiv:2408.09699](https://arxiv.org/abs/2408.09699) (*Double-Precision Floating-Point Data Visualizations Using Vulkan API*): compare **native GPU fp64** (`dvec3` / `dmat4`) against **emulated** Bailey-style high/low `vec3` splits and a **single-precision** baseline.

## Requirements

- Vulkan renderer build (`./scripts/compile_engine.sh vulkan`)
- `r_fbo 1` (normal FBO path)
- **Native mode (0):** GPU `shaderFloat64` + `vid_restart` with `r_fp64Points 1` (enables device feature at init)
- **Emulated (1) / single (2):** work without fp64 hardware; still useful for timing comparisons

## Quick start

```text
set r_fp64Points 1
set r_fp64PointsMode 0
vid_restart
fp64_points_gen 100000 2
```

Modes:

| `r_fp64PointsMode` | Description |
|--------------------|-------------|
| 0 | Native `dvec3` positions, `dmat4` MVP (GL_ARB_gpu_shader_fp64) |
| 1 | Emulated: `highPos + lowPos` per Bailey / DSFUN90-style split |
| 2 | Baseline `vec3` / `mat4` (f32) |

## Commands

| Command | Purpose |
|---------|---------|
| `fp64_points_gen <count> [2\|3]` | Random points in [-1, 1] (paper-style 2D/3D CSV data) |
| `fp64_points_load <path> [2\|3]` | Load comma-separated `x,y` or `x,y,z` from game FS |
| `fp64_points_clear` | Free GPU buffers |
| `fp64_points_benchmark [frames]` | Average ms/frame per mode (in-process draw calls) |

## Cvars

| Cvar | Default | Notes |
|------|---------|-------|
| `r_fp64Points` | 0 | LATCH — enable draw + `shaderFloat64` at `vid_restart` |
| `r_fp64PointsMode` | 0 | 0 / 1 / 2 |
| `r_fp64PointsSize` | 2 | `gl_PointSize` |
| `r_fp64PointsMaxVerts` | 1000000 | Cap for gen/load |

## Implementation

- Shaders: `src/renderers/vulkan/shaders/glsl/fp64_points_*.vert|frag`
- Host: `src/renderers/vulkan/vk_fp64_points.c`
- Drawn in the main scene pass after `RB_DebugGraphics` when `r_fp64Points 1`

External 3D fractal CSV meshes from the paper’s [3d-fractal-generators](https://github.com/NeziheSozen/3d-fractal-generators) repo can be loaded with `fp64_points_load` once placed under your game `base/` path.
