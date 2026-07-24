# Exploding Geometry / Stretched Triangles

**Status:** Active for BSP30 maps (`surf_aztec`) and VBO soft-IBO overflow  
**Not:** post-process haloing / SMAA / bloom / AO

## Classification

```text
EXPLODING_TRIANGLES / STRETCHED_TRIANGLE_SPIKES
```

Rasterized triangles with invalid connectivity or exterior fan triangles — not screen-space edge bleed.

## Proven owners (surf_aztec)

### 1. BSP30 non-convex face triangulation (primary)

BSP30 faces are edge-walked rings that may be **non-convex**. A triangle-fan from vertex 0 emits **exterior** triangles (hard black / stretched wedges on letter brushes such as `func_illusionary *17` “AZ”).

| Method | Exterior-centroid faces on surf_aztec |
|--------|----------------------------------------|
| Fan hub=0 | ~9.6% (208 / 2167) |
| Ear-clip only | ~1.7% (37 / 2167) |
| Ear-clip + hub-fan search | target ≈ 0% (centroid-validated) |

Code: `renderers/common/tr_bsp30_triangulate.c`  
Load: `renderers/vulkan/tr_bsp30.c`  
Plane winding: Newell alignment before cull (`R_CullSurface`).

### 2. VBO soft-IBO overflow (secondary)

`VBO_AddItemDataToSoftBuffer` previously ignored `vk_tess_index` failure (`~0U`) and still inflated `soft_buffer_indexes`. Binding offset `~0U` as UINT32 index data produces screen-spanning garbage triangles.

Fix: abort soft upload on `~0U`; require contiguous tess offsets; refuse draw if offset is `~0U`.

## Diagnostics

```text
geometry_corruption_status
geometry_corruption_validate
geometry_corruption_capture
geometry_draw_count
geometry_draw_limit <n>
geometry_draw_range <first> <last>
r_geometryCorruptionDebug 0..12
```

Mode 9: use with `r_showtris 1` for wireframe overlay.


## Tests

```text
ctest -R unit_bsp30_triangulate
tests/scripts/test_geometry_corruption_regression.sh
```

Offline map audit (optional):

```text
gcc -O2 -o /tmp/bsp30_map_tri tests/unit/test_bsp30_map_triangulate.c \
  renderers/common/tr_bsp30_triangulate.c -Irenderers/common -Iengine/core -lm
/tmp/bsp30_map_tri maps/surf_aztec.bsp
```
