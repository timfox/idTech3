# High-Throughput Raster Engine 1.0

**Candidate profile:** `config/modern_high_throughput.cfg` — **NOT the boot default.**  
**Recovery:** `exec modern_vulkan.cfg; vid_restart`  
**Preserves:** `modern_vulkan.cfg`, mode 2 Forward+ fallback, mode 3 deferred opaque, Forward+ transparent/weapon, classic BSP, Q3 shaders, QVM, OpenGL fallback, pass registry, signal ownership.

Ray tracing remains optional (locked off under this candidate).

## Slice status

| Slice | Scope | Status |
|-------|--------|--------|
| **A — GPU throughput** | Global indices, GPU scene, cull, light/decal binning, indirect, merge, profiling | **shipped (opt-in)** |
| B — Animated data | Compact tangents, skeletal compression, geometry cache | **superseded by HT 1.1** — see `docs/HIGH_THROUGHPUT_RASTER_1.1.md` |
| C — Surface interaction | Material compositing, geometry-aware decals, damage | not started |
| D — Water and effects | Dedicated water, dense GPU particles | not started |
| E — Certification | Budgets, soak, promotion | not started |

## Slice A — what shipped

### Global resource indices (`vk_ht_throughput`)

- Index **0 = INVALID** (never white/default).
- Reserved fallbacks: white, black, flat normal, rough, material, geometry, anim (`1..7`).
- User indices from `64`. Alloc/free/resolve with validation (`r_htResValidate`).
- Counters: capacity, alive, high-water, invalid lookups, fallback uses.

### Persistent GPU scene (existing + merge)

- `vk_gpu_scene`: instance/mesh records, frustum + Hi-Z companion cull, host indirect pack.
- **Compatible draw merge:** `vk_gpu_scene_merge_compatible_draws` coalesces same index-range draws into multi-instance cmds (`r_htMergeDraws`).
- Classic BSP world type remains default; BSP not GPU-scene driven.
- Meshlet MDI path remains the production `vkCmdDrawIndexedIndirect` consumer (`r_meshletsMdiDraw`).

### Light / decal binning

- **Lights:** Forward+ tile/cluster compute (unchanged production owner).
- **Decals:** host bin onto the **same** Forward+ tile grid (`r_htDecalBin`), max 8/cluster, overflow keeps earliest entries and sets a diagnostic flag (no OOB reads).

### Profiling

Commands: `ht_status`, `ht_decal_status`, `ht_res_status`, plus `gpu_scene_status`, `meshlet_status`, `hiz_status`.

## Enable

```
exec modern_high_throughput.cfg
vid_restart
```

## Explicit non-goals this slice

- Full raster bindless descriptor arrays (RTX bindless remains separate)
- GPU compute instance cull (still host frustum + Hi-Z policy)
- Hi-Z depth sample reject (pyramid + conservative bias only)
- Geometry-cache / skeletal compression (Slice B)
- Material compositing / damage / water (Slices C–D)
- Invented 60 Hz measurements (Slice E)

## Promotion decision (Slice A)

| Subsystem | Class |
|-----------|--------|
| Forward+ light binning | parity-adjacent (already shipping) |
| GPU scene cull + indirect pack | quality opt-in |
| Compatible draw merge | quality opt-in |
| Global resource indices | quality opt-in |
| Decal binning (host/cluster) | quality opt-in |
| Meshlet MDI | quality opt-in (1.6) |
| Hi-Z sample reject | experimental (resource only) |
| Boot `modern_vulkan.cfg` | unchanged certified fallback |

## Highest-impact next failure to fix

Wire registered GPU-scene meshes with non-zero index ranges into a real `vkCmdDrawIndexedIndirect` consumer (or expand meshlet registration), then measure CPU submission delta — before Slice B.
