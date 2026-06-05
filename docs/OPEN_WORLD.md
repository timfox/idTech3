# Infinite open worlds (streaming)

Engine-original **infinite walkable world** scaffolding: view-driven **sector residency** with three layers — modular BSP streaming, **per-chunk Detour nav tiles**, and **billboard scatter** — aligned with id Tech 8-style open worlds (engine-original conventions, not proprietary formats).

Works alongside [DISTRICTS.md](DISTRICTS.md) (USD world partition) and [cm_stream](MOD_SDK.md) sector pk3 delivery.

## Architecture

```
View position
    └── WorldOpen_UpdateView (r_openWorldRadius)
            ├── cm_stream: prefetch sector_X_Y.pk3 / optional CM_LoadMap
            ├── nav/sector_X_Y.nav → Detour addTile (per-chunk walk mesh)
            └── sprites/sector_X_Y.ents → misc_billboard scatter
```

| Layer | Asset path | Default |
|-------|------------|---------|
| BSP collision | `maps/sector_X_Y.bsp` | Off (`cm_openWorldCollision 0`) — see limitations |
| Nav tile | `nav/sector_X_Y.nav` | On (`r_openWorldNav 1`) |
| Billboard scatter | `sprites/sector_X_Y.ents` | On (`r_openWorldSprites 1`) |

Sector grid cell size: `r_openWorldSectorSize` (default **4096** world units).

## Quick start

```text
set r_openWorld 1
set cm_stream 1
openworld_start
openworld_sector 0 0
openworld_status
```

View-driven residency runs each frame when `r_openWorld 1` and a valid client snapshot exists (`CL_OpenWorld_Frame`).

Demo mod: `exec demo_openworld.cfg`.

**Procedural sector typing** (Voronoi, grid, hex, etc.): see [PROC_PATTERNS.md](PROC_PATTERNS.md). With `r_proc 1`, sector loads log region/palette from the active pattern.

## Console

| Command | Purpose |
|---------|---------|
| `openworld_start` | Enable `r_openWorld`, `cm_stream`, create tiled nav mesh |
| `openworld_stop` | Disable and unload sectors |
| `openworld_status` | Cvars + loaded sector table |
| `openworld_list` | List active sectors and layers |
| `openworld_sector <x> <y>` | Force-load nav + scatter (+ collision if enabled) |

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_openWorld` | `0` | Master toggle |
| `r_openWorldSectorSize` | `4096` | World units per sector cell |
| `r_openWorldRadius` | `12288` | Residency radius around view |
| `r_openWorldStream` | `1` | cm_stream prefetch around view |
| `r_openWorldNav` | `1` | Load Detour tiles per sector |
| `r_openWorldSprites` | `1` | Parse scatter entity files per sector |
| `cm_openWorldCollision` | `0` | Merge/load sector BSP collision on residency |
| `cm_streamMerge` | `0` | `1` = overlay merge; `0` = legacy CM_LoadMap replace |
| `cm_streamSectorSize` | `4096` | World translation for local sector BSP brushes |
| `cm_stream` | `0` | Required for sector pk3 prefetch/load |
| `cl_sectorPrefetch` | `1` | Adjacent sector HTTP prefetch |
| `sv_sectorURL` | `` | Base URL for `sector_X_Y.pk3` autodownload |

Startup log: `[world_open] open-world streaming layer initialized`.

## Authoring scatter

Per-sector entity lumps use the same keys as map `misc_billboard` props (see `engine_sprite_map.c`).

**Path resolution** (first match wins):

1. `sprites/sector_<cellX>_<cellY>.ents` — per-cell override
2. `sprites/region_<regionId>.ents` — shared scatter for procedural region (`r_procScatterRegion 1`)
3. `sprites/palette_<paletteIndex>.ents` — palette bucket fallback

Region ids come from [PROC_PATTERNS.md](PROC_PATTERNS.md) (`proc_sample_cell`, `proc_map`).

```text
{
"classname" "misc_billboard"
"origin" "512 512 64"
"shader" "sprites/demo_billboard"
"radius" "48"
}
```

Also supports `misc_flipbook` and `misc_imposter`.

## Nav tiles

Pre-bake Detour navmesh **tiles** per sector (one tile per `r_openWorldSectorSize` cell):

- Path: `nav/sector_<cellX>_<cellY>.nav`
- Loaded via `Nav_LoadSectorTile` into the open-world tiled mesh (`Nav_CreateOpenWorldMesh`)
- Open-world mesh tile size matches `r_openWorldSectorSize` (default 4096)

### In-engine bake (authoring)

From sector BSP collision (`maps/sector_X_Y.bsp` platform brushes):

```text
nav_bake_sector 0 0
nav_bake_view
```

Writes `nav/sector_X_Y.nav` via Recast, then hot-loads into the active open-world mesh.

Prerequisite: sector BSP in the mod pk3 (`scripts/tools/gen_sector_bsp.py`).

Until tiles exist, nav load is skipped with a developer log; scatter and prefetch still work.

### Dedicated server collision

```text
set sv_openWorld 1
set cm_stream 1
set cm_streamMerge 1
set cm_openWorldCollision 1
```

`SV_OpenWorld_Frame` merges sector BSP around each active player's origin (no client nav/sprites).

### Multiplayer sector sync

When **`sv_openWorldSync 1`** (default), the server publishes loaded sector cells via **`CS_ENGINE_OPENWORLD_SECTORS`** (`0_0,1_0,...`). Clients with **`cl_openWorldSync 1`** merge authoritative collision for those cells and **unload** sectors dropped from the server list (bidirectional sync).

### Renderer visual overlay

**`r_bspStream 1`** (default) draws streamed sector BSP as a world overlay (parallel to `cm_stream_merge`). When a sector BSP includes **`LUMP_SURFACES`** (e.g. `gen_sector_bsp.py --visual`), planar faces are rendered with authored shaders; otherwise brush-top quads are inferred from collision brushes.

## Modular BSP collision merge (v2)

With **`cm_streamMerge 1`** (set by `openworld_start` and `demo_openworld.cfg`), `CM_Stream_LoadSector` calls **`CM_Stream_MergeSector`**: sector brushes are parsed from `maps/sector_X_Y.bsp`, translated into world space by `cm_streamSectorSize`, and traced as a **collision overlay** on the base CM (no `CM_LoadMap` replace).

**Fixture generator** (CI + local authoring):

```bash
python3 scripts/tools/gen_sector_bsp.py maps/sector_0_0.bsp --cell-x 0 --cell-y 0 --visual
```

Emits a minimal IBSP v46 with one solid platform brush in local sector space. **`--visual`** adds drawVerts/surfaces so **`r_bspStream`** renders authored geometry; without it, the renderer infers brush-top quads. `ctest -R test_cm_stream_merge` validates the merge API wiring and BSP lump layout.

Requirements:

1. Load a **base hub map** first (`maps/open_void.bsp` or your playfield) — provides world tree + default floor.
2. Enable **`cm_openWorldCollision 1`** for view-driven sector merge residency.
3. Author sector BSPs in **local sector space** (0..sectorSize); the engine offsets by `(cellX, cellY) * sectorSize`.

Legacy replace mode: **`cm_streamMerge 0`** still calls `CM_LoadMap` per sector (single-sector collision only).

Traces and `CM_PointContents` query merged sector brushes after the base BSP tree.

## Related

- [DISTRICTS.md](DISTRICTS.md) — USD districts + proxy meshes
- [MOD_SDK.md](MOD_SDK.md) — `sv_sectorURL`, replication
- Billboard map props: `r_spriteProps`, `misc_billboard` in [AGENTS.md](../AGENTS.md)

`ctest -R test_openworld` validates wiring and fixtures.
