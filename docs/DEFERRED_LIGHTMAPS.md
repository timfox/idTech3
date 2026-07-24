# Deferred lightmaps and deluxe maps

Lightmap ownership is explicit. A Deferred-owned pixel stores decoded,
scene-linear irradiance in `GBufferSurfaceData.rgb`; its owner bit is separate.
The Deferred light loop skips duplicate static sun diffuse when the lightmap
owns that term. Forward-owned classic materials retain the original Forward+
lightmap evaluation from start to finish.

Directional deluxe evaluation is not production-certified yet. Materials that
require an unrepresentable deluxe/custom stage must remain Forward-owned.
`lightmap_parity_validate` therefore requires GPU reference evidence before
certification and must detect missing, doubled, gamma-wrong, UV-mismatched, or
invalid-owner lightmaps.
