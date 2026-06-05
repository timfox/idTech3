#!/usr/bin/env python3
"""
Emit a minimal loadable Quake 3 IBSP v46 hub map (maps/open_void.bsp).

Includes nodes/leafs/models so CM_LoadMap succeeds, plus one solid floor brush.
Sector overlays merge on top via cm_streamMerge.
"""
from __future__ import annotations

import argparse
import os
import struct
import sys
from typing import List, Tuple

from gen_sector_bsp import (
    BSP_IDENT,
    BSP_VERSION,
    CONTENTS_SOLID,
    HEADER_LUMPS,
    LUMP_BRUSHES,
    LUMP_BRUSHSIDES,
    LUMP_ENTITIES,
    LUMP_LEAFS,
    LUMP_LEAFBRUSHES,
    LUMP_LEAFSURFACES,
    LUMP_MODELS,
    LUMP_NODES,
    LUMP_PLANES,
    LUMP_SHADERS,
    axial_box_planes,
    pack_brush,
    pack_brushside,
    pack_plane,
    pack_shader,
    pad4,
)


def pack_node(plane_num: int, child0: int, child1: int,
              mins: Tuple[int, int, int], maxs: Tuple[int, int, int]) -> bytes:
    return struct.pack("<i ii 6i", plane_num, child0, child1, *mins, *maxs)


def pack_leaf(cluster: int, area: int,
              mins: Tuple[int, int, int], maxs: Tuple[int, int, int],
              first_leaf_surface: int, num_leaf_surfaces: int,
              first_leaf_brush: int, num_leaf_brushes: int) -> bytes:
    return struct.pack(
        "<ii 6i ii ii",
        cluster,
        area,
        *mins,
        *maxs,
        first_leaf_surface,
        num_leaf_surfaces,
        first_leaf_brush,
        num_leaf_brushes,
    )


def pack_model(mins: Tuple[float, float, float], maxs: Tuple[float, float, float],
               first_surface: int, num_surfaces: int,
               first_brush: int, num_brushes: int) -> bytes:
    return struct.pack("<6f 4i", *mins, *maxs, first_surface, num_surfaces, first_brush, num_brushes)


def build_hub_bsp(floor_mins: Tuple[float, float, float],
                  floor_maxs: Tuple[float, float, float]) -> bytes:
    shaders_data = pack_shader("textures/openworld/hub_floor", 0, CONTENTS_SOLID)

    brush_planes = axial_box_planes(floor_mins, floor_maxs)
    split_plane = (1.0, 0.0, 0.0, 0.0)
    planes_data = b"".join(pack_plane(*p) for p in brush_planes)
    planes_data += pack_plane(*split_plane)

    brushsides_data = b"".join(pack_brushside(i, 0) for i in range(6))
    brushes_data = pack_brush(0, 6, 0)

    imins = (int(floor_mins[0]), int(floor_mins[1]), int(floor_mins[2]))
    imaxs = (int(floor_maxs[0]), int(floor_maxs[1]), int(floor_maxs[2]))
    leafbrushes_data = struct.pack("<i", 0)
    leafs_data = (
        pack_leaf(0, 0, imins, imaxs, 0, 0, 0, 1)
        + pack_leaf(0, 0, imins, imaxs, 0, 0, 0, 1)
    )
    nodes_data = pack_node(6, -1, -2, imins, imaxs)
    models_data = pack_model(floor_mins, floor_maxs, 0, 0, 0, 1)

    entities_data = pad4(
        b'{\n"classname" "worldspawn"\n"message" "open_void"\n'
        b'"openworld_hub" "1"\n}\n\x00'
    )

    lumps: List[Tuple[int, bytes]] = [(i, b"") for i in range(HEADER_LUMPS)]
    lumps[LUMP_ENTITIES] = (LUMP_ENTITIES, entities_data)
    lumps[LUMP_SHADERS] = (LUMP_SHADERS, shaders_data)
    lumps[LUMP_PLANES] = (LUMP_PLANES, planes_data)
    lumps[LUMP_NODES] = (LUMP_NODES, nodes_data)
    lumps[LUMP_LEAFS] = (LUMP_LEAFS, leafs_data)
    lumps[LUMP_LEAFBRUSHES] = (LUMP_LEAFBRUSHES, leafbrushes_data)
    lumps[LUMP_MODELS] = (LUMP_MODELS, models_data)
    lumps[LUMP_BRUSHSIDES] = (LUMP_BRUSHSIDES, brushsides_data)
    lumps[LUMP_BRUSHES] = (LUMP_BRUSHES, brushes_data)

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
    ap = argparse.ArgumentParser(description="Generate open-world hub BSP (open_void.bsp)")
    ap.add_argument("output_bsp", nargs="?", default="maps/open_void.bsp")
    ap.add_argument("--floor-min", type=float, nargs=3, default=(-8192.0, -8192.0, -64.0))
    ap.add_argument("--floor-max", type=float, nargs=3, default=(8192.0, 8192.0, 0.0))
    args = ap.parse_args()

    data = build_hub_bsp(tuple(args.floor_min), tuple(args.floor_max))
    os.makedirs(os.path.dirname(args.output_bsp) or ".", exist_ok=True)
    with open(args.output_bsp, "wb") as f:
        f.write(data)
    print(f"Wrote {len(data)} bytes -> {args.output_bsp} (hub floor brush + BSP tree)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
