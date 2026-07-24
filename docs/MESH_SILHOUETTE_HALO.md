# Mesh Silhouette Halo

Screen-space luminous coronas and warped fringes around correctly shaped meshes.

## Not this issue

* Exploding / stretched triangles (see `EXPLODING_GEOMETRY.md`)
* Texture corruption
* Broken model transforms

## Commands

```text
mesh_halo_status
mesh_halo_capture
mesh_halo_validate
mesh_halo_pass_bisect
r_meshHaloDebug 0..8
```

## Production fixes (2026-07)

| Pass | Change |
|------|--------|
| `bloom.frag` | Depth-aware firefly + far-side silhouette extract gate |
| `smaa_compose.frag` | Relative view-depth blend rejection + depth descriptor |
| `av_filter.comp` | Exact AV texel fetches; full-resolution depth/normal guidance uses trace-relative normalized texel-center UVs |
| `weapon_taa.frag` | Relative view-depth confidence (default thresh 0.04) |
| `ssao.frag` | Relative view-depth silhouette skip |
| `rcgi_upscale.comp` | `texelFetch` bilateral upsample |

## First alignment defect found

The reduced-resolution Ambient Visibility filter used AV trace coordinates
directly in `texelFetch` calls against full-resolution depth and normal
textures. At half resolution this did not select the corresponding
full-resolution screen location, so bilateral depth/normal rejection was
guided by unrelated pixels.

The AV signal remains integer-addressed at its own resolution. Full-resolution
guidance is now sampled at:

```glsl
(vec2(tracePixel) + 0.5) / vec2(traceExtent)
```

This also preserves correct alignment for odd render extents.

## Definition of done

* Opaque silhouette correct before post
* No bright/dark fringe expanding the true contour beyond intentional soft bloom
* Background does not leak around foreground; SMAA does not dilate across depth edges
* Weapon boundary clean against world/sky
* Regression: `tests/scripts/test_mesh_halo_regression.sh`
