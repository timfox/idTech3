# Deferred software hair sidecar

This is the engine contract for the deferred software-rasterized hair method
described by Lipp et al., *Deferred Software Rasterization for Efficient
Real-time Hair Rendering* (arXiv:2607.04230).

## Ownership

`r_hairDeferred 1` is an opt-in sidecar. Ordinary opaque geometry remains
owned by mode-3 deferred lighting. Hair owns only its compressed visibility,
deferred hair shading, coverage filter, and depth composite. Existing shadow
atlas pages remain the first shadow source; selective RT may add hero-hair
secondary lighting later, but never replaces primary visibility.

## Groom contract

An importer will provide bundles with layer ranges, strand/control-point
counts, a style index, per-style LOD lambda, and baked mesh AO. Styling data is
sampled during deferred shading so the visibility payload does not store a
material index for every strand.

## Planned GPU stages

1. Bundle LOD prepass and indirect dispatch generation (`hair_lod.comp` is
   now compiled as the first executable stage).
2. Cooperative strand assembly and compute line rasterization.
3. Atomic compressed 64-bit visibility payload (depth, octahedral tangent,
   styling coordinates, coverage/AO).
4. Clustered deferred hair shade.
5. Connectivity/coverage filtering and depth composite into SceneHDR.

The current milestone exposes the cvar, ownership/status contract, deterministic
LOD shader, and a compiled packed-64 visibility arbitration kernel
(`hair_visibility_atomic.comp`). The kernel proves the encoding/arbitration
contract for projected samples; it is not yet the paper’s complete line
rasterizer. It does not yet claim a groom importer, live frame dispatch, or
deferred hair composite. `hair_deferred_status`
continues to report `contract_only` until groom buffers are wired.
