# Planar BSP LOD

The Vulkan renderer can reduce topology for distant planar BSP faces through
`r_bspLod`:

- `0`: disabled
- `1`: balanced
- `2`: aggressive

`r_bspLodDistance` controls the distance-to-face-radius ratio at which the
first reduction is selected. Two conservative boundary-ring index sets are
generated during world loading. The original face indices remain active for
near views, and the VBO shortcut is bypassed only when a face has a usable LOD
so the selected topology is honored.

The pass is shared by native Q3 BSP, BSP30, and Source VBSP world faces. It
does not alter collision, visibility, lightmaps, or material data. Highly
non-convex imported faces should remain on the full-resolution path until
Source displacement and robust polygon triangulation support is added.
