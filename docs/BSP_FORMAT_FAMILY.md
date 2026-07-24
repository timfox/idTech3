# BSP format family

`engine/core/bsp_format.h` is the additive container and normalization API.
`engine/core/qfiles.h` and `engine/core/qfiles_bsp30.h` remain the frozen legacy
disk declarations.

## Detection order

Detection uses bytes and validated directories, never a filename extension:

1. `XBSP`, version, header size, directory entry size, declared file size, and
   every 64-bit lump range.
2. BSP30 version 30 plus its complete 15-entry directory.
3. `IBSP` plus version 46 or 47 and the complete 17-entry directory.
4. For a valid legacy base, the aligned end of all standard data is checked for
   the exact standard `BSPX` header and named directory.

Malformed native or legacy directories are not downgraded into a different
format.

## Normalized world

`bspRuntimeWorld_t` owns format-neutral arrays for planes, nodes, leaves,
surfaces, vertices, 32-bit indices, leaf-surface links, models, materials,
visibility, lighting, and extensions. File offsets and aggregate sizes use
64-bit types. Runtime element references use 32 bits and reserve
`UINT32_MAX` as invalid.

The normalized definition is the migration target for renderer, collision,
visibility, navigation, and tool consumers. It deliberately contains no
packed disk records.

## Extension selection

The deterministic policy is:

1. validate the complete extension set and its manifest;
2. use a valid `GX_*` replacement when its schema and dependencies are
   supported;
3. otherwise decode the corresponding legacy lump;
4. reject a required invalid extension set, or use legacy-only fallback only
   when the manifest permits it.

Widened nodes must not be combined with leaves or leaf-surface tables from a
different extension build. `GX_META` will carry the base and extension-set
hashes used to enforce that rule.

## Implementation state

The format-neutral definitions, safe arithmetic, detection, BSPX/XBSP
directory readers, explicit XBSP header/directory serialization, payload
header validation, and GX registry are implemented. Existing BSP30 and IBSP
renderer/collision paths have not yet all been converted to populate the
normalized world. That migration is deliberately separate from freezing the
wire formats.

