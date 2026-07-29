# Graph compute (Phase 0 sector graph + Phase 1 cluster/influence)

Boolean frontier reachability on the **open-world sector grid**, with optional **Vulkan compute** BFS. Primary consumer: **streaming** — filters [`WorldResidency`](WORLD_RESIDENCY.md) candidates to cells reachable within **k** hops of player(s).

Phase 1 adds **nav influence heatmaps** (hop-distance field on sectors), a **BSP cluster portal graph** for dynamic PVS-adjacent reachability, and a **CPU crossover bench** for BLEST/WMMA planning.

Inspired by BLEST-style graph-as-linear-algebra; Phase 0 uses CPU BFS + SSBO compute (no WMMA yet).

## Sector graph model

| Concept | Mapping |
|---------|---------|
| Node | Sector cell `(cellX, cellY)` in a bounded window (max 64×64) |
| Edge | 4-neighbor grid (optional 8-neighbor via `r_graphSectorDiagonals`) |
| Source | Player view / MP union of active player origins |
| Query | k-hop reachability bitset + hop-distance influence |

Window is centered on the view cell and sized from residency hysteresis (`r_openWorldUnloadRadius`).

Reachability is **cached per frame** when the view cell, origin set, hop count, and window parameters are unchanged (skipped when `r_graphCompute` or `r_graphStreamVerify` is on).

### Nav influence (Phase 1)

When `r_graphNavInfluence 1`, scripts and nav middleware can query:

- `SectorGraph_GetHopDistance(cellX, cellY)` — `-1` if unreachable
- `SectorGraph_GetInfluence(cellX, cellY)` — `1.0` at source, linear decay to `0` at `maxHops`

Console: `graph_influence_list` dumps up to 64 reachable cells with hops/influence.

## Cluster portal graph (Phase 1)

Built from **BSP node splits** when a map loads (`ClusterGraph_RebuildFromMap` after vis load). Each undirected edge connects clusters on opposite sides of a node plane (portal adjacency, not full PVS bitset).

| Cvar | Default | Role |
|------|---------|------|
| `r_graphClusterReach` | `0` | Track k-hop cluster reachability from view cluster |
| `r_graphClusterHops` | `4` | Max BFS hops on cluster graph |

Client open-world frame updates the field when `r_graphClusterReach 1` (`CL_OpenWorld_Frame` → `ClusterGraph_UpdateReachability`).

Console: `cluster_graph_status`, `cluster_graph_reach <cluster> [hops]`.

## CM_stream vs sector graph window

[`cm_stream.c`](../engine/core/cm_stream.c) tracks loaded sectors in a **32×32 bitmap** for non-negative cell indices. The sector graph uses an independent **64×64 logical window** centered on the view and supports negative cell coordinates in BFS. Collision merge limits and graph reachability limits are therefore separate concerns.

| Cvar | Default | Role |
|------|---------|------|
| `r_graphStreamReach` | `0` | Filter WorldResidency candidates by reachability |
| `r_graphStreamHops` | `8` | Max BFS hops |
| `r_graphSectorDiagonals` | `0` | 8-neighbor edges |
| `r_graphBlockUnloaded` | `0` | Block edges into unloaded sectors |
| `r_graphCompute` | `0` | Enable Vulkan compute BFS (client) |
| `r_graphStreamVerify` | `0` | Log CPU vs GPU bitset mismatches |
| `r_graphNavInfluence` | `0` | Enable hop-distance influence queries |

Startup when residency enables graph: `[sector_graph] stream_reach hops=… nodes=… edges=… compute=…`

## Multiplayer

Server [`WorldResidency_UpdateServerOrigins`](../runtime/server/world/sv_openworld.c) passes **all active player origins** as BFS sources before collision planning. Clients use the same filter locally when `r_graphStreamReach 1`; server collision allow-list remains authoritative.

## Console

| Command | Purpose |
|---------|---------|
| `graph_reach_test <sx> <sy> <dx> <dy> [hops]` | Event-to-base sector reachability debug |
| `graph_bfs_status` | Sector cvars + current window |
| `graph_influence_list` | Sector hop-distance / influence dump |
| `graph_bfs_crossover` | CPU BFS timing at 5×5 vs 32×32 (WMMA gate) |
| `graph_bfs_bench` | Toy 5×5 grid timing (client + GPU) |
| `cluster_graph_status` | Cluster graph stats |
| `cluster_graph_reach <cluster> [hops]` | Cluster k-hop reachability test |

## Architecture

```
Player origin(s) → SectorGraph_UpdateReachability (CPU BFS)
                 → optional vk_graph_bfs (GPU, r_graphCompute 1)
                 → reachability bitset + hop-distance field
                 → WorldResidency candidate filter
                 → WorldOpen load/unload

Map load → ClusterGraph_RebuildFromMap (portal adjacency CSR)
View cluster → ClusterGraph_UpdateReachability (r_graphClusterReach 1)
```

## Phase 2 (deferred)

- BLEST WMMA boolean sparse matmul frontier on Tensor Cores (after `graph_bfs_crossover` + `graph_bfs_bench` show GPU crossover)
- Nav mesh integration consuming sector influence as coarse AI heatmap weights

## Testing

```bash
ctest -R unit_sector_graph -V
ctest -R unit_cluster_graph -V
ctest -R test_graph_compute -V
```

## Related

- [OPEN_WORLD.md](OPEN_WORLD.md)
- [WORLD_RESIDENCY.md](WORLD_RESIDENCY.md)
