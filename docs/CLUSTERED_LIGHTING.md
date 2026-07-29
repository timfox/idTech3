# Clustered Lighting (Clustered Hybrid M2)

**Status:** Milestone 2  
**Related:** [RENDERER_PATH_OWNERSHIP.md](RENDERER_PATH_OWNERSHIP.md), [UNIFIED_CLUSTERED_RENDERER.md](UNIFIED_CLUSTERED_RENDERER.md)

## Contract

Shared CPU/GPU types in `renderers/vulkan/vk_cluster_contract.h` and `shaders/glsl/cluster_contract.glsl`:

| Struct | Fields |
|--------|--------|
| `gpuClusterHeader_t` | `offset`, `count` (8 bytes) |
| `gpuClusterParams_t` | grid XYZ, tile size, index capacity, zNear/zFar, zScale/zBias, generation, overflow, flags |

`viewDepth` is **positive** forward view-space distance (`abs(view Z)` / clip `w`).

Cluster index order is `xy + z * (clusterCountX * clusterCountY)`. CPU helpers mirror the GLSL helpers:

- `Cluster_IndexFromPixelAndViewDepth` maps a screen pixel plus view depth into one 3D cluster.
- `Cluster_IndexFromTileAndSlice` maps explicit tile/slice coordinates and clamps to the grid.
- `Cluster_LightSliceSpan` maps a light's near/far view-depth bounds to the first/last Z slices it can affect.
- `Cluster_LightOverlapsSlice` is the shared conservative interval test used by binning.

GLSL exposes the same primitive as `fp_light_slice_span` for the compute culler. The culler rejects clusters by slice id first, then keeps the interval overlap test as the conservative guard for boundary cases.

## Log Z slicing

```
slice = clamp(int(log2(viewDepth) * zScale + zBias), 0, Z-1)
```

`zScale` / `zBias` map `[zNear, zFar]` onto `Z` slices without gaps. Linear mode (`r_forwardPlusZSliceMode 0`) is debug-only.

**zFar policy:** `effectiveFar = min(r_clusterZFar, camera_zFar)` with a floor above `zNear`. Default `r_clusterZFar` = **4096**.

Console: `cluster_z_test` prints per-slice near/far/thickness/ratio. Unit: `unit_cluster_math`.

## Compact light lists

When compact is active (`r_clusterCompactLists` auto/1; default on for mode 3 / `zSlices>1`):

Tile SSBO layout:

1. `meta[4]` — atomic index cursor, overflow count, flags, generation mirror  
2. `headers[clusterCount]` — `uvec2(offset, count)`  
3. `indices[indexCapacity]` — packed light indices  

Build (single compute pass, markers `ClusterClear`→`ClusterFill`):

- Per cluster: conservative XY + log-Z overlap, importance/index truncate to `r_clusterMaxLightsPerCluster` (default **32**)
- Light Z membership uses the same near/far span contract as `Cluster_LightSliceSpan`, avoiding Forward+'s single depth interval blind spot when one screen tile contains foreground and distant geometry.
- `atomicAdd` into shared index pool; overflow recorded deterministically

Legacy fallback: fixed **8** slots/cluster (`r_clusterCompactLists 0` or `r_clusterForceBuildFailure 1`).

## Overflow policy (`r_clusterOverflowPolicy`)

| Value | Behavior |
|-------|----------|
| 0 | Diagnostic: empty list + overflow meta (magenta-friendly) |
| 1 | Truncate by stable light index order |
| **2** | Importance retention (production default for mode 3) |

## Consumers

Deferred (`deferred_lighting_common.glsl`), Forward+ opaque/transparent/weapon (`gen_frag.tmpl`) all use `Cluster_FetchLightIndex` from `cluster_light_list.glsl` against the **same** tile SSBO + generation. Sun/directional stays on the separate PBR sun path.

Assert: `vk_cluster_assert_shared_consumers()` logs header/light handles + generation.

## Debug / parity

| Command / cvar | Purpose |
|----------------|---------|
| `cluster_status` | Grid, slices, capacity, utilization, overflow, generation |
| `cluster_inspect` / `r_clusterInspect` | Crosshair cluster header + light indices |
| `r_clusterDebug` / `r_forwardPlusDebug` **6** | Z-slice colors + crosshair slice id |
| `r_hybridCompare` **0–8** | Split / abs RGB / relative luma / diffuse / spec / cluster / membership / shadow |
| `hybrid_compare_status` | Mode + warn/fail thresholds |
| `r_clusterForceBuildFailure` | Force legacy fallback (logged) |
| `r_clusterForceOverflow` | Force overflow path |
| `r_clusterForceStaleGeneration` | Skip generation bump (stale detect) |

## Memory (approx)

At 1920×1080, tile 16, Z=8 → ~120×68×8 ≈ 65K clusters.

| Path | Footprint |
|------|-----------|
| Legacy | `clusters × 8 × 4` ≈ 2.1 MB |
| Compact | `16 + clusters×8 + r_clusterMaxIndices×4` ≈ 0.5 MB headers + 1 MB indices (default 256K) |

Scale roughly with `ceil(W/16)×ceil(H/16)×Z`.

## Demo

```
exec demo_cluster_lab.cfg
```
