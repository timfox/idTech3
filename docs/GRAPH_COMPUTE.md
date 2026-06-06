# Graph compute (Phase 0 sector graph spike)

Boolean frontier reachability on the **open-world sector grid**, with optional **Vulkan compute** BFS. Primary consumer: **streaming** — filters [`WorldResidency`](WORLD_RESIDENCY.md) candidates to cells reachable within **k** hops of player(s).

Inspired by BLEST-style graph-as-linear-algebra; Phase 0 uses CPU BFS + SSBO compute (no WMMA yet).

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
| `r_graphBlockUnloaded` | `0` | Block edges into unloaded sectors |
| `r_graphCompute` | `0` | Enable Vulkan compute BFS (client) |
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
