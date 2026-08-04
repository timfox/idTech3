# Source 1 VBSP support

The Vulkan renderer contains a clean-room reader for the Source 1 `VBSP`
container (versions 19 through 21). It is implemented in
`renderers/vulkan/tr_source_vbsp.c` and uses no Valve SDK headers, libraries,
source code, or game assets.

The current bridge imports the static world geometry needed for a first render:
planes, vertices, edges, surfedges, faces, entity text, and the texture-name
lookup tables. Faces are converted to ordinary idTech3 planar surfaces and
submitted through the existing Vulkan deferred/clustered paths. Material names
are looked up as `source/<material>` so a matching user-provided idTech3
material can be supplied; absent VMT/VTF content uses the renderer's normal
fallback.

Planar face LOD is supported by the same topology reduction used for native
idTech3 BSP faces. It is enabled by default with `r_bspLod 1`; use `r_bspLod 2`
for the aggressive tier or `r_bspLod 0` to disable it.
`r_bspLodDistance` controls the distance-to-face-radius transition ratio.
`bsp_lod_status` reports how many imported faces received reduced index lists.

The entity lump is parsed independently for Source-inspired point lights
(`light`, `light_spot`, and `light_environment`). Their origin, color, and
range are submitted through the normal renderer light API when
`r_sourceEntities 1` is enabled. No Source game behavior is executed.

FGDs are optional editor/schema inputs, not required to render entity
instances. Use `source_fgd_load path/to/entities.fgd` and
`source_fgd_status` to load and inspect the clean-room schema summary.

This is intentionally an initial geometry bridge, not a Source game runtime.
Source node/leaf topology and RLE-compressed cluster visibility are imported
when those lumps validate; malformed or absent visibility data falls back to
all-visible rendering. Displacement surfaces, static props, VTF/VMT decoding,
light styles, and Source-specific entity behavior remain follow-up work. Users
must provide any map and material assets they have the right to use.
