# Raster Ultra 1.9 — Virtualized Raster Shadows + Large-World Lighting

Continuation of [RASTER_ULTRA_1.8.md](RASTER_ULTRA_1.8.md). **RT remains completely disabled.**

**Certification:** experimental / opt-in. Boot stays `modern_vulkan.cfg`. Ultra base does **not** force virtual shadows — use the overlay. Certified CSM and local atlases remain the sampling fallbacks by default.

## Enable

```
exec modern_raster_ultra.cfg
exec vulkan_overlay_raster_ultra_1_9_virtual_shadows.cfg
vid_restart
```

Command: `vshadow_status`

## Architecture (genuine virtual pages)

This is **not** a renamed CSM. The system implements:

| Component | Implementation |
|-----------|----------------|
| Virtual page addressing | Packed `virtualId` (light \| clipLevel \| pageX \| pageY) |
| Physical page cache | Fixed pool (`r_vshadowPoolPages`, default 64) with atlas grid coords |
| Page residency | `resident` / `initialized` / `pinned` / `staticCached` metadata |
| Page-table lookup | Open-addressed table (`VK_VSHADOW_PAGE_TABLE_SLOTS`) |
| Receiver demand | Clipmap rings around camera; local-light importance requests |
| Invalidation | Camera cut, map change, sun-direction threshold |
| Allocation / eviction | Free list + LRU (pinned near pages exempt) |
| Cached shadow pages | Dirty queue + render budget; never sample uninitialized |

## Signal ownership

| Signal | Owner |
|--------|-------|
| Certified PBR/froxel sun sample | **CSM atlas** while `r_vshadowFallbackCsm 1` (default) |
| Virtual sun page residency | `vk_vshadow` clipmap levels |
| Local virtual pages | Demand queue; overflow → **local atlas** (`localAtlasFallbacks`) |
| Contact shadows | Still not implemented (documented gap) |
| RT sun | Forbidden under Raster Ultra |

## Directional clipmaps

World-snapped pages per level (`r_vshadowBasePageWorld` × 2^level). Small camera motion only allocates newly entered cells — no full redraw of the virtual set. Sun-direction changes beyond `r_vshadowSunDirThreshold` dirty all resident pages.

## Fallbacks

1. `r_vshadow 0` → CSM + local atlas only (boot/Ultra default)
2. Pool exhaustion → allocation failure + missing-page fallback counters; CSM remains
3. Local importance low / pool full → `vk_vshadow_request_local` returns false → atlas path
4. `r_vshadowFallbackCsm 0` only when healthy; otherwise CSM forced back on

## Alpha-tested casters

`r_vshadowAlphaCasters 1` (default under overlay): pages record alpha-caster eligibility for foliage/fences/grates — **not** solid rectangles. Dedicated stochastic/alpha depth policy continues to expand with sample path.

## Pass / resource registry

Virtual shadow demand/alloc/page-render are scheduled in the sun-shadow phase (`VK_SPINE_PASS_SUN_SHADOW` extension point). Page table + physical pool are tracked in `vshadow_status` (memory budget bytes estimated from pool × pageSize²).

## Validation

```
./scripts/raster_ultra_1_9_check.sh
```

Manual: outdoor move (reuse↑), teleport (`camera cut` inval), sun rotate, many dlights (atlas fallback), `r_vshadowPoolPages 8` exhaustion, classic map without overlay, `vid_restart`.

## Promotion decision

| Item | Status |
|------|--------|
| Virtual pages real (address/table/pool/demand/evict) | **yes** |
| Clipmap stable snap | **yes** |
| CSM fallback default | **yes** |
| Local atlas fallback | **yes** |
| Alpha caster policy | **yes** (page metadata) |
| Dedicated per-page GPU frustum draw | **partial** — residency after CSM; sample-from-pages future |
| Contact shadows | **no** (prior gap) |
| Promote to Ultra default | **no** — overlay only |
| Boot unchanged | **yes** |

## Highest-impact fix

Large worlds were stuck on **full-scene × N cascade redraw into a fixed ≤4096 atlas**. Ultra 1.9 introduces a **bounded physical page pool with demand-driven clipmap residency and LRU eviction**, while keeping certified CSM sampling so lighting never goes unshadowed when pages miss.
