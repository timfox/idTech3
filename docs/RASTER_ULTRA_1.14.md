# Raster Ultra 1.14 — Terrain + Vegetation + Biomes + Natural Microgeometry

Continuation of [RASTER_ULTRA_1.13.md](RASTER_ULTRA_1.13.md). **RT remains locked off.** Temporal reconstruction is not required for basic outdoor correctness (SMAA stays the certified AA path). Does NOT force TAA.

**Certification (this ship):** scaffolding + world routing + heightfield LOD core — see Promotion decision below. Boot default stays `modern_vulkan.cfg`. Ultra base profile does **not** enable terrain/vegetation; use the overlay.

## Enable

```
exec modern_raster_ultra.cfg
exec vulkan_overlay_raster_ultra_1_14_terrain.cfg
vid_restart
cbt_load textures/demo/cbt_height.tga
cbt_splat textures/demo/cbt_control.tga
terrain_status
biome_status
veg_status
gpu_scene_status
```

Recovery: `exec modern_vulkan.cfg; vid_restart`

## World ownership

| Value (`r_gpuSceneWorldType`) | Name | Effective when |
|-------------------------------|------|----------------|
| 0 | `WORLD_CLASSIC_BSP` | Always (default) |
| 1 | `WORLD_TERRAIN` | Terrain heightmap metadata present |
| 2 | `WORLD_STREAMED` | Open-world / stream metadata |
| 3 | `WORLD_HYBRID` | Terrain and/or stream metadata |

Absent metadata → **classic BSP** with a logged fallback reason. Terrain never clears BSP ownership, PVS, or lightmaps. Without `cbt_load`, terrain/veg systems stay idle (no black classic maps).

Commands: `gpu_scene_status`, `terrain_status` / `cbt_status`, `biome_status`, `veg_status`

## Terrain representation (chosen primary)

**Tiled heightfield mesh** (CBT path in `vk_terrain.c`) — single architecture for 1.14:

| Capability | Support |
|------------|---------|
| Large coordinates / origin bias | Yes (`CBTerrain_OnOriginRebase`) |
| Screen-space LOD + hysteresis | Yes (8×8 chunks, step ≤1 per frame) |
| Edge stitch flags | Yes (neighbor finer-LOD mask) |
| Height-derived normals | Shared sampler (consistent across LOD) |
| Material splat layers | `cbt_splat` + `materialBlend` |
| Collision correspondence | Height sample API (hooks) |
| Deformation | Sparse delta field (`r_cbtTerrainDeform`) |
| Streaming residency | Chunk resident/fallback flags |
| GPU compute LOD cmds | Evolving; CPU heightfield draw is visible path |

**Not shipping as competitors:** clipmaps / nested rings as alternate primaries.

### Limits

- Full GPU mesh draw from CBT indirect buffer still evolving
- Triplanar / anti-tiling / POM are material-path follow-ons (documented; not forced on)
- Virtual textures optional — tiled textures remain valid

## Biomes

Deterministic seed (`r_biomeSeed`). Inputs: elevation, slope, moisture noise, season (`r_biomeSeason`), wetness (`r_biomeWetness`). Types: soil, grass, rock, sand, mud, snow, wetland, forest, desert, ash. Blended normalized weights; cached defs; no per-launch reshuffle.

## Vegetation

| Topic | Policy |
|-------|--------|
| Generation | Host→instance buffer, deterministic hash/Poisson-style jitter (GPU-ready records) |
| No CPU per-blade ents | Instances stored once; batched draw budget |
| Cull | Frustum + distance LOD + impostor flags |
| Wind | World-space deterministic; prev transforms for motion |
| Interaction | Bounded field, not per-instance CPU updates |
| Alpha | Coverage policy (`r_vegGpuAlphaCoverage`); stable without TAA |
| Shadows | Distance shadow LOD / canopy aggregate flag |
| Species | grass…trees, rocks, debris |

## Streaming / fallbacks

Missing height CPU samples → procedural heights + warning (not black). Missing regions use coarse fallback residency. Classic maps with terrain cvars off are unaffected.

## Pass / resource registry

Spine resources: terrain height/LOD/layers, biome map, veg instance/visible/indirect, wind + interaction fields, deform, residency. Passes: terrain LOD/cull/draw, biome eval, veg generate/cull/draw/wind/interaction, deform, residency.

## Quality tiers (cvars)

| Tier | `r_cbtTerrainQuality` | Veg density |
|------|----------------------|-------------|
| Low | 0 | sparse |
| Medium | 1–2 | moderate |
| High | 3 | dense |
| Ultra / Reference | 4 | max + deterministic |

## Validation (static / wiring)

- `scripts/raster_ultra_1_14_check.sh`
- `tests/scripts/test_cbt_terrain.sh` (extended expectations)
- Runtime soak / measured GPU costs: **not invented** — report when available from profiling sessions

## Promotion decision

| Feature | Status |
|---------|--------|
| Terrain LOD core | **quality opt-in** |
| Biome layering | **experimental** |
| Grass GPU instances | **experimental** |
| Trees | **experimental** (proxy cards) |
| Wind | **quality opt-in** (deterministic scaffold) |
| Interaction | **experimental** |
| Deformation | **experimental** (hooks) |
| Streaming | **experimental** |
| Classic BSP safety | **Raster Ultra certified** (default path unchanged) |

## Highest-impact next fixes

1. Crack-free edge geomorphing on stitch boundaries  
2. Dedicated foliage BRDF + coverage-preserving mips in material path  
3. GPU compute placement (move generation off host)  
4. Shadow cascade LOD parity for canopy

## RT lock

Overlay forces `r_rtx 0`, `r_hybrid1 0`, `r_pathtrace 0`, and related RT GI off. No BLAS/TLAS/ray-query dependency in terrain/biome/veg modules.
