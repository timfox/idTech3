# OpenVDB + Woodcock volumetrics (Beyond ExaBricks alignment)

This path applies ideas from [arXiv:2211.09997](https://arxiv.org/abs/2211.09997) (*Beyond ExaBricks: GPU Volume Path Tracing of AMR Data*) to the **real-time Vulkan** volumetric fog stack using **OpenVDB/NanoVDB**—not full OptiX AMR path tracing.

## What maps from the paper

| Paper concept | Engine implementation |
|---------------|----------------------|
| Spatial majorant grid (§5.1.3) | `r_vdbMajorantBrick` macrocells: max density per brick → `vdbFogMajorant` 3D texture |
| Woodcock / delta tracking (Algorithm 1) | `r_volumetricFogIntegration 3` screen-space path in `volumetric_fog.frag` |
| Transfer-function extinction | `r_vdbFogBlend` scales sampled density/majorant (interactive, no offline kd-tree) |
| Froxel path (fast default) | Mode `0` unchanged: compute pass + froxel march |

Full AMR ExaBrick traversal, ratio tracking, and multi-bounce global illumination are **out of scope** for the game loop; mode `3` is a **single-scatter Woodcock** pass suited to authored `.nvdb` fog volumes.

**CPU decode:** `vdb_load` parses NanoVDB `GridData` (672B layout), walks float/half/double leaf nodes (or FogVolume blind data) into a dense `R32` grid before `vdb_upload`. Supports raw grid buffers and standard `NanoVDB2` file headers.

## Setup

Quick path (demo pack): `exec demo_vdb_woodcock.cfg` — loads shipped `vdb/fog_2cubed.nvdb` and enables mode 3.

```text
set r_volumetricFog 1
set r_vdbFog 1
set r_vdbMajorantBrick 8
vdb_load path/to/volume.nvdb
vdb_upload 0
vdb_bind_fog 0
set r_volumetricFogIntegration 3
```

Or: `volumetric_integration 3`

After changing **`r_vdbMajorantBrick`**, majorant bricks refresh automatically on the next volumetric params update (or run **`vdb_rebuild_majorant <handle>`** manually).

## Cvars

| Cvar | Role |
|------|------|
| `r_vdbMajorantBrick` | Brick size for majorant grid (2–32, default 8) |
| `r_volumetricFogIntegration` | `3` = OpenVDB Woodcock composite |
| `r_vdbFog` / `r_vdbFogBlend` | Blend authored density into extinction |

## Shaders / bindings

- Compute: `vdbFogDensity` (17), `vdbFogMajorant` (18) — majorant empty-space skip in global density
- Composite: `vdbFogDensity` (10), `vdbFogMajorant` (11) — Woodcock segment bounds

## Related

- [docs/VOLUMETRIC_FOG_ENHANCEMENTS.md](VOLUMETRIC_FOG_ENHANCEMENTS.md)
- [AGENTS.md](../AGENTS.md) VDB console commands
