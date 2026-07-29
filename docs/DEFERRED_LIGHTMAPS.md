# Deferred lightmaps and deluxe maps

Lightmap ownership is explicit. A Deferred-owned pixel stores decoded,
scene-linear irradiance in `GBufferSurfaceData.rgb`; its owner bit is separate.
The Deferred light loop skips duplicate static sun diffuse when the lightmap
owns that term. Forward-owned classic materials retain the original Forward+
lightmap evaluation from start to finish.

Directional deluxe mode is implemented as a conservative compute-side
approximation until `GBufferSurfaceData` grows a true deluxe-vector channel:
`r_deferredLightmapMode 1` preserves `SurfaceData.rgb` energy and applies a
bounded normal-facing directional shape from the dominant sun direction.
`r_deferredLightmapMode 2` blends irradiance-only and approximate directional
terms for comparison captures. Materials that require an exact deluxe/custom
stage must remain Forward-owned. `lightmap_parity_validate` therefore requires
GPU reference evidence before certification and must detect missing, doubled,
gamma-wrong, UV-mismatched, or invalid-owner lightmaps.

`deferred_status`, `deferred_contract_status`, and `deferred_certify_status`
print the active lightmap mode. The deferred contract hash includes that mode,
so switching between irradiance, directional approximation, and compare modes
invalidates stale certification evidence.
