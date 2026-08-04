# Transparent Texture Authoring

## Prefer

- Straight-alpha textures with **dilated** RGB under soft edges (RGB continues past α=0 fringe).
- Or true premultiplied textures with matching sampler/filter semantics, declared via material name `*premul*` or future PBR metadata.

## Avoid

- Straight-alpha with **black** RGB under α≈0 (bilinear mip fringes).
- Premultiplied data labeled as straight (double association → dark edges).
- Straight data labeled as premul (washed / bright edges).

## Filtering

| Mode | Use |
|------|-----|
| `STRAIGHT_SOURCE` | Classic Q3/OA |
| `PREMULTIPLIED_SOURCE` | Associated authoring |
| `EDGE_DILATED_STRAIGHT` | Straight with dilated RGB |

OIT source passes use implicit-LOD sampling, so mip transitions honor the active
trilinear/aniso sampler. Dilate color into transparent texels offline when using
straight alpha to prevent RGB fringes at bilinear/mip boundaries.

Texture atlases: keep transparent padding dilated; avoid neighboring sprite color bleeding across α=0 gutters (clamp edges of atlas regions when possible).

## Runtime

`r_alphaFilterDebug 1`, `r_alphaDebug 8/9`, `r_transparentEdgePolicy`.
