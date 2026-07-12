# Meshlets (CPU cull)

Chocolate **Nanite-lite**: bake triangle clusters (meshlets) at **model load** into a local-space cache, then frustum-cull on CPU each frame (entity-pose AABB transform). Does **not** require `VK_NV_mesh_shader` / mesh pipelines.

## Enable

```
set r_meshlets 1
set r_meshletsMdi 1   // pack VkDrawIndexedIndirectCommand list (scaffold)
exec demo_meshlets.cfg
meshlet_status
```

When enabled, MD3 surfaces (≤512 verts / ≤1024 tris) bake meshlets at load (and on first miss) and skip draw if all clusters are outside the view frustum. With `r_meshletsMdi 1`, visible clusters also pack an MDI command list (GPU multi-draw still deferred).

## Cvars / commands

| | |
|--|--|
| `r_meshlets` | Default 0 |
| `r_meshletsMdi` | Pack indirect draw cmds from visible meshlets (default 0) |
| `meshlet_status` | Bake calls, cache hits/misses, cull + MDI counts |

## API

- `R_Meshlets_Bake` — cluster triangles into AABB meshlets
- `R_Meshlets_CacheLocal` / `R_Meshlets_Lookup` — bake-at-load cache keyed by surface pointer
- `R_Meshlets_CullViewFrustum` / `R_Meshlets_CullViewFrustumXform` — cull against `tr.viewParms.frustum`
- `R_Meshlets_PackIndirect` — host `VkDrawIndexedIndirectCommand` packing

## Deferred

- GPU `vkCmdDrawIndexedIndirect` / multi-draw path
- `VK_EXT_mesh_shader` task/mesh pipelines
- BSP world meshlets / streaming
- Skinned / morph meshlets
