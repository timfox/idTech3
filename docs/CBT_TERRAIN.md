# CBT terrain (GPU LOD + splat)

Experimental Continuous Binary Tree–style terrain path for Vulkan.

## Enable

```
set r_cbtTerrain 1
cbt_load textures/demo/cbt_height.tga
cbt_splat textures/demo/cbt_control.tga textures/demo/dirt.tga textures/demo/rock.tga
cbt_status
```

Or: `exec demo_cbt_splat.cfg`

## Runtime

| Cvar / command | Meaning |
|----------------|---------|
| `r_cbtTerrain` | Master toggle (latched via archive; restart frame path when set) |
| `r_cbtTerrainScale` | World extent (default 256) |
| `r_cbtTerrainGrid` | Patch grid resolution (default 32) |
| `r_cbtTerrainSplat` | Use control/splat map when loaded |
| `cbt_load <heightmap>` | Bind height texture for compute LOD |
| `cbt_splat <control> [l0] [l1] [l2] [l3]` | Control map + optional layer albedos |
| `cbt_status` | Print enable/paths/compute readiness |

Startup log: `CBT terrain tessellation: r_cbtTerrain …`

## Frame path

1. When enabled and a heightmap is loaded, dispatch `cbt_terrain.comp` (indirect draw command buffer + patch counter).
2. CPU tess fallback draws a coarse grid so terrain is visible without a full GPU mesh path.
3. Prefer shader `textures/demo/cbt_splat_ground` (`materialBlend splat` + `splatMap`); falls back to `blend_ground`.

## Authoring splat

- Control texture in terrain UV space: RGBA = weights for layers 0–3 (same semantics as vertex paint).
- Shader keywords: `materialBlend splat`, `splatMap <path>`, plus `layerMap` / PBR maps as needed.
- Vertex RGBA on the CPU grid is a procedural stand-in when no paint sidecar is used.

## Limits

- Compute fills an indirect command buffer; full GPU mesh draw from that buffer is still evolving — CPU grid remains the visible path.
- Up to 4 splat layers on the terrain shader; world PBR blend supports up to 8 via set-19 layer arrays (see [MATERIAL_BLEND.md](MATERIAL_BLEND.md)).

## Tests

`tests/scripts/test_cbt_terrain.sh` (ctest `test_cbt_terrain`).
