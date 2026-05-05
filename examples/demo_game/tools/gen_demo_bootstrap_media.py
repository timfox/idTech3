#!/usr/bin/env python3
"""
Generate tiny RGBA PNGs for idtech3_demo.pk3 so a bare base/ tree still has
HUD/console shaders, charset, and external shaders referenced at R_Init.

Regenerate after changing dimensions or layout:
  python3 examples/demo_game/tools/gen_demo_bootstrap_media.py \\
    examples/demo_game/bootstrap_media
"""
from __future__ import annotations

import struct
import sys
import zlib
from pathlib import Path


def _png_chunk(chunk_type: bytes, data: bytes) -> bytes:
    return (
        struct.pack(">I", len(data))
        + chunk_type
        + data
        + struct.pack(">I", zlib.crc32(chunk_type + data) & 0xFFFFFFFF)
    )


def write_png_rgba(path: Path, width: int, height: int, rgba_rows: list[bytes]) -> None:
    if len(rgba_rows) != height or any(len(r) != width * 4 for r in rgba_rows):
        raise ValueError("rgba_rows must be height rows of width*4 bytes")
    raw = b"".join(b"\x00" + row for row in rgba_rows)
    compressed = zlib.compress(raw, 9)
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    blob = (
        b"\x89PNG\r\n\x1a\n"
        + _png_chunk(b"IHDR", ihdr)
        + _png_chunk(b"IDAT", compressed)
        + _png_chunk(b"IEND", b"")
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(blob)


def solid_rgba(w: int, h: int, r: int, g: int, b: int, a: int) -> list[bytes]:
    px = bytes([r, g, b, a])
    row = px * w
    return [row] * h


def bigchars_atlas() -> list[bytes]:
    """256x256: 16x16 cells (Q3 SCR_DrawChar UV layout)."""
    w, h = 256, 256
    rows: list[bytes] = []
    for y in range(h):
        row = bytearray(w * 4)
        for x in range(w):
            cx = x // 16
            # light gray cell background, dark grid, white glyph-ish center dot
            br, bg, bb, ba = (220, 220, 225, 255)
            if x % 16 == 0 or y % 16 == 0:
                br, bg, bb = 40, 40, 48
            dx = abs((x % 16) - 8)
            dy = abs((y % 16) - 8)
            if dx + dy < 5:
                br, bg, bb = 30, 30, 35
            i = x * 4
            row[i : i + 4] = bytes([br, bg, bb, ba])
        rows.append(bytes(row))
    return rows


def radial_flare(w: int, h: int) -> list[bytes]:
    cx, cy = (w - 1) / 2.0, (h - 1) / 2.0
    rows: list[bytes] = []
    for y in range(h):
        row = bytearray(w * 4)
        for x in range(w):
            dx = (x - cx) / cx
            dy = (y - cy) / cy
            d = min(1.0, (dx * dx + dy * dy) ** 0.5)
            a = int(255 * (1.0 - d) ** 2)
            i = x * 4
            row[i : i + 4] = bytes([255, 255, 240, a])
        rows.append(bytes(row))
    return rows


def radial_shadow(w: int, h: int) -> list[bytes]:
    cx, cy = (w - 1) / 2.0, (h - 1) / 2.0
    rows: list[bytes] = []
    for y in range(h):
        row = bytearray(w * 4)
        for x in range(w):
            dx = (x - cx) / cx
            dy = (y - cy) / cy
            d = min(1.0, (dx * dx + dy * dy) ** 0.5)
            a = int(200 * (1.0 - d))
            i = x * 4
            row[i : i + 4] = bytes([0, 0, 0, a])
        rows.append(bytes(row))
    return rows


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: gen_demo_bootstrap_media.py <output_root>", file=sys.stderr)
        return 2
    root = Path(sys.argv[1]).resolve()
    gfx2d = root / "gfx" / "2d"
    demo = root / "gfx" / "demo"
    write_png_rgba(gfx2d / "bigchars.png", 256, 256, bigchars_atlas())
    write_png_rgba(demo / "bootstrap_white.png", 4, 4, solid_rgba(4, 4, 255, 255, 255, 255))
    write_png_rgba(
        demo / "bootstrap_console.png",
        64,
        64,
        solid_rgba(64, 64, 12, 12, 18, 220),
    )
    write_png_rgba(demo / "bootstrap_flare.png", 64, 64, radial_flare(64, 64))
    write_png_rgba(demo / "bootstrap_shadow.png", 64, 64, radial_shadow(64, 64))
    print("Wrote:", root / "gfx/2d/bigchars.png", root / "gfx/demo/*.png", sep="\n  ")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
