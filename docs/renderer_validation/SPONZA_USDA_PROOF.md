# Sponza USDA validation

Sponza is the canonical USDA validation scene for the modern renderer. The
repo-local asset is loaded through the OpenArena base tree using the native
FreeUSD district path; the asset itself is not copied into game data.

`tests/data/usd/sponza_fixture.usda` declares `District_Sponza`. The runtime
resolves its full payload as `world/districts/sponza.usda`, which may be a
symlink to `sponza/main_sponza/NewSponza_Main_USD_Zup_003.usda` during local
validation. The Z-up layer declares `metersPerUnit = 0.01` and carries the
material texture references used by the proof.

The importer now accepts the Sponza layer size (417 MiB), multiline array
attributes, trailing interpolation metadata, output declarations, and float4
material tuples. Mesh discovery also falls back to the composed traversal when
the optional `primOrder` metadata is absent. A live validation run imported
`District_Sponza` and initialized the mode-3 Forward+/WBOIT/Ambient Visibility
stack without a crash.

The single-surface full-draw gate is removed: large USDA meshes are now passed
to the existing multi-surface GPU model representation instead of being
rejected and sent to the legacy importer. The remaining scene milestone is
unbudgeted multi-mesh streaming; the current opt-in composer imports a bounded
set of mesh prims into one chunked model while material/transform residency is
being expanded.

The first drawable proof used the authored `/root/_st_floor/arch_stones_03`
mesh. The larger `/root/_st_floor/arch_stones_01` now also draws through the
modern path:

```text
r_renderMode 3 r_forwardPlus 1 r_ambientVisibilityMode 2
r_oit 1 r_oitForwardPlus 1 r_freeusdMeshPath arch_stones_03
district_load world/sponza_fixture.usda
district_load_full 0
```

For a deterministic visual proof from an existing game map, add:

```text
set r_districtAnchorView 1
set r_districtLoadRadius 100000
```

`r_districtAnchorView` is validation-only placement: it translates the loaded
district so its bounds center is at the current camera. The BSP remains the
host map, but Sponza is no longer dependent on the player spawn or a random
camera location. Normal world-district placement remains the default (`0`).

Runtime evidence: `FreeUSD: selected mesh ... arch_stones_01 (63053 tris)`;
`FreeUSD: tessellated mesh ... (63053 tris)`; `MeshImport: ... (189159 verts,
63053 tris)`; `District_Sponza full load ... model 89`. Capture:
`screenshots/sponza_large_proof.jpg`. This is a large-mesh and pipeline proof,
not yet a complete Sponza composition capture.

The multi-mesh proof uses:

```text
r_freeusdImportAllMeshes 1
r_freeusdMeshBudget 250000
```

It composed 115 authored meshes and 229,786 triangles into one GPU model,
stopping at the explicit budget. Runtime evidence is
`FreeUSD: composed 115 meshes (229786 tris, budget 250000)` and
`MeshImport: ... (689358 verts, 229786 tris)`. Capture:
`screenshots/sponza_composed_proof.jpg`.

## Dedicated scene capture

OpenArena's retail QVM normally owns the active camera and suppresses modern
district entities. The validation path now has an explicit scene mode:

```
seta r_districtCamera 1
seta r_districtOnly 1
seta r_districtCameraDistance 0.6
```

`r_districtCamera` redirects only the submitted refdef to a bounds-derived
USDA camera; it does not mutate network player state. `r_districtOnly` hides
the active BSP world and QVM entities, leaving the loaded district as the
primary visibility owner. Capture `sponza_centered_proof.jpg` is the first
proof with no random OpenArena room or gameplay entities in the frame.

This capture is intentionally a geometry/import proof. The composed model
still uses the current white material fallback, so texture/material parity is
the next Sponza milestone; it should not yet be presented as a final beauty
render.

The USDA material bridge now resolves Windows-style asset paths, texture paths
relative to the USDA directory, PreviewSurface-to-NodeGraph-to-UsdUVTexture
connections, and material bindings authored on the mesh, GeomSubset children,
or enclosing Xforms. This last fallback is important for composed Sponza
layers, where optional scene metadata does not always retain the binding.
Authored world-space normals are also preserved. GeomSubset surfaces now carry
their authored normal and emissive texture qpaths into the Vulkan material stage,
so PBR permutations are selected by the same shader flags used by native assets.
Roughness/metallic packing and scalar fallback values remain the next material
parity gate before this is a complete PBR proof.

After that gate, the first capture will certify mode-3 deferred opaque
lighting, Forward+ clustered lights, WBOIT alpha/additive transparency, SSAO,
and shadow-atlas ownership in one frame, followed by one-feature-at-a-time A/B
captures with status lines and image hashes.

The parity capture sequence is codified in
`examples/demo_game/mod/sponza_material_parity.cfg`. It fixes the USDA camera,
district-only visibility, 250,000-triangle residency budget, exposure, and
temporal settings, then emits Forward+, deferred, WBOIT, SSR, and RTX captures.
