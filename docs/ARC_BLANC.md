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
| `r_arcBlancSpread` | 0 | Extra directional spread shaping; higher values narrow waves around wind direction |
| `r_arcBlancGrid` | 128 | FFT resolution (power of two, ≤256) |
| `r_arcBlancTile` | 256 | Tile size (world units) |
| `r_arcBlancWindDir` | 0 | Wind direction (degrees) |
| `r_arcBlancSeaLevel` | 0 | Base sea level offset |
| `r_arcBlancAmplitude` | 1 | Master amplitude scale affecting vertical and horizontal motion |
| `r_arcBlancHeightScale` | 1 | Additional vertical-only scale |
| `r_arcBlancChoppiness` | 1 | Horizontal displacement scale for sharper crests |
| `r_arcBlancWaveSpeed` | 1 | Simulation playback speed |
| `r_arcBlancGustStrength` | 0 | Time-varying gust amplitude |
| `r_arcBlancGustSpeed` | 0.5 | Gust animation speed |
| `r_arcBlancUpdateHz` | 0 | Fixed simulation update rate. `0` updates every rendered frame |
| `r_arcBlancMaxSubsteps` | 4 | Max fixed ocean steps per rendered frame when `r_arcBlancUpdateHz` is enabled |
| `r_arcBlancDraw` | 1 | Tessellated ocean mesh in world pass |
| `r_arcBlancMeshDiv` | 48 | Subdivisions per tile edge (8–128) |
| `r_arcBlancTileRadius` | 1 | Tiles around the anchor point to render |
| `r_arcBlancFollowCamera` | 1 | Keep the ocean snapped under the camera |
| `r_arcBlancAdaptiveMesh` | 1 | Automatically reduce patch density for outer rings / high camera altitudes |
| `r_arcBlancMeshDivFar` | 20 | Outer-ring mesh divisions when adaptive mesh is enabled |
| `r_arcBlancAdaptiveHeightStart` | 512 | Camera height where adaptive reduction starts |
| `r_arcBlancAdaptiveHeightEnd` | 4096 | Camera height where adaptive reduction reaches full effect |
| `r_arcBlancTileBreak` | 1 | Renderer-side tile breakup blend |
| `r_arcBlancTileBreakOffset` | -500 | Secondary sample offset for tile breakup |
| `r_arcBlancTileBreakBlend` | 0.45 | Tile breakup blend strength |
| `r_arcBlancTileBreakCell` | 768 | Tile breakup noise cell size |
| `r_arcBlancNormalStrength` | 1 | Normal exaggeration for shading |
| `r_arcBlancFoam` | 1 | Crest foam shading toggle |
| `r_arcBlancFoamIntensity` | 0.35 | Foam brightness |
| `r_arcBlancFoamThreshold` | 0.28 | Steepness threshold before foam appears |
| `r_arcBlancFoamSoftness` | 1.5 | Foam spread/softness |
| `r_arcBlancLakeMode` | 0 | Clamp rendering to a rotated finite lake footprint |
| `r_arcBlancLakeCenter` | `0 0 0` | Lake center / world anchor |
| `r_arcBlancLakeExtents` | `1024 1024` | Lake half extents `(x z)` |
| `r_arcBlancLakeAngle` | 0 | Lake rotation in degrees |
| `r_arcBlancGpu` | 0 | GPU FFT ocean: 0=CPU, 1=Vulkan compute + CPU readback for physics |
| `r_arcBlancGpuVelocity` | 1 | GPU depth velocity slices when `r_arcBlancGpu` 1 (0=CPU IFFT) |
| `r_arcBlancWake` | 1 | Interactive FDM wake scale added to height sampling |
| `cl_arcBlancUnderwaterAuto` | 1 | Auto-apply a submerged volumetric look when the camera goes below the surface |
| `cl_arcBlancUnderwater*` | varies | Underwater profile parameters (`DepthBias`, `Density`, `Tint`, `Intensity`, etc.) |

Console: **`arc_blanc_status`**, **`arc_blanc_reseed`**, **`arc_blanc_sample <x> <z>`**, **`arc_blanc_preset <calm|lake|ocean|storm|cinematic>`**.

## Transcript-inspired workflow

- Start with `arc_blanc_preset ocean` or `arc_blanc_preset calm`.
- Use `r_arcBlancWind`, `r_arcBlancFetch`, `r_arcBlancAmplitude`, `r_arcBlancHeightScale`, and `r_arcBlancChoppiness` as the main art-direction dials.
- Use `r_arcBlancDirectional` plus `r_arcBlancSpread` for longer, straighter wind-driven waves.
- Use `r_arcBlancTileBreak*` to reduce obvious repetition on broad water planes.
- Use `r_arcBlancFoam*` to tune crest foam visibility from subtle caps to stormier whitewater.
- Use `r_arcBlancUpdateHz` as the main performance/cinematic tick-rate dial. Lower values reduce cost; higher values help motion blur and close-up shots.
- Leave `r_arcBlancAdaptiveMesh 1` on for gameplay scenes so outer rings and high-altitude cameras automatically get cheaper ocean geometry.
- For non-infinite bodies of water, set `r_arcBlancLakeMode 1` and tune `r_arcBlancLakeCenter`, `r_arcBlancLakeExtents`, and `r_arcBlancLakeAngle`.
- In the Vulkan ImGui overlay, the `Water` panel groups these controls into one workflow surface and includes quick underwater fog looks plus automatic underwater-state tuning.

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
