# Consistent submodular sector residency

Value-aware open-world sector selection under per-layer cardinality budgets, with bounded symmetric-difference updates. Replaces the legacy **radius disk** (`load everything inside r_openWorldRadius`) when `r_openWorldResidency 1`.

Inspired by the consistent submodular optimization framework (Dütting et al., 2026): ROBUST-GREEDY cardinality selection, optional partition-matroid ROBUST-SWAP, and transition spreading so residency sets change gradually.

## Problem mapping

| Paper concept | Engine mapping |
|---------------|----------------|
| Ground set | Sector cells `(cellX, cellY)` in hysteresis annulus |
| Budget `k` | `r_openWorldMaxSectors` (collision, ≤64), `r_openWorldMaxNavSectors`, `r_openWorldMaxSpriteSectors` |
| Value oracle | Distance + proc region bonus + sticky resident bonus |
| Consistency | `r_openWorldResidencyMaxSwaps` per frame + ε-transition windows |
| Matroid | One collision sector per `r_proc` region when `r_openWorldResidencyMatroid 1` |

## Layers

| Layer | Budget cvar | Default | Apply path |
|-------|-------------|---------|------------|
| Collision | `r_openWorldMaxSectors` | 64 | `WorldOpen_LoadSector` / `CM_Stream_*` |
| Nav | `r_openWorldMaxNavSectors` | 32 | Detour tile load via WorldOpen |
| Sprites | `r_openWorldMaxSpriteSectors` | 64 | Scatter `.ents` via WorldOpen |

## Cvars

| Cvar | Default | Role |
|------|---------|------|
| `r_openWorldResidency` | `0` | Client/SP master toggle (legacy disk when off) |
| `sv_openWorldResidency` | `0` | Server MP collision planner (union over player origins) |
| `r_openWorldResidencyEpsilon` | `0.05` | ε — transition fraction + robustness |
| `r_openWorldMaxSectors` | `64` | Collision cardinality (clamped to CM merge limit) |
| `r_openWorldMaxNavSectors` | `32` | Nav tile budget |
| `r_openWorldMaxSpriteSectors` | `64` | Scatter sector budget |
| `r_openWorldResidencyMaxSwaps` | `4` | Max load/unload ops per frame outside transitions |
| `r_openWorldLoadRadius` / `r_openWorldUnloadRadius` | `12288` / `14336` | Hysteresis envelope for **candidate generation** |
| `r_openWorldResidencyMatroid` | `0` | Partition-matroid mode (requires `r_proc 1`) |
| `r_openWorldResidencyW_dist` / `W_proc` / `W_sticky` | `1.0` / `0.25` / `0.35` | Score oracle weights |
| `cl_openWorldResidencyNavLocal` | `0` | Client-only nav beyond server list (default off) |

Startup log when enabled: `[world_residency] enabled epsilon=… k_col=… max_swaps=… matroid=…`

## Quick start

After hub map load and `openworld_start` (see [OPEN_WORLD.md](OPEN_WORLD.md)):

```text
set r_openWorldResidency 1
set r_openWorldLoadRadius 12288
set r_openWorldUnloadRadius 14336
set r_openWorldMaxSectors 64
set r_openWorldMaxNavSectors 32
set r_openWorldMaxSpriteSectors 64
set r_openWorldResidencyMaxSwaps 4

// Optional: k-hop reachability pre-filter
set r_graphStreamReach 1
set r_graphStreamHops 8

openworld_status
```

Demo mod ships commented defaults in `demo_openworld.cfg` (`exec demo_openworld.cfg`). For MP, also set **`sv_openWorldResidency 1`** on the dedicated server.

## Multiplayer rules

- **Server collision is authoritative.** `SV_OpenWorld_Frame` with `sv_openWorldResidency 1` plans collision from the union of active player origins, then publishes via `CS_ENGINE_OPENWORLD_SECTORS`.
- **Clients clamp collision** to the server allow list (`WorldResidency_SetServerCollisionAllowList` from configstring parse).
- **Nav:** when residency is on and `cl_openWorldResidencyNavLocal 0`, nav tiles load only from the server sector list. Sprites remain view-driven locally.

## Graph reachability pre-filter

When **`r_graphStreamReach 1`**, [`WorldResidency`](src/world/world_residency.c) intersects candidates with the sector graph k-hop mask from [`SectorGraph_UpdateReachability`](src/world/sector_graph.c). MP server path uses the union of all active player origins as BFS sources. See [GRAPH_COMPUTE.md](GRAPH_COMPUTE.md).

## District integration

When `WorldDistrict_StreamSectors` runs with `r_openWorldResidency 1`, it sets a district candidate filter (`WorldResidency_SetDistrictFilter`) and calls `WorldResidency_UpdateView` at the district centroid instead of brute-force loading every cell.

## Limitations

- The score function is an **approximate** submodular proxy (distance coverage + bonuses); no formal 1/2 guarantee is claimed in-engine.
- Entity-level replication relevance is out of scope (future ECS layer).
- Renderer `r_bspStream` face residency uses a separate budget path.

## Testing

```bash
ctest -R unit_world_residency -V
ctest -R test_openworld_residency -V
```

## Related

- [OPEN_WORLD.md](OPEN_WORLD.md) — open-world layers and MP sync
- [PROC_PATTERNS.md](PROC_PATTERNS.md) — regionId for matroid mode
- [DISTRICTS.md](DISTRICTS.md) — district streaming
