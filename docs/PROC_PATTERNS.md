# Procedural world patterns

Deterministic **sector typing** for open worlds: Voronoi cells, rectangular grids, checkerboards, hex tiling, radial rings, horizontal/vertical stripes, and hash noise. Patterns are seeded and scaled in world units so the same coordinates always yield the same region id and palette index.

Used with [OPEN_WORLD.md](OPEN_WORLD.md) sector streaming — when a sector loads, the engine logs the sampled region under `r_proc 1`.

## Patterns

| Name | `r_procPattern` | Description |
|------|-----------------|-------------|
| Grid | `grid` | Rectangular blocks of `r_procGridW` × `r_procGridH` sector cells |
| Checker | `checker` | Alternating cells by `(cellX + cellY) & 1` |
| Voronoi | `voronoi` | Jittered site diagram; spacing ≈ `r_procScale` |
| Hex | `hex` | Axial hex grid at `r_procScale` spacing |
| Radial | `radial` | Concentric rings from world origin |
| Stripes H | `stripe_h` | Horizontal bands of width `r_procScale` |
| Stripes V | `stripe_v` | Vertical bands of width `r_procScale` |
| Noise | `noise` | 128-unit hash noise cells |

Alias: `veroni` → `voronoi`, `check` → `checker`, `hexagon` → `hex`, `rings` → `radial`.

## Cvars

| Cvar | Default | Purpose |
|------|---------|---------|
| `r_proc` | `1` | Master toggle |
| `r_procPattern` | `voronoi` | Active pattern |
| `r_procSeed` | `1` | Deterministic seed |
| `r_procScale` | `4096` | Feature scale (Voronoi site spacing, stripe width, hex size) |
| `r_procGridW` | `4` | Grid pattern width in sector cells |
| `r_procGridH` | `4` | Grid pattern height in sector cells |
| `r_procPalette` | `8` | Palette size for region indices (2–64) |
| `r_procScatterRegion` | `1` | Fall back to `sprites/region_<id>.ents` / `sprites/palette_<n>.ents` |

Align `r_procScale` with `r_openWorldSectorSize` when sampling per-sector cells.

With **`r_openWorldResidencyMatroid 1`** (requires **`r_proc 1`**), the residency planner enforces at most one collision sector per procedural region id — useful for sparse region scatter without loading every cell in a Voronoi cell. See [WORLD_RESIDENCY.md](WORLD_RESIDENCY.md).

## Console

```text
proc_list
proc_pattern voronoi
proc_seed 42
proc_scale 4096
proc_grid 4 4
proc_sample [x y]
proc_sample_cell <cellX> <cellY>
proc_map <centerCellX> <centerCellY> [radius]
```

`proc_map` prints an ASCII region-id map (last digit) around a center cell — useful for debugging Voronoi and grid layouts.

## Quick start

```text
exec demo_proc.cfg
proc_map 0 0 4
```

With open-world streaming:

```text
exec demo_openworld.cfg
set r_procPattern hex
openworld_start
```

Sector loads log `[world_proc] sector X,Y -> region N palette P` when `r_proc 1`.

## API (C)

```c
#include "world_proc.h"

WorldProc_Init();  // registers cvars (also called from CL_Proc_Init)
worldProcSample_t s = WorldProc_SampleWorld( worldX, worldY );
int region = WorldProc_RegionAtSector( cellX, cellY, sectorSize );
```

`WorldProc_SampleWorld` returns `regionId`, `paletteIndex`, and optional `voronoiDist` / `noiseValue` depending on pattern.

## Scatter asset binding

Open-world billboard scatter resolves paths in order (see [OPEN_WORLD.md](OPEN_WORLD.md)):

1. `sprites/sector_X_Y.ents`
2. `sprites/region_<regionId>.ents` when `r_procScatterRegion 1`
3. `sprites/palette_<paletteIndex>.ents`

Use `proc_sample_cell` to discover region ids for shared region scatter files.
