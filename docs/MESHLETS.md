# Meshlets (CPU cull + compact draw)

Chocolate **Nanite-lite**: bake triangle clusters (meshlets) at **model load** into a local-space cache, frustum-cull on CPU, then **emit only visible triangles** into tess (`r_meshletsCompact`, default 1). Does **not** require mesh shaders.

## Enable

```
set r_meshlets 1
set r_meshletsCompact 1
set r_meshletsMdi 1   // optional: pack indirect cmd metrics
exec demo_meshlets.cfg
meshlet_status
```

When enabled, MD3 surfaces (≤512 verts / ≤1024 tris) bake at load, skip add if fully culled, and compact draw when partially visible.

## Cvars / commands

| | |
|--|--|
| `r_meshlets` | Default 0 |
| `r_meshletsCompact` | Partial triangle draw from visible meshlets (default 1) |
| `r_meshletsMdi` | Pack `VkDrawIndexedIndirectCommand` metrics (default 0) |
| `meshlet_status` | Bake/cache/cull/compact/MDI counts |

## API

- `R_Meshlets_Bake` / `CacheLocal` / `Lookup`
- `R_Meshlets_CullViewFrustumXform`
- `R_Meshlets_AppendVisibleIndexes` — used by `RB_SurfaceMesh`
- `R_Meshlets_PackIndirect` — host MDI scaffold

## Deferred

- GPU `vkCmdDrawIndexedIndirect`
- `VK_EXT_mesh_shader` task/mesh pipelines
- BSP world / skinned meshlets
