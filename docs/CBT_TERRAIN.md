# CBT terrain (GPU LOD + splat)

Experimental Continuous Binary Tree–style / **tiled heightfield** terrain path for Vulkan.
Primary outdoor representation for [Raster Ultra 1.14](RASTER_ULTRA_1.14.md).

## Enable

```
set r_cbtTerrain 1
cbt_load textures/demo/cbt_height.tga
cbt_splat textures/demo/cbt_control.tga textures/demo/dirt.tga textures/demo/rock.tga
terrain_status
```

Or Ultra overlay: `exec vulkan_overlay_raster_ultra_1_14_terrain.cfg` then `cbt_load` / `cbt_splat`.

Or: `exec demo_cbt_splat.cfg`

## Runtime

| Cvar / command | Meaning |
|----------------|---------|
| `r_cbtTerrain` | Master toggle (archive; off by default — does not replace BSP) |
| `r_cbtTerrainScale` | World extent (default 256) |
| `r_cbtTerrainGrid` | Patch grid resolution (default 32) |
| `r_cbtTerrainSplat` | Use control/splat map when loaded |
| `r_cbtTerrainLodHysteresis` | Stable LOD transitions (default 1) |
| `r_cbtTerrainQuality` | LOD quality tier 0–4 |
| `r_cbtTerrainDeform` | Sparse height deformation hooks |
| `r_cbtTerrainDebug` | LOD / error / chunk / stitch debug |
| `cbt_load <heightmap>` | Bind height texture + CPU samples for LOD/normals |
| `cbt_splat <control> [l0] [l1] [l2] [l3]` | Control map + optional layer albedos |
| `cbt_status` / `terrain_status` | Enable/paths/LOD/residency |

Startup log: `CBT terrain (Raster Ultra 1.14): r_cbtTerrain …`

## Frame path

1. Inactive without heightmap metadata — classic BSP maps stay unaffected.
2. Screen-space chunk LOD (8×8) with hysteresis + edge stitch flags.
3. Dispatch `cbt_terrain.comp` when compute pipeline exists.
4. CPU heightfield tess draws heightmap-sampled geometry (shared normals).
5. Prefer shader `textures/demo/cbt_splat_ground` (`materialBlend splat`); falls back to `blend_ground`.

## Authoring splat

- Control texture in terrain UV space: RGBA = weights for layers 0–3 (same semantics as vertex paint).
- Shader keywords: `materialBlend splat`, `splatMap <path>`, plus `layerMap` / PBR maps as needed.

## Limits

- Compute fills an indirect command buffer; full GPU mesh draw from that buffer is still evolving — CPU heightfield remains the visible path.
- Up to 4 splat layers on the terrain shader; world PBR blend supports up to 8 via set-19 layer arrays (see [MATERIAL_BLEND.md](MATERIAL_BLEND.md)).
- Does not replace classic BSP ownership or clear lightmaps/PVS.

## Tests

`tests/scripts/test_cbt_terrain.sh` (ctest `test_cbt_terrain`); `scripts/raster_ultra_1_14_check.sh`.
