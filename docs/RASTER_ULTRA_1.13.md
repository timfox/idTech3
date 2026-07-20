# Raster Ultra 1.13 — Dynamic Radiance Cache GI + Emissive Transport

Continuation of [RASTER_ULTRA_1.3.md](RASTER_ULTRA_1.3.md). **RT remains locked off.**

**Certification:** **experimental** / quality opt-in. Boot default stays `modern_vulkan.cfg` (stable). Lightmaps remain the baked baseline; this milestone adds a **camera-centered clipmapped radiance cache** for dynamic indirect delta, with confidence-routed SSGI near-field and probe/SH fallbacks.

## Enable

```
exec modern_raster_ultra.cfg
vid_restart
```

Or overlay only:

```
exec vulkan_overlay_raster_ultra_1_13_radiance_cache.cfg
vid_restart
```

Latched: `r_radianceCache`, `r_radianceCacheLevels`, `r_radianceCacheGrid`, `r_radianceCacheCellSize`.

## GI ownership (`INDIRECT_DIFFUSE`)

| Surface class | Static baseline | Dynamic delta | Near-field | Fallback |
|---------------|-----------------|---------------|------------|----------|
| Lightmapped world | Baked lightmap (in scene color) | Radiance cache (preferred) + probe gaps | SSGI confidence lerp | SH/IBL already in forward/base |
| Unlightmapped world | — | Cache / probes | SSGI | SH/IBL |
| Dynamic objects | — | Cache / probes (entity blend) | SSGI when trustworthy | SH / lightgrid |

**Do not** add full lightmap + full probe L0 + full cache + full SSGI. Resolve:

```
probeIndirect = probeIrr * albedo * AO * probeStrength * lightmapDelta
cacheIndirect = cacheIrr * albedo * AO * cacheStrength * lightmapDelta
baseGI        = lerp(probeIndirect, cacheIndirect, cacheConf²)
ssgiIndirect  = ssgiRad * ssgiStrength          // AO not reapplied
finalIndirect = lerp(baseGI, ssgiIndirect, ssgiWeight)
sceneColor   += finalIndirect
```

GTAO modulates **approximate unresolved** probe/cache ambient only. Valid SSGI skips AO.

## Cache architecture

| Topic | Behavior |
|-------|----------|
| Levels | 1–4 camera-centered clipmaps (Ultra default **3**) |
| Grid | `r_radianceCacheGrid`³ per level (default **24**) |
| Cell size | Finest `r_radianceCacheCellSize` (default **96**); coarser ×2 each level |
| Representation | L0 RGB + L1 SH (xyz) + confidence, age, occupancy, variance, lightingRev, leakRisk |
| Scroll | Incremental cell reuse; teleport / large jump invalidates level |
| Geometry | BSP leaf occupancy (solid blocks inject + propagation) |
| Memory | ≈ `levels × grid³ × sizeof(cell)` host + SSBO mirror (~96 B/cell) |

## Injection and propagation

| Source | Policy |
|--------|--------|
| Point/spot dlights | Canonical `refdef` lights; influence × distance; budgeted |
| Sky | Soft outdoor only (`r_radianceCacheSkyScale`) |
| Emissive | Opt-in: material `emissiveAffectsGI` + `rcache_add_emissive`; analytic dlight **owns** GI when both present |
| Propagation | Neighbor exchange, 0–2 iters (`r_radianceCachePropIters`); albedo transport 0.45; energy clamps |
| Decay | Per-update `r_radianceCacheDecay` so lights/emissives/weather do not stick |

## Leak prevention

- Occupancy from BSP solid leaves
- Propagation blocked across occupied neighbors
- Sample-time leakRisk attenuates confidence
- Debug: `r_rasterGiDebug` 8 / 11; `r_radianceCacheDebug` 1–3

**Known residual:** thin non-solid walls / portals may still leak until portal/room classification deepens — partially mitigated in Raster Ultra 2.0 (`RC_BlockedBetween` + stronger leak mute); see [RASTER_ULTRA_2.0.md](RASTER_ULTRA_2.0.md).

## Dynamic behavior

- Moving lights / color changes → inject + decay
- Weather / sun → `lightingRevision` + dirty cells
- Doors/movers → occupancy refresh on budgeted cells; local invalidate (not full rebuild)
- Map load / `rcache_invalidate` → full reset
- Camera teleport → level rebuild

## Quality tiers (`r_radianceCacheQuality`)

| Tier | Behavior |
|------|----------|
| 0 | Path muted |
| 1 | Low — cache sample muted; prefer probes/SH |
| 2 | Medium — reduced update budget |
| 3 | High — Ultra default |
| 4 | Ultra — higher budget + up to 2 prop iters |

## Debug

`r_rasterGiDebug`: 0–8 (1.3) · **9** cache raw · **10** cache contrib · **11** cache conf/level · **12** baseGI

`r_radianceCacheDebug`: 1 level heatmap · 2 confidence · 3 leak

Commands: `rcache_status`, `rcache_invalidate`, `rcache_add_emissive`.

## Pass / resource registry

Pass `raster_gi`. Resources: prior 1.3 plus `radiance_clipmap`, `radiance_cache_irradiance`.

## Validation notes

- GPU soak / 30-min GI soak / fixed-exposure captures: **not measured in this check-in** (headless CI).
- Static gate: `./scripts/raster_ultra_1_13_check.sh`
- Highest-impact next fix: **thin-wall leak** (occupancy-only) and **stale bright inject after light-off** under very low budgets.

## Promotion

| Feature | Class |
|---------|--------|
| Radiance clipmap | **experimental** (quality opt-in via Ultra) |
| Emissive GI flags | **experimental** |
| Boot default | unchanged (`modern_vulkan.cfg`) |
