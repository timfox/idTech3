# Infinite open worlds (streaming)

Engine-original **infinite walkable world** scaffolding: view-driven **sector residency** with three layers — modular BSP streaming, **per-chunk Detour nav tiles**, and **billboard scatter** — aligned with id Tech 8-style open worlds (engine-original conventions, not proprietary formats).

Works alongside [DISTRICTS.md](DISTRICTS.md) (USD world partition), [WORLD_CONFIG.md](WORLD_CONFIG.md) (named map-state / geometry-set transitions), and [cm_stream](MOD_SDK.md) sector pk3 delivery.

## Architecture

```
View position
    └── WorldOpen_UpdateView
            ├── r_openWorldResidency 0: legacy radius disk (r_openWorldRadius)
            └── r_openWorldResidency 1: WorldResidency (value + budget + bounded delta)
                    ├── cm_stream: collision merge
                    ├── nav/sector_X_Y.nav → Detour addTile
                    └── sprites/sector_X_Y.ents → scatter
```

| Layer | Asset path | Default |
|-------|------------|---------|
| BSP collision | `maps/sector_X_Y.bsp` | Off (`cm_openWorldCollision 0`) — see [Limitations](#limitations) |
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
| `sv_openWorldSync` | `1` | Server publishes loaded sector cells to clients |
| `cl_openWorldSync` | `1` | Client applies server sector list (collision + nav) |
| `r_openWorldResidency` | `0` | Consistent value-aware residency (see [WORLD_RESIDENCY.md](WORLD_RESIDENCY.md)) |
| `sv_openWorldResidency` | `0` | Server-side collision residency planner for MP |
| `cl_openWorldResidencyNavLocal` | `0` | Client nav beyond server list when residency on |
| `r_graphStreamReach` | `0` | k-hop sector graph filter for residency (see [GRAPH_COMPUTE.md](GRAPH_COMPUTE.md)) |
| `r_graphStreamHops` | `8` | Max hops for graph reachability |

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

When **`sv_openWorldSync 1`** (default), the server publishes loaded sector cells via **`CS_ENGINE_OPENWORLD_SECTORS`** (`0_0,1_0,...`). Clients with **`cl_openWorldSync 1`** merge authoritative **collision** for those cells, load **nav tiles** when **`r_openWorldNav 1`**, and **unload** layers dropped from the server list.

With **`sv_openWorldResidency 1`**, the server uses consistent cardinality selection (union over player origins) before publishing. Clients with **`r_openWorldResidency 1`** clamp collision (and nav unless **`cl_openWorldResidencyNavLocal 1`**) to the server list; sprites stay view-driven. Demo: **`exec demo_world_residency.cfg`**. Console: **`world_residency_status`**. See [WORLD_RESIDENCY.md](WORLD_RESIDENCY.md).

### Graph reachability filter

With **`r_graphStreamReach 1`**, residency candidates must lie within **k** hops on the sector grid from player origin(s) (`r_graphStreamHops`, default 8). Optional GPU path: **`r_graphCompute 1`** (client). See [GRAPH_COMPUTE.md](GRAPH_COMPUTE.md).

### Renderer visual overlay

**`r_bspStream 1`** (default) draws streamed sector BSP as a world overlay (parallel to `cm_stream_merge`). When a sector BSP includes **`LUMP_SURFACES`** (e.g. `gen_sector_bsp.py --visual`), planar faces and patches are rendered with authored shaders; otherwise brush-top quads are inferred from collision brushes.

## Modular BSP collision merge (v2)

With **`cm_streamMerge 1`** (set by `openworld_start` and `demo_openworld.cfg`), `CM_Stream_LoadSector` calls **`CM_Stream_MergeSector`**: sector brushes are parsed from `maps/sector_X_Y.bsp`, translated into world space by `cm_streamSectorSize`, and traced as a **collision overlay** on the base CM (no `CM_LoadMap` replace).

**Fixture generator** (CI + local authoring):

```bash
python3 scripts/tools/gen_sector_bsp.py maps/sector_0_0.bsp --cell-x 0 --cell-y 0 --visual
```

Emits a minimal IBSP v46 with one solid platform brush in local sector space. **`--visual`** adds drawVerts/surfaces so **`r_bspStream`** renders authored geometry; without it, the renderer infers brush-top quads. Brush planes use the same side order as **`CM_BoundBrush`** (even side = min, odd = max per axis). `ctest -R test_cm_stream_merge` validates the merge API wiring and BSP lump layout.

Requirements:

1. Load a **base hub map** first (`maps/open_void.bsp` from `scripts/tools/gen_hub_bsp.py`, or your playfield) — provides world tree + default floor.
2. Enable **`cm_openWorldCollision 1`** for view-driven sector merge residency.
3. Author sector BSPs in **local sector space** (0..sectorSize); the engine offsets by `(cellX, cellY) * sectorSize`.

Legacy replace mode: **`cm_streamMerge 0`** still calls `CM_LoadMap` per sector (single-sector collision only).

Traces and `CM_PointContents` query merged sector brushes after the base BSP tree.

## Limitations

- **Hub map required**: Load `maps/open_void.bsp` (or your playfield) before sector merge; generator: `scripts/tools/gen_hub_bsp.py`.
- **Collision default off on client**: Enable `cm_openWorldCollision 1` for view-driven merge (`openworld_start` / `demo_openworld.cfg` do this).
- **MP sync scope**: `CS_ENGINE_OPENWORLD_SECTORS` is driven by **server collision residency**; clients also load **nav** when `r_openWorldNav 1` + `cl_openWorldSync 1`. Scatter/sprites remain view-driven unless loaded locally.
- **Renderer overlay**: `r_bspStream` draws planar faces, Bezier patches (`MST_PATCH` via `R_SubdividePatchToGrid`), or brush-top quads from hunk memory (`r_bspStreamResident 1` logs face counts, stream VBO surfaces, and sector lightmap tiles). Console **`bsp_stream_status`** reports active patches, face/lightmap totals, and LOD. With **`r_bspStreamBake 1`** (default), patch grids are baked to static triangle soup at merge time using `r_lodCurveError` LOD — avoiding per-frame `SF_GRID` tessellation. With **`r_bspStreamVbo 1`** (default, requires **`r_vbo 1`**), merges **incrementally upload** the new sector’s surfaces; unmerge still rebuilds the stream VBO. **`r_bspStreamLod 1|2`** distance-clamps face counts for far sectors. With **`r_bspStreamLightmaps 1`** (default), sector `LUMP_LIGHTMAPS` upload into a dedicated stream lightmap atlas in `tr.lightmaps` (fixed `lightmapCountX × lightmapCountY` tile grid per atlas texture; many sectors can exhaust it). When **`tr.worldDeluxeMapping`** is on, RGB + deluxe pairs upload into paired stream atlases (`tr.deluxemaps`); sector lumps with RGB-only data log and skip deluxe tiles. Unmerging a sector compacts atlas tiles by reloading remaining patches (`R_BspStream_CompactLightmaps`); hub reload via **`RE_BspStream_ClearAll`** resets the atlas entirely. **Brush-top fallback** (collision-only sector BSP) draws a flat **`tr.defaultShader`** quad with no authored lightmap/shader. Stream faces get PBR lightdirs + MikkT (`vk_mikkt_bsp_face_generate`) like hub **`ParseFace`** when **`USE_VK_PBR`** is on. Limits: **64** patches × **16** faces; patch hash (**128** slots) warns when full. `ctest -R test_bsp_stream_vbo` validates VBO/lightmap wiring.
- **Districts**: With `r_openWorld 1`, `district_load_full` streams through **WorldOpen** (collision + nav + scatter). With `r_openWorld 0`, districts use **cm_stream collision only** (legacy).

## Related

- [DISTRICTS.md](DISTRICTS.md) — USD districts + proxy meshes
- [MOD_SDK.md](MOD_SDK.md) — `sv_sectorURL`, replication
- Billboard map props: `r_spriteProps`, `misc_billboard` in [AGENTS.md](../AGENTS.md)

`ctest -R test_openworld` validates wiring and fixtures.

**Runtime validation** (requires built `idtech3_server`):

```bash
ctest -R test_openworld_runtime -V
ctest -R test_sector_stream_fidelity -V
```

`test_openworld_runtime` loads `maps/open_void.bsp`, merges `sector_0_0`, traces platform at `(2048,2048)` expecting hit z ≈ 128.

`test_sector_stream_fidelity` runs **`openworld_smoke_fidelity`** on the dedicated server: multi-sector collision at world offsets, visual BSP lump checks (for **`r_bspStream`**), simulated MP sync list build/unload, 32-cycle load stress, and **`unit_openworld_nav`** Detour walkable probes at sector platform centers.

**Demo pk3** (open-world assets ship in `idtech3_demo.pk3` when built with `--target demo_game_pk3`):

```bash
ctest -R test_demo_openworld_pk3 -V
```
