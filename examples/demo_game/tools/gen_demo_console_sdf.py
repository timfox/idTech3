#!/usr/bin/env python3
"""
Build a BMFont-style metrics file + RGBA atlas for cl_sdf_font.c (alpha = normalized SDF).

Glyphs: ASCII 32-126 in a fixed grid. Requires Pillow, numpy, scipy.

Usage:
  python3 gen_demo_console_sdf.py <font.ttf> <output_dir>
  python3 gen_demo_console_sdf.py \\
    fonts/Inter_28pt-Regular.ttf \\
    examples/demo_game/bootstrap_media/fonts
"""
from __future__ import annotations

import math
import struct
import sys
import zlib
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy.ndimage import distance_transform_edt


def write_png_rgba(path: Path, rgba: np.ndarray) -> None:
    """rgba: HxWx4 uint8"""
    h, w, c = rgba.shape
    assert c == 4
    raw = b"".join(b"\x00" + rgba[y, :, :].tobytes() for y in range(h))
    compressed = zlib.compress(raw, 9)
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)

    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(
            ">I", zlib.crc32(tag + data) & 0xFFFFFFFF
        )

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", compressed) + chunk(b"IEND", b"")
    )


def sdf_alpha_from_mask(mask: np.ndarray, spread: float) -> np.ndarray:
    """mask: float HxW 0..1, inside glyph ~1. Returns uint8 HxW alpha 0-255 (0.5 = edge)."""
    b = mask > 0.5
    inside = distance_transform_edt(~b)
    outside = distance_transform_edt(b)
    sdf = inside.astype(np.float32) - outside.astype(np.float32)
    # map sdf in [-spread, spread] to [0,1] with edge at 0.5
    t = 0.5 + 0.5 * np.clip(sdf / spread, -1.0, 1.0)
    return (np.clip(t, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    ttf = Path(sys.argv[1]).resolve()
    out_dir = Path(sys.argv[2]).resolve()
    if not ttf.is_file():
        print(f"Missing TTF: {ttf}", file=sys.stderr)
        return 2

    cell = 128  # BMFont cell + atlas cell in final PNG
    cell_hi = 256  # internal raster / SDF resolution (downsampled to cell)
    scale = cell_hi // cell
    cols = 16
    first, last = 32, 126
    codes = list(range(first, last + 1))
    rows = int(math.ceil(len(codes) / float(cols)))
    atlas_w = cols * cell
    atlas_h = rows * cell
    atlas = np.zeros((atlas_h, atlas_w, 4), dtype=np.uint8)

    font_px = 56 * scale
    font = ImageFont.truetype(str(ttf), size=font_px)
    spread = 32.0 * float(scale)
    line_height = float(cell - 16)  # breathing room vs cell (output coords)
    base = line_height * 0.75

    fnt_lines = [
        "info face=demo_console_sdf size=56 bold=0 italic=0 charset= unicode=1 stretchH=100 smooth=1 aa=1 padding=0,0,0,0 spacing=1,1",
        f"common lineHeight={line_height:.0f} base={base:.0f} scaleW={atlas_w} scaleH={atlas_h}",
        'page id=0 file="demo_console_sdf.png"',
        f"chars count={len(codes)}",
    ]

    for idx, code in enumerate(codes):
        row, col = divmod(idx, cols)
        cx, cy = col * cell, row * cell
        sub = Image.new("L", (cell_hi, cell_hi), 0)
        dr = ImageDraw.Draw(sub)
        ch = chr(code)
        bbox = dr.textbbox((0, 0), ch, font=font)
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
        ox = (cell_hi - tw) // 2 - bbox[0]
        oy = (cell_hi - th) // 2 - bbox[1]
        dr.text((ox, oy), ch, font=font, fill=255)

        m = np.asarray(sub, dtype=np.float32) / 255.0
        alpha_hi = sdf_alpha_from_mask(m, spread)
        pil_a = Image.fromarray(alpha_hi, mode="L")
        alpha = np.asarray(
            pil_a.resize((cell, cell), Image.Resampling.LANCZOS), dtype=np.uint8
        )
        # Replicate in RGB so linear sampling still sees a distance field if alpha is ignored.
        for c in range(4):
            atlas[cy : cy + cell, cx : cx + cell, c] = alpha

        xadvance = float((tw + 6 * scale) / float(scale))
        fnt_lines.append(
            f"char id={code} x={float(cx)} y={float(cy)} width={float(cell)} height={float(cell)} "
            f"xoffset=0 yoffset=0 xadvance={xadvance:.1f}"
        )

    png_path = out_dir / "demo_console_sdf.png"
    fnt_path = out_dir / "demo_console_sdf.fnt"
    write_png_rgba(png_path, atlas)
    fnt_path.write_text("\n".join(fnt_lines) + "\n", encoding="utf-8")
    print(f"Wrote {png_path} ({atlas_w}x{atlas_h}) and {fnt_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
