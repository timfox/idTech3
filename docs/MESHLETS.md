# Virtual Geometry Meshlets

Northlight-inspired virtual geometry path for this renderer: bake triangle clusters (meshlets) at **model load** into a local-space cache, frustum/cone-cull and screen-LOD those clusters, compact visible triangles, then draw visible ranges with indexed MDI when available.

This is not a proprietary Northlight implementation. The production path is portable Vulkan meshlet MDI, with optional `VK_NV_mesh_shader` capability kept behind `r_vk_meshShaderNV` for mesh-shader experiments and diagnostics.

## Enable

```
set r_virtualGeometry 1
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
| `r_virtualGeometry` | Master switch for virtual geometry meshlets (default 1; safe profile sets 0) |
| `r_meshlets` | Default 0 |
| `r_meshletsCompact` | Partial triangle draw from visible meshlets (default 1; also implied by MDI draw) |
| `r_meshletsMdi` | Pack `VkDrawIndexedIndirectCommand` metrics (default 0) |
| `r_meshletsMdiDraw` | GPU `vkCmdDrawIndexedIndirect` for tess-relative meshlet ranges (default 0) |
| `r_meshletsLod` | Screen-space projected AABB LOD cull (default 0) |
| `r_meshletsLodPixels` | Minimum projected diagonal in pixels to keep (default 2) |
| `r_meshletsGpuCull` | Portable compute cull + GPU-generated indirect commands (default 1) |
| `r_meshletsHiZ` | Conservative Hi-Z rejection when the pyramid is ready (default 1) |
| `r_meshletsStreaming` | Gate commands on persistent surface residency (default 1) |
| `r_meshletsBsp` / `r_meshletsSkinned` | Enable shared BSP and dynamic/skinned record inputs |
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

## Portable GPU-driven baseline

Surface indexes now have persistent GPU-visible storage, and
`meshlet_cull_indirect.comp` consumes the shared meshlet/object records to do
frustum, cone, screen-LOD, and conservative Hi-Z tests before atomically
generating indexed-MDI commands. Scene generation and stream residency are
part of the record ABI, so stale or evicted surfaces are rejected. BSP and
skinned/dynamic pilots use this same record format and fall back to CPU compact
submission while a stream is loading.

Mesh shaders remain an optional optimization after portable MDI. They consume
the same records and ownership; they are never required for correctness.

## Raster Ultra 1.6

See [RASTER_ULTRA_1.6.md](RASTER_ULTRA_1.6.md): stable uint64 cache keys + generation, normal-cone metadata/cull, companion GPU scene / Hi-Z overlay (`vulkan_overlay_raster_ultra_1_6_geometry.cfg`).

See also [RENDERER_2027.md](RENDERER_2027.md) Phase 2.
