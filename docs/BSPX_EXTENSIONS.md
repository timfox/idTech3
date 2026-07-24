# BSPX extensions

The engine follows the established BSPX envelope:

| Field | Bytes | Encoding |
|---|---:|---|
| magic | 4 | ASCII `BSPX` |
| lump count | 4 | little-endian unsigned integer |
| name | 24 | null-terminated ASCII identifier |
| file offset | 4 | little-endian unsigned integer |
| file length | 4 | little-endian unsigned integer |

The header begins at the four-byte-aligned end of all standard legacy lumps.
Directory records immediately follow the eight-byte header. Their offsets
address payloads in the complete file.

The reader caps the directory at 4096 entries, requires names to terminate
inside 24 bytes, rejects duplicate names, checks every range without
overflow, and rejects directory/payload and payload/payload overlap.

Engine-owned names use the `GX_` prefix. The initial registry contains:
`GX_META`, `GX_STRINGS`, `GX_NODES`, `GX_LEAVES`, `GX_LEAFSURF`,
`GX_SURFACES`, `GX_VERTICES`, `GX_INDICES`, `GX_MODELS`, `GX_MATERIALS`,
`GX_VIS`, `GX_CLUSTERS`, `GX_AREAS`, `GX_PORTALS`, `GX_LIGHTMAPS`,
`GX_LIGHTGRID`, `GX_PROBES`, `GX_DECALS`, `GX_OCCLUSION`, `GX_PHYSICS`,
`GX_NAVMESH`, `GX_ENTBIN`, `GX_ASSETREFS`, `GX_CELLS`, `GX_CELLDEPS`,
`GX_BUILD`, and `GX_HASHES`.

Each custom payload begins with a 48-byte explicitly serialized `GXLP`
header containing its independent schema version, compression identifier,
element count, uncompressed and stored sizes, and content hash. Small payloads
should use no compression. Unknown names or schemas remain opaque for
lossless copying.

