# BSP compatibility contract

This engine has three distinct BSP compatibility tiers. They are intentionally
different formats, not version aliases.

## Frozen legacy formats

GoldSrc BSP30 retains its 32-bit version field, 15 directory entries, original
lump indices, little-endian field encoding, and original record widths.
`bsp30_header_t` is exactly 124 bytes.

Quake 3 IBSP46 retains the `IBSP` magic, version 46, 17 directory entries,
original lump ordering, little-endian field encoding, and original record
widths. `dheader_t` is exactly 144 bytes. The same directory shape is accepted
for the already-supported IBSP47 family, but it remains a separately detected
version.

The `unit_bsp_format` target makes the header, directory-entry, BSP30 node,
leaf, and face sizes compile-time invariants. A change to a frozen declaration
therefore fails the build.

No extended writer may widen a field, add a directory entry, or emit an
extended record while retaining BSP30, IBSP46, or IBSP47 identification.

## Hybrid legacy plus BSPX

A hybrid file contains a complete, valid legacy representation. A standard
`BSPX` header and named directory follow the aligned end of the standard BSP
data. A legacy reader remains free to ignore all trailing bytes.

Extension records never make an invalid or truncated legacy fallback valid.
If the fallback cannot represent the map, hybrid compilation must fail and
recommend native XBSP or an explicitly requested partitioning strategy.

Unknown BSPX names are opaque data. Lossless tools must copy their names and
stored bytes without decoding them. Duplicate names, invalid ranges, and
overlapping extension payloads are rejected.

## Native XBSP

XBSP uses the distinct `XBSP` magic and version 1. It has an explicitly packed
104-byte header and 72-byte named directory records with 64-bit offsets,
stored sizes, uncompressed sizes, and element counts. It cannot be identified
as BSP30 or IBSP.

XBSP is engine-owned. It is not Respawn rBSP and makes no binary-compatibility
claim with that family.

## Runtime boundary

Legacy disk structures are immutable input schemas. New consumers use checked
file/lump views and decode into `bspRuntimeWorld_t`, whose indices and counts
are independent of BSP30's narrow references. File bytes are not mutable
runtime objects.

The current compatibility loaders remain active while their conversion is
migrated incrementally. Ordinary legacy map loading must never require BSPX,
XBSP, compression, external storage, or streaming support.

