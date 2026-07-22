# Meshlets (CPU cull + compact / MDI draw)

Chocolate **Nanite-lite**: bake triangle clusters (meshlets) at **model load** into a local-space cache, frustum-cull on CPU, then draw only visible triangles. Does **not** require mesh shaders.

## Enable

```
set r_meshlets 1
set r_meshletsCompact 1
set r_meshletsMdi 1
set r_meshletsMdiDraw 1   // Phase 2: real vkCmdDrawIndexedIndirect
set r_meshletsLod 1       // screen-space projected AABB cull
set r_meshletsLodPixels 2
exec demo_meshlets.cfg
meshlet_status
```

When enabled, MD3 surfaces (≤512 verts / ≤1024 tris) bake at load, skip add if fully culled, and compact draw when partially visible. With `r_meshletsMdiDraw 1`, each visible meshlet range is issued via `vkCmdDrawIndexedIndirect` against the tess index buffer (falls back to a single `vkCmdDrawIndexed` if the entry point is missing). With `r_meshletsLod 1`, frustum-visible meshlets whose projected AABB diagonal is below `r_meshletsLodPixels` are dropped (distance / FOV screen-size LOD). **Animated MD3** (`frame != 0`) skips meshlet cull/LOD/compact and draws the full surface — bind-pose AABBs are unsafe under animation.

## Cvars / commands

| | |
|--|--|
| `r_meshlets` | Default 0 |
| `r_meshletsCompact` | Partial triangle draw from visible meshlets (default 1; also implied by MDI draw) |
| `r_meshletsMdi` | Pack `VkDrawIndexedIndirectCommand` metrics (default 0) |
| `r_meshletsMdiDraw` | GPU `vkCmdDrawIndexedIndirect` for tess-relative meshlet ranges (default 0) |
| `r_meshletsLod` | Screen-space projected AABB LOD cull (default 0) |
| `r_meshletsLodPixels` | Minimum projected diagonal in pixels to keep (default 2) |
| `r_meshletsBspPilot` | Cheat: meshlet bake/cull for small BSP `SF_FACE` (default 0) |
| `r_meshletsModelPilot` | Static MD3 meshlet path; skip animated (default 1) |
| `meshlet_status` | Bake/cache/cull/LOD/compact/MDI counts |

## API

- `R_Meshlets_Bake` / `CacheLocal` / `Lookup`
- `R_Meshlets_CullViewFrustumXform`
- `R_Meshlets_AppendVisibleIndexes` — used by `RB_SurfaceMesh` (compact + optional MDI enqueue)
- `R_Meshlets_PackIndirect` — host MDI metrics
- `R_Meshlets_TryDrawIndirect` — flush pending cmds from `vk_draw_geometry`
- `R_Meshlets_BeginSurface` — clear per-batch MDI queue

## Deferred

- Persistent per-surface IBO (avoid remapping into tess)
- `VK_EXT_mesh_shader` task/mesh pipelines
- Full BSP world / skinned meshlets (pilot: `r_meshletsBspPilot` for one face class)
- GPU cull / continuous cluster LOD streaming

## Raster Ultra 1.6

See [RASTER_ULTRA_1.6.md](RASTER_ULTRA_1.6.md): stable uint64 cache keys + generation, normal-cone metadata/cull, companion GPU scene / Hi-Z overlay (`vulkan_overlay_raster_ultra_1_6_geometry.cfg`).

See also [RENDERER_2027.md](RENDERER_2027.md) Phase 2.
