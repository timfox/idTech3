# Meshlets (CPU cull)

Chocolate **Nanite-lite**: bake triangle clusters (meshlets) and frustum-cull on CPU. Does **not** require `VK_NV_mesh_shader` / mesh pipelines.

## Enable

```
set r_meshlets 1
meshlet_status
```

When enabled, MD3 surfaces (≤512 verts / ≤1024 tris) bake meshlets each add-pass and skip draw if all clusters are outside the view frustum.

## Cvars / commands

| | |
|--|--|
| `r_meshlets` | Default 0 |
| `meshlet_status` | Bake call count + last cull visible/total |

## API

- `R_Meshlets_Bake` — cluster triangles into AABB meshlets
- `R_Meshlets_CullViewFrustum` — cull against `tr.viewParms.frustum`

## Deferred

- Multi-draw indirect GPU path
- `VK_EXT_mesh_shader` task/mesh pipelines
- BSP world meshlets / streaming
- Skinned / morph meshlets
