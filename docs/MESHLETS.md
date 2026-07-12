# Meshlets (CPU cull)

Chocolate **Nanite-lite**: bake triangle clusters (meshlets) at **model load** into a local-space cache, then frustum-cull on CPU each frame (entity-pose AABB transform). Does **not** require `VK_NV_mesh_shader` / mesh pipelines.

## Enable

```
set r_meshlets 1
exec demo_meshlets.cfg
meshlet_status
```

When enabled, MD3 surfaces (≤512 verts / ≤1024 tris) bake meshlets at load (and on first miss) and skip draw if all clusters are outside the view frustum.

## Cvars / commands

| | |
|--|--|
| `r_meshlets` | Default 0 |
| `meshlet_status` | Bake calls, cache hits/misses, last cull visible/total |

## API

- `R_Meshlets_Bake` — cluster triangles into AABB meshlets
- `R_Meshlets_CacheLocal` / `R_Meshlets_Lookup` — bake-at-load cache keyed by surface pointer
- `R_Meshlets_CullViewFrustum` / `R_Meshlets_CullViewFrustumXform` — cull against `tr.viewParms.frustum`

## Deferred

- Multi-draw indirect GPU path
- `VK_EXT_mesh_shader` task/mesh pipelines
- BSP world meshlets / streaming
- Skinned / morph meshlets
