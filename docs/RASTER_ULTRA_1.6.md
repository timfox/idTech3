# Raster Ultra 1.6 — GPU-Driven Geometry + Meshlets + LOD + Streaming

Continuation of [RASTER_ULTRA_1.5.md](RASTER_ULTRA_1.5.md). **RT remains locked off.**

**Certification:** experimental / geometry opt-in. Boot default stays `modern_vulkan.cfg` (mode 2 + SMAA). Ultra profile does **not** force GPU scene — use the overlay.

## Enable

```
exec modern_raster_ultra.cfg
exec vulkan_overlay_raster_ultra_1_6_geometry.cfg
vid_restart
```

Commands: `gpu_scene_status`, `hiz_status`, `meshlet_status`

## GPU scene architecture

Persistent records (`vk_gpu_scene.*`) with **stable handles** (no CPU pointers on GPU):

| Record | Contents |
|--------|----------|
| Instance | transform, prev transform, bounds, sphere, LOD, flags, stream state, visible age, object/material IDs |
| Mesh | material, meshlet range, index range, bounds, generation |
| Draw cmd | `VkDrawIndexedIndirectCommand` layout |

**World ownership** (`r_gpuSceneWorldType`):

| Value | Path |
|-------|------|
| 0 | **classic BSP** (default) — GPU scene only augments registered instances |
| 1 | **terrain** (CBT heightfield; requires terrain metadata) — see [RASTER_ULTRA_1.14.md](RASTER_ULTRA_1.14.md) |
| 2 | streamed open-world / sector stream |
| 3 | hybrid (BSP + terrain/stream augment) |

Absent streaming/terrain metadata always routes as classic BSP — **classic maps never go black**.

## Culling

1. **Frustum** — bounding sphere vs view frustum  
2. **Hi-Z companion** (`r_hiZ`) — depth pyramid resource + conservative policy  
3. **Meshlet** — frustum + **normal cone** + screen LOD  

Anti one-frame disappearance:

- `r_hiZMinVisibleFrames` (default 2)
- visible-last-frame bias
- large-object keep (`r_hiZLargeObjectPx`)
- camera-cut → all visible
- **occlusion query readback failure keeps entities visible** (was zeroing → hide-all)

> Note: `r_forwardPlusHiZ` is **tile probe padding**, not this pyramid.

## Indirect submission

Host-packed indirect commands into a persistent buffer (no CPU readback of GPU cull results required for the host path). `drawIndirectCount` remains optional when device support is present; meshlets already use `vkCmdDrawIndexedIndirect`.

## Meshlet generation

Existing bake-at-load (`docs/MESHLETS.md`) enhanced for 1.6:

- **Stable uint64 cache keys** + generation (no unsafe pointer-backed GPU records)
- **Normal cone** metadata (hard-edge aware cutoff; does not merge incompatible partitions)
- Cone cull in `R_Meshlets_CullViewFrustumXform`
- Animated MD3 still skips meshlets (bind-pose AABB unsafe)

## LOD / HLOD

| Layer | Status |
|-------|--------|
| Meshlet screen LOD | `r_meshletsLod` + hysteresis-friendly thresholds |
| Instance LOD flags | `lodLevel` / `lodHysteresis` on GPU instances |
| HLOD aggregates | `r_gpuSceneHlod` — static distant proxies only (no dynamic/gameplay merge) |
| Classic MD3 LOD | unchanged (`r_lodbias`) |

## Streaming

Ownership types above. Mesh/texture residency fields on instances (`streamState`). Sector/open-world streaming (`r_openWorld`, `r_bspStream`, VT) remains the production stream path; GPU scene **augments** it. Classic BSP stays resident under type 0.

## Classic compatibility

Preserved: BSP submission, Q3 shaders, lightmaps, authored normals, hard-edge splits, MikkTSpace, mode 2 SMAA, mode 3 Ultra lighting, portals/mirrors (view cuts reset Hi-Z), weapon views, QVM.

## Measured performance

Use `gpu_scene_status` / `meshlet_status` / `hiz_status` for:

- instance / visible / indirect counts
- frustum / Hi-Z / LOD / stream rejects
- meshlet bake/cache/cone/LOD/MDI stats

Full CPU submission delta vs baseline requires GPU soak (not claimed as numeric cert here).

## Validation

Static: `./scripts/raster_ultra_1_6_check.sh`

Manual: classic map, prop-dense map, portals, weapon, `vid_restart`, map change, occlusion on/off, overlay enable/disable.

## Promotion decision

| Item | Status |
|------|--------|
| Persistent GPU scene | **yes** (opt-in) |
| Frustum cull | **yes** |
| Conservative Hi-Z | **yes** (pyramid + bias; sample-reject GPU path iterative) |
| Indirect cmds | **yes** (host pack + buffer) |
| Meshlets preserve boundaries | **yes** (cone + no material merge) |
| LOD/HLOD scaffolding | **yes** |
| Streaming ownership | **yes** (classic default) |
| Classic BSP correct | **yes** |
| No one-frame hide | **yes** (occlusion + Hi-Z bias) |
| Boot unchanged | **yes** |
| Promote to Ultra default | **no** — overlay only |

## Highest-impact fix (this milestone)

**Occlusion query readback failure previously zeroed all entity visibility** (everything disappeared for a frame). It now seeds **all-visible**, matching camera-cut policy.
