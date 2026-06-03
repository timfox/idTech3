#!/usr/bin/env python3
"""Write a minimal NanoVDB grid for manual vdb_load testing (blind 2^3 or single float leaf 8^3)."""
import argparse
import struct
from pathlib import Path

MAGIC_GRID = 0x314244566f6e614E
GRID, TREE, ROOT, META = 672, 64, 64, 288
LEAF_FLOAT = 2144


def wr(buf: bytearray, off: int, fmt: str, *vals) -> None:
    struct.pack_into(fmt, buf, off, *vals)


def write_blind(buf: bytearray) -> None:
    floats = 8
    total = GRID + TREE + ROOT + META + floats * 4
    wr(buf, 0, "<Q", MAGIC_GRID)
    wr(buf, 28, "<I", 1)
    wr(buf, 32, "<Q", total)
    wr(buf, 560, "<6d", 0, 0, 0, 2, 2, 2)
    wr(buf, 608, "<3d", 1, 1, 1)
    wr(buf, 636, "<I", 1)
    wr(buf, 640, "<q", GRID + TREE + ROOT)
    wr(buf, 648, "<I", 1)
    tree = GRID
    wr(buf, tree + 24, "<q", TREE)
    wr(buf, tree + 56, "<Q", 8)
    root = GRID + TREE
    wr(buf, root + 0, "<6i", 0, 0, 0, 1, 1, 1)
    meta = GRID + TREE + ROOT
    wr(buf, meta + 0, "<q", META)
    wr(buf, meta + 8, "<Q", floats)
    wr(buf, meta + 16, "<I", 4)
    wr(buf, meta + 20, "<I", 11)
    wr(buf, meta + 28, "<I", 1)
    data = meta + META
    for i in range(floats):
        wr(buf, data + i * 4, "<f", float(i + 1))


def write_leaf(buf: bytearray) -> None:
    total = GRID + TREE + ROOT + LEAF_FLOAT
    wr(buf, 0, "<Q", MAGIC_GRID)
    wr(buf, 28, "<I", 1)
    wr(buf, 32, "<Q", total)
    wr(buf, 560, "<6d", 0, 0, 0, 8, 8, 8)
    wr(buf, 608, "<3d", 1, 1, 1)
    wr(buf, 636, "<I", 1)
    wr(buf, 640, "<Q", total)
    wr(buf, 648, "<I", 0)
    tree = GRID
    wr(buf, tree + 0, "<q", TREE + ROOT)
    wr(buf, tree + 8, "<q", TREE + ROOT + LEAF_FLOAT)
    wr(buf, tree + 24, "<q", TREE)
    wr(buf, tree + 32, "<I", 1)
    wr(buf, tree + 56, "<Q", 1)
    root = GRID + TREE
    wr(buf, root + 0, "<6i", 0, 0, 0, 7, 7, 7)
    leaf = GRID + TREE + ROOT
    wr(buf, leaf + 0, "<3i", 0, 0, 0)
    wr(buf, leaf + 16, "<Q", 1)
    wr(buf, leaf + 96, "<f", 42.0)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("output", nargs="?", default="tests/data/fog_2cubed.nvdb")
    ap.add_argument("--leaf", action="store_true", help="single 8^3 float leaf with one active voxel")
    args = ap.parse_args()
    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    if args.leaf:
        buf = bytearray(GRID + TREE + ROOT + LEAF_FLOAT)
        write_leaf(buf)
        kind = "leaf 8^3"
    else:
        buf = bytearray(GRID + TREE + ROOT + META + 8 * 4)
        write_blind(buf)
        kind = "blind 2^3"
    out.write_bytes(buf)
    print(f"wrote {out} ({len(buf)} bytes, {kind})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
