# BSP limits and loader audit

This is the initial inventory for the normalized BSP migration.

## Current consumers

| Area | Files | Current format dependency |
|---|---|---|
| Quake 3 collision | `engine/core/cm_load.c` | copies/casts `dheader_t`, indexed legacy lumps |
| BSP30 collision | `engine/core/cm_bsp30.c` | isolated BSP30 adapter with checked lump helper |
| Quake 3 renderer | `renderers/vulkan/tr_bsp.c` | mutable `dheader_t` view and indexed lumps |
| BSP30 renderer | `renderers/vulkan/tr_bsp30.c` | isolated BSP30 header and lump views |
| streamed collision | `engine/core/cm_stream_merge.c` | direct `dheader_t` copy |
| streamed renderer | `renderers/vulkan/tr_bsp_stream.c` | direct `dheader_t` copies |
| navigation | `modules/navigation/nav_bsp_extract.cpp`, `nav_recast.cpp` | direct IBSP header copy |
| open-world smoke | `engine/core/com_openworld_smoke.c` | direct IBSP header copy |
| checksum/identity | map loading and filesystem call sites | primarily whole-file legacy checksum |

The direct header-copy sites are migration targets. They are not changed by
the container foundation because each requires consumer-specific normalized
conversion and regression maps.

## Existing Quake 3 limits

| Limit | Value | Origin/category |
|---|---:|---|
| header lumps | 17 | `FILE_FORMAT` |
| lump offset/length | signed 32-bit disk fields | `FILE_FORMAT` |
| models | 4096 | `COMPILER` / `ARBITRARY_ENGINE_LIMIT` |
| brushes | 32768 | `COMPILER` |
| entities | 2048 | `COMPILER` |
| entity text | 262144 bytes | `COMPILER` |
| shaders | 1024 | `COMPILER` |
| areas/fogs | 256 each | `COMPILER`, area network coupling |
| planes/nodes/leaves | 131072 each | `COMPILER` |
| leaf brushes | 262144 | `COMPILER` |
| lighting/light grid | 8388608 bytes each | `COMPILER` |
| visibility | 2097152 bytes | `COMPILER` |
| draw surfaces | 131072 | `COMPILER` |
| draw vertices/indices | 524288 each | `COMPILER` |
| world coordinates | ±131072 | `RUNTIME` / `COLLISION` / `RENDERER` |

These constants must not be raised merely to claim larger map support.
IBSP record fields and every downstream allocation/reference are audited
before changing any one of them.

## BSP30 structural limits

| Item | Width | Origin/category |
|---|---:|---|
| directory entries | 15 | `FILE_FORMAT` |
| lump offset/length | signed 32-bit | `FILE_FORMAT` |
| node children/bounds | signed 16-bit | `FILE_FORMAT` |
| node face spans | unsigned 16-bit | `FILE_FORMAT` |
| leaf bounds | signed 16-bit | `FILE_FORMAT` |
| leaf marksurface spans | unsigned 16-bit | `FILE_FORMAT` |
| face plane and edge count | 16-bit | `FILE_FORMAT` |
| edge vertex references | unsigned 16-bit | `FILE_FORMAT` |
| collision hulls | 4 | `FILE_FORMAT` / `COLLISION` |

Large replacements therefore require GX or XBSP records; extra directory
slots alone cannot widen these references.

## Status commands

The requested `bsp_format_status`, `bsp_limits_status`, `bsp_loader_status`,
and `bsp_writer_status` console commands are not yet wired. Their data model
should report this inventory plus live registry and capability information;
they must not imply that unconverted consumers use normalized structures.

