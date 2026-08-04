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
material tuples. The current remaining gate is scene snapshot extraction:
FreeUSD reports no Mesh prims for the monolithic layer, after which the legacy
fallback attempts to buffer the 417 MiB file and exhausts the renderer hunk.
No Sponza screenshot is claimed until that ownership/mesh-selection issue is
fixed.

After that gate, the first capture will certify mode-3 deferred opaque
lighting, Forward+ clustered lights, WBOIT alpha/additive transparency, SSAO,
and shadow-atlas ownership in one frame, followed by one-feature-at-a-time A/B
captures with status lines and image hashes.
