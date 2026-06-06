#!/usr/bin/env python3
"""
Emit a minimal Quake 3 IBSP v46 sector chunk for cm_stream_merge collision overlay.

Sector BSPs live in local sector space (0..sector_size). The engine offsets brushes
by (cellX, cellY) * cm_streamSectorSize at merge time.

Collision-only: one axial solid brush (platform slab). No nodes/leaves required for merge.

With --visual: adds drawVerts/surfaces for the platform top so r_bspStream can render
authored geometry instead of inferring brush-top quads.
"""
from __future__ import annotations

import argparse
import os
import struct
import sys
from typing import List, Tuple

BSP_IDENT = (ord("P") << 24) + (ord("S") << 16) + (ord("B") << 8) + ord("I")
BSP_VERSION = 46
HEADER_LUMPS = 17

LUMP_ENTITIES = 0
LUMP_SHADERS = 1
LUMP_PLANES = 2
LUMP_NODES = 3
LUMP_LEAFS = 4
LUMP_LEAFSURFACES = 5
LUMP_LEAFBRUSHES = 6
LUMP_MODELS = 7
LUMP_BRUSHES = 8
LUMP_BRUSHSIDES = 9
LUMP_DRAWVERTS = 10
LUMP_DRAWINDEXES = 11
LUMP_FOGS = 12
LUMP_SURFACES = 13
LUMP_LIGHTMAPS = 14
LUMP_LIGHTGRID = 15
LUMP_VISIBILITY = 16

CONTENTS_SOLID = 1
MST_PLANAR = 1


def pad4(data: bytes) -> bytes:
    while len(data) % 4:
        data += b"\x00"
    return data


def pack_shader(name: str, surface_flags: int = 0, contents: int = CONTENTS_SOLID) -> bytes:
    b = name.encode("ascii")[:63].ljust(64, b"\x00")
    return b + struct.pack("<II", surface_flags, contents)


def pack_plane(nx: float, ny: float, nz: float, dist: float) -> bytes:
    return struct.pack("<ffff", nx, ny, nz, dist)


def pack_brushside(plane_num: int, shader_num: int) -> bytes:
    return struct.pack("<ii", plane_num, shader_num)


def pack_brush(first_side: int, num_sides: int, shader_num: int) -> bytes:
    return struct.pack("<iii", first_side, num_sides, shader_num)


def pack_drawvert(
    xyz: Tuple[float, float, float],
    st: Tuple[float, float],
    lightmap: Tuple[float, float],
    normal: Tuple[float, float, float],
    color: Tuple[int, int, int, int] = (255, 255, 255, 255),
) -> bytes:
    return struct.pack(
        "<fff ff ff fff BBBB",
        xyz[0], xyz[1], xyz[2],
        st[0], st[1],
        lightmap[0], lightmap[1],
        normal[0], normal[1], normal[2],
        color[0], color[1], color[2], color[3],
    )


def pack_dsurface(
    shader_num: int,
    surface_type: int,
    first_vert: int,
    num_verts: int,
    first_index: int,
    num_indexes: int,
    plane_normal: Tuple[float, float, float] = (0.0, 0.0, 1.0),
) -> bytes:
    lightmap_num = -1
    lightmap_x = lightmap_y = lightmap_w = lightmap_h = 0
    origin = (0.0, 0.0, 0.0)
    vecs = (
        (1.0, 0.0, 0.0),
        (0.0, 1.0, 0.0),
        plane_normal,
    )
    flat_vecs = [c for v in vecs for c in v]
    return struct.pack(
        "<iii ii ii 5i 3f 9f ii",
        shader_num,
        -1,
        surface_type,
        first_vert,
        num_verts,
        first_index,
        num_indexes,
        lightmap_num,
        lightmap_x,
        lightmap_y,
        lightmap_w,
        lightmap_h,
        *origin,
        *flat_vecs,
        0,
        0,
    )


def axial_box_planes(mins: Tuple[float, float, float], maxs: Tuple[float, float, float]) -> List[Tuple[float, float, float, float]]:
    """Six brush sides in CM_BoundBrush order (even=min, odd=max per axis)."""
    return [
        (-1.0, 0.0, 0.0, -mins[0]),
        (1.0, 0.0, 0.0, maxs[0]),
        (0.0, -1.0, 0.0, -mins[1]),
        (0.0, 1.0, 0.0, maxs[1]),
        (0.0, 0.0, -1.0, -mins[2]),
        (0.0, 0.0, 1.0, maxs[2]),
    ]


def build_visual_lumps(
    platform_mins: Tuple[float, float, float],
    platform_maxs: Tuple[float, float, float],
) -> Tuple[bytes, bytes, bytes]:
    z = platform_maxs[2]
    corners = [
        (platform_mins[0], platform_mins[1], z),
        (platform_maxs[0], platform_mins[1], z),
        (platform_maxs[0], platform_maxs[1], z),
        (platform_mins[0], platform_maxs[1], z),
    ]
    normal = (0.0, 0.0, 1.0)
    drawverts_data = b"".join(
        pack_drawvert(
            xyz,
            (xyz[0] / 64.0, xyz[1] / 64.0),
            (0.0, 0.0),
            normal,
        )
        for xyz in corners
    )
    indexes_data = struct.pack("<6i", 0, 1, 2, 0, 2, 3)
    surfaces_data = pack_dsurface(
        shader_num=1,
        surface_type=MST_PLANAR,
        first_vert=0,
        num_verts=4,
        first_index=0,
        num_indexes=6,
        plane_normal=normal,
    )
    return drawverts_data, indexes_data, surfaces_data


def build_sector_bsp(
    cell_x: int,
    cell_y: int,
    sector_size: float,
    platform_mins: Tuple[float, float, float],
    platform_maxs: Tuple[float, float, float],
    visual: bool = False,
) -> bytes:
    shaders_data = pack_shader("textures/openworld/sector_solid", 0, CONTENTS_SOLID)
    if visual:
        shaders_data += pack_shader("textures/openworld/sector_ground", 0, 0)

    planes = axial_box_planes(platform_mins, platform_maxs)
    planes_data = b"".join(pack_plane(*p) for p in planes)

    brushsides_data = b"".join(pack_brushside(i, 0) for i in range(6))
    brushes_data = pack_brush(0, 6, 0)

    entities_data = pad4(
        (
            '{{\n"classname" "worldspawn"\n'
            '"message" "sector_{cell_x}_{cell_y}"\n'
            '"openworld_cell" "{cell_x} {cell_y}"\n'
            '"openworld_size" "{sector_size:.0f}"\n'
            '}}\n'
        ).format(
            cell_x=cell_x, cell_y=cell_y, sector_size=sector_size,
        ).encode("ascii") + b"\x00"
    )

    lumps: List[Tuple[int, bytes]] = [(i, b"") for i in range(HEADER_LUMPS)]
    lumps[LUMP_ENTITIES] = (LUMP_ENTITIES, entities_data)
    lumps[LUMP_SHADERS] = (LUMP_SHADERS, shaders_data)
    lumps[LUMP_PLANES] = (LUMP_PLANES, planes_data)
    lumps[LUMP_BRUSHSIDES] = (LUMP_BRUSHSIDES, brushsides_data)
    lumps[LUMP_BRUSHES] = (LUMP_BRUSHES, brushes_data)

    if visual:
        drawverts_data, indexes_data, surfaces_data = build_visual_lumps(
            platform_mins, platform_maxs,
        )
        lumps[LUMP_DRAWVERTS] = (LUMP_DRAWVERTS, drawverts_data)
        lumps[LUMP_DRAWINDEXES] = (LUMP_DRAWINDEXES, indexes_data)
        lumps[LUMP_SURFACES] = (LUMP_SURFACES, surfaces_data)

    header_size = 8 + HEADER_LUMPS * 8
    out = bytearray(header_size)
    struct.pack_into("<I", out, 0, BSP_IDENT)
    struct.pack_into("<I", out, 4, BSP_VERSION)

    offset = header_size
    lump_payloads: List[bytes] = []
    for i in range(HEADER_LUMPS):
        _, data = lumps[i]
        d = pad4(data)
        lump_payloads.append(d)
        struct.pack_into("<II", out, 8 + i * 8, offset, len(d))
        offset += len(d)

    for d in lump_payloads:
        out.extend(d)

    return bytes(out)


def main() -> int:
    ap = argparse.ArgumentParser(description="Generate open-world sector BSP for cm_stream_merge")
    ap.add_argument("output_bsp", help="Output path, e.g. maps/sector_0_0.bsp")
    ap.add_argument("--cell-x", type=int, default=0)
    ap.add_argument("--cell-y", type=int, default=0)
    ap.add_argument("--sector-size", type=float, default=4096.0)
    ap.add_argument("--platform-min", type=float, nargs=3, default=(512.0, 512.0, 0.0))
    ap.add_argument("--platform-max", type=float, nargs=3, default=(3584.0, 3584.0, 128.0))
    ap.add_argument(
        "--visual",
        action="store_true",
        help="Emit drawVerts/surfaces for r_bspStream authored geometry",
    )
    args = ap.parse_args()

    data = build_sector_bsp(
        args.cell_x, args.cell_y, args.sector_size,
        tuple(args.platform_min), tuple(args.platform_max),
        visual=args.visual,
    )
    os.makedirs(os.path.dirname(args.output_bsp) or ".", exist_ok=True)
    with open(args.output_bsp, "wb") as f:
        f.write(data)
    mode = "visual+solid" if args.visual else "collision-only"
    print(
        f"Wrote {len(data)} bytes -> {args.output_bsp} "
        f"(1 solid brush, sector {args.cell_x},{args.cell_y}, {mode})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
