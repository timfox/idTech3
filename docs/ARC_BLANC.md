# Arc Blanc ocean framework

Real-time ocean simulation per Algis et al. 2025 ([arXiv:2503.03326](https://arxiv.org/abs/2503.03326)): Tessendorf FFT free surface (JONSWAP + Donelan-Banner/swell, three cascades), depth velocity slices, fluid→solid buoyancy/drag, solid→fluid FDM wake mask.

## Build

```bash
./scripts/compile_engine.sh vulkan
# or full profile (enables USE_ARC_BLANC):
cmake -B build -DIDTECH3_PROFILE=full
```

CMake: **`USE_ARC_BLANC`** (default OFF; ON in **`full`** profile).

## Runtime

| Cvar | Default | Description |
|------|---------|-------------|
| `r_arcBlanc` | 0 | Master toggle |
| `r_arcBlancWind` | 20 | Wind speed (m/s) |
| `r_arcBlancFetch` | 1000 | Fetch (m) |
| `r_arcBlancSwell` | 0.5 | Swell 0–1 |
| `r_arcBlancDirectional` | 1 | Directional blend 0–1 |
| `r_arcBlancGrid` | 128 | FFT resolution (power of two, ≤256) |
| `r_arcBlancTile` | 256 | Tile size (world units) |
| `r_arcBlancWindDir` | 0 | Wind direction (degrees) |
| `r_arcBlancSeaLevel` | 0 | Base sea level offset |
| `r_arcBlancDraw` | 1 | Tessellated ocean mesh in world pass |
| `r_arcBlancMeshDiv` | 48 | Subdivisions per tile edge (8–128) |
| `r_arcBlancGpu` | 0 | GPU FFT ocean: 0=CPU, 1=Vulkan compute + CPU readback for physics |
| `r_arcBlancGpuVelocity` | 1 | GPU depth velocity slices when `r_arcBlancGpu` 1 (0=CPU IFFT) |
| `r_arcBlancWake` | 1 | Interactive FDM wake scale added to height sampling |

Console: **`arc_blanc_status`**, **`arc_blanc_reseed`**, **`arc_blanc_sample <x> <z>`**.

### GPU path (`r_arcBlancGpu 1`)

Vulkan compute shaders (`arc_blanc_htilde`, `arc_blanc_fft_1d`, `arc_blanc_extract`, `arc_blanc_combine`, `arc_blanc_velocity`, `arc_blanc_velocity_accum`) update Tessendorf cascades and optional **depth velocity slices** on the GPU (`r_arcBlancGpuVelocity` 1). Combined height/displacement grids are read back for buoyancy and hull coupling. CPU path remains available for parity and headless tests.

CPU path uses **Hermitian paired IFFTs** (Theorem 1) to cut cascade FFT work from five to three 2D transforms per layer. **`h̃₀(-k)`** symmetry is enforced on spectrum seed; buoyancy/drag use **ITTC seawater density** vs depth. Interactive wakes are baked into **`combinedHeight`** each frame (renderer + physics share the same grid).

Requires **`USE_ARC_BLANC=ON`** build and a working Vulkan device. Falls back to CPU FFT if the GPU step fails.

Demo: **`exec demo_arc_blanc.cfg`**.

## API

- `ArcBlanc_SampleHeight(x, z)` — displaced surface height
- `ArcBlanc_SampleVelocity(x, y, z, out)` — depth-interpolated fluid velocity
- `ArcBlanc_RegisterBoxHull(physBody, origin, mins, maxs)` — coupling hull (max 8)

Vulkan uploads combined height to `*arc_blanc_height` via `re.ArcBlancUploadHeightMap` when enabled. The renderer draws a view-snapped tessellated patch via `R_ArcBlanc_AddSurfaces` (shader `arc_blanc_ocean` when present in the mod).

## Lua (`Engine.ArcBlanc`)

```lua
if Engine.ArcBlanc.enabled() then
  local h = Engine.ArcBlanc.sampleHeight(x, z)
  local vx, vy, vz = Engine.ArcBlanc.sampleVelocity(x, h, z)
  local hull = Engine.ArcBlanc.registerHull(physBody, ox, oy, oz, minX, minY, minZ, maxX, maxY, maxZ)
end
```

## Tests

```bash
ctest -R unit_arc_blanc
./tests/scripts/test_arc_blanc.sh
```
