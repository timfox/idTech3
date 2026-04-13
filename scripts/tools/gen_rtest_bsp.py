#!/usr/bin/env python3
"""
Emit a minimal Quake 3 IBSP v46 map for dedicated-server CM_LoadMap testing.

Collision-only: one empty world leaf (cluster 0), no brushes, one axial split node
whose children both reference that leaf. Not for client rendering.
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


def pad4(data: bytes) -> bytes:
    while len(data) % 4:
        data += b"\x00"
    return data


def pack_shader(name: str, surface_flags: int = 0, contents: int = 0) -> bytes:
    b = name.encode("ascii")[:63].ljust(64, b"\x00")
    return b + struct.pack("<II", surface_flags, contents)


def pack_plane(nx: float, ny: float, nz: float, dist: float) -> bytes:
    return struct.pack("<ffff", nx, ny, nz, dist)


def pack_node(plane_num: int, child0: int, child1: int, mins: Tuple[int, int, int], maxs: Tuple[int, int, int]) -> bytes:
    return struct.pack("<iiiiiiiii", plane_num, child0, child1, mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2])


def pack_leaf(cluster: int, area: int, mins: Tuple[int, int, int], maxs: Tuple[int, int, int],
              first_leaf_surface: int, num_leaf_surfaces: int,
              first_leaf_brush: int, num_leaf_brushes: int) -> bytes:
    return struct.pack("<iiiiiiiiiiii", cluster, area, mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2],
                       first_leaf_surface, num_leaf_surfaces, first_leaf_brush, num_leaf_brushes)


def pack_model(mins: Tuple[float, float, float], maxs: Tuple[float, float, float],
               first_face: int, num_faces: int, first_brush: int, num_brushes: int) -> bytes:
    return struct.pack("<ffffffIIII", mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2],
                       first_face, num_faces, first_brush, num_brushes)


def build_bsp(map_message: str) -> bytes:
    """Minimal hollow world: 1 plane, 1 node (both children -> leaf 0), 1 leaf, 1 shader, 0 brushes."""
    shaders_data = pack_shader("textures/rtest/caulk", 0, 0)
    planes_data = pack_plane(1.0, 0.0, 0.0, 0.0)
    leaf_mins = (-512, -512, -128)
    leaf_maxs = (512, 512, 512)
    leafs_data = pack_leaf(0, 0, leaf_mins, leaf_maxs, 0, 0, 0, 0)
    # child indices: negative = -(leaf_index + 1); leaf 0 -> -1
    nodes_data = pack_node(0, -1, -1, leaf_mins, leaf_maxs)
    models_data = pack_model(
        (-512.0, -512.0, -128.0), (512.0, 512.0, 512.0), 0, 0, 0, 0,
    )
    entities_data = pad4(
        ('{{\n"classname" "worldspawn"\n"message" "{}"\n}}\n'.format(map_message)).encode("ascii") + b"\x00"
    )

    lumps: List[Tuple[int, bytes]] = [(i, b"") for i in range(HEADER_LUMPS)]
    lumps[LUMP_ENTITIES] = (LUMP_ENTITIES, entities_data)
    lumps[LUMP_SHADERS] = (LUMP_SHADERS, shaders_data)
    lumps[LUMP_PLANES] = (LUMP_PLANES, planes_data)
    lumps[LUMP_NODES] = (LUMP_NODES, nodes_data)
    lumps[LUMP_LEAFS] = (LUMP_LEAFS, leafs_data)
    lumps[LUMP_LEAFSURFACES] = (LUMP_LEAFSURFACES, b"")
    lumps[LUMP_LEAFBRUSHES] = (LUMP_LEAFBRUSHES, b"")
    lumps[LUMP_MODELS] = (LUMP_MODELS, models_data)
    lumps[LUMP_BRUSHES] = (LUMP_BRUSHES, b"")
    lumps[LUMP_BRUSHSIDES] = (LUMP_BRUSHSIDES, b"")
    lumps[LUMP_DRAWVERTS] = (LUMP_DRAWVERTS, b"")
    lumps[LUMP_DRAWINDEXES] = (LUMP_DRAWINDEXES, b"")
    lumps[LUMP_FOGS] = (LUMP_FOGS, b"")
    lumps[LUMP_SURFACES] = (LUMP_SURFACES, b"")
    lumps[LUMP_LIGHTMAPS] = (LUMP_LIGHTMAPS, b"")
    lumps[LUMP_LIGHTGRID] = (LUMP_LIGHTGRID, b"")
    lumps[LUMP_VISIBILITY] = (LUMP_VISIBILITY, b"")

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
    ap = argparse.ArgumentParser()
    ap.add_argument("output_bsp", help="Output path, e.g. maps/rtest_parity.bsp")
    ap.add_argument("--map-message", default="rtest", help="worldspawn message string")
    args = ap.parse_args()
    data = build_bsp(args.map_message)
    os.makedirs(os.path.dirname(args.output_bsp) or ".", exist_ok=True)
    with open(args.output_bsp, "wb") as f:
        f.write(data)
    print(f"Wrote {len(data)} bytes -> {args.output_bsp}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
