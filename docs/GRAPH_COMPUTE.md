# Graph compute (Phase 0 sector graph spike)

Boolean frontier reachability on the **open-world sector grid**, with optional **Vulkan compute** BFS. Primary consumer: **streaming** — filters [`WorldResidency`](WORLD_RESIDENCY.md) candidates to cells reachable within **k** hops of player(s).

Inspired by BLEST-style graph-as-linear-algebra; Phase 0 uses CPU BFS + SSBO compute (no WMMA yet).

## Quick start

Graph reachability only affects residency when **both** the value-aware planner and the graph filter are on:

```text
set r_openWorld 1
set r_openWorldResidency 1
set r_graphStreamReach 1
set r_graphStreamHops 8
exec demo_openworld.cfg
openworld_start
graph_bfs_status
```

Debug a cell pair without loading sectors:

```text
graph_reach_test 0 0 3 0 4
```

Optional GPU path (client + Vulkan only):

```text
set r_graphCompute 1
vid_restart
graph_bfs_bench
```

## Prerequisites

| Requirement | Why |
|-------------|-----|
| `r_openWorld 1` | Sector graph window derives from open-world sector size and residency radii |
| `r_openWorldResidency 1` | Graph filter applies inside `WorldResidency` candidate generation |
| `r_graphStreamReach 1` | Enables reachability intersection (no-op when off) |
| `vid_restart` after `r_graphCompute` | Registers Vulkan compute pipeline (`vk_graph_bfs.c`) |

With `r_openWorldResidency 0`, the legacy radius disk ignores the graph bitset even if `r_graphStreamReach 1`.

## Sector graph model

| Concept | Mapping |
|---------|---------|
| Node | Sector cell `(cellX, cellY)` in a bounded window (max 64×64) |
| Edge | 4-neighbor grid (optional 8-neighbor via `r_graphSectorDiagonals`) |
| Source | Player view / MP union of active player origins |
| Query | k-hop reachability bitset |

Window is centered on the view cell and sized from residency hysteresis (`r_openWorldUnloadRadius`).

Reachability is **cached per frame** when the view cell, origin set, hop count, and window parameters are unchanged (skipped when `r_graphCompute` or `r_graphStreamVerify` is on).

## CM_stream vs sector graph window

[`cm_stream.c`](../src/qcommon/cm_stream.c) tracks loaded sectors in a **32×32 bitmap** for non-negative cell indices. The sector graph uses an independent **64×64 logical window** centered on the view and supports negative cell coordinates in BFS. Collision merge limits and graph reachability limits are therefore separate concerns.

| Cvar | Default | Role |
|------|---------|------|
| `r_graphStreamReach` | `0` | Filter WorldResidency candidates by reachability |
| `r_graphStreamHops` | `8` | Max BFS hops |
| `r_graphSectorDiagonals` | `0` | 8-neighbor edges |
| `r_graphBlockUnloaded` | `0` | Omit graph edges into cells not loaded in WorldOpen / `cm_stream` |
| `r_graphCompute` | `0` | Run Vulkan compute BFS in parallel (bench/verify; filtering still uses CPU) |
| `r_graphStreamVerify` | `0` | Log CPU vs GPU bitset mismatches |

Startup when residency enables graph: `[sector_graph] stream_reach hops=… nodes=… edges=… compute=…`

## Multiplayer

Server [`WorldResidency_UpdateServerOrigins`](src/server/sv_openworld.c) passes **all active player origins** as BFS sources before collision planning. Clients use the same filter locally when `r_graphStreamReach 1`; server collision allow-list remains authoritative.

## Console

| Command | Purpose |
|---------|---------|
| `graph_reach_test <sx> <sy> <dx> <dy> [hops]` | Event-to-base reachability debug |
| `graph_bfs_status` | Cvars + current window |
| `graph_bfs_bench` | Toy 5×5 grid timing (client + GPU) |

## Architecture

```
Player origin(s) → SectorGraph_UpdateReachability (CPU BFS)
                 → optional vk_graph_bfs (GPU, r_graphCompute 1)
                 → reachability bitset
                 → WorldResidency candidate filter
                 → WorldOpen load/unload
```

## Troubleshooting

| Symptom | Check |
|---------|-------|
| `graph_bfs_bench` missing or warns pipeline not ready | Client build, `r_graphCompute 1`, then `vid_restart` |
| Reach filter has no effect | `r_openWorldResidency 1` and `r_graphStreamReach 1` both on |
| CPU/GPU mismatch warnings | `r_graphStreamVerify 1` logs per-word diffs; **filtering uses CPU BFS** (`s_reachBits`) — GPU is Phase 0 bench/verify only |
| Cells outside window never reachable | Window capped at 64×64 cells centered on view; increase `r_graphStreamHops` or move view |
| Diagonal shortcuts through void | Keep `r_graphSectorDiagonals 0` (4-neighbor) unless your layout needs 8-neighbor |

## Phase 1 follow-ons (not implemented)

- Nav influence heatmaps from sector reachability
- Dynamic PVS cluster graph
- BLEST WMMA boolean sparse matmul after crossover benchmarks

## Testing

```bash
ctest -R unit_sector_graph -V
ctest -R test_graph_compute -V
```

## Related

- [OPEN_WORLD.md](OPEN_WORLD.md)
- [WORLD_RESIDENCY.md](WORLD_RESIDENCY.md)
