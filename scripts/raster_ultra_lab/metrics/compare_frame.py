#!/usr/bin/env python3
"""Raster Ultra 1.11 — host-side image metrics (RMSE / PSNR / SSIM).

No new rendering techniques. Compares PNG/TGA/PPM captures.
Usage:
  compare_frame.py --ref a.png --test b.png [--rmse-max 8] [--psnr-min 28] [--ssim-min 0.92]
  compare_frame.py --ref a.png --test b.png --json out.json
"""
from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from pathlib import Path


def _load_png(path: Path):
    try:
        from PIL import Image  # type: ignore
    except ImportError:
        raise SystemExit("Pillow required for PNG: pip install Pillow (or use TGA/PPM)")
    im = Image.open(path).convert("RGB")
    return im.size[0], im.size[1], list(im.getdata())


def _load_tga(path: Path):
    data = path.read_bytes()
    if len(data) < 18:
        raise ValueError("TGA too small")
    w, h = struct.unpack_from("<HH", data, 12)
    bpp = data[16]
    if bpp not in (24, 32):
        raise ValueError(f"unsupported TGA bpp {bpp}")
    offset = 18 + data[0]
    pixels = []
    stride = bpp // 8
    for i in range(w * h):
        o = offset + i * stride
        b, g, r = data[o], data[o + 1], data[o + 2]
        pixels.append((r, g, b))
    return w, h, pixels


def _load_ppm(path: Path):
    raw = path.read_bytes()
    if not raw.startswith(b"P6"):
        raise ValueError("only binary PPM P6 supported")
    parts = raw.split(b"\n")
    # skip comments
    header = []
    i = 1
    while len(header) < 2 and i < len(parts):
        line = parts[i]
        i += 1
        if line.startswith(b"#"):
            continue
        header.append(line)
    dims = header[0].split()
    w, h = int(dims[0]), int(dims[1])
    body = b"\n".join(parts[i:])
    # maxval line may be separate
    if b"\n" in body[:16]:
        body = body.split(b"\n", 1)[1]
    pixels = [(body[j], body[j + 1], body[j + 2]) for j in range(0, w * h * 3, 3)]
    return w, h, pixels


def load_image(path: Path):
    suf = path.suffix.lower()
    if suf == ".png":
        return _load_png(path)
    if suf in (".tga", ".targa"):
        return _load_tga(path)
    if suf in (".ppm", ".pnm"):
        return _load_ppm(path)
    raise SystemExit(f"unsupported image type: {suf}")


def rmse(a, b) -> float:
    n = len(a)
    if n == 0 or n != len(b):
        return float("inf")
    acc = 0.0
    for (r1, g1, b1), (r2, g2, b2) in zip(a, b):
        acc += (r1 - r2) ** 2 + (g1 - g2) ** 2 + (b1 - b2) ** 2
    return math.sqrt(acc / (n * 3))


def psnr(rmse_v: float, peak: float = 255.0) -> float:
    if rmse_v <= 1e-12:
        return 99.0
    return 20.0 * math.log10(peak / rmse_v)


def ssim_approx(a, b, w: int, h: int) -> float:
    """Windowed luminance SSIM approximation (8x8 blocks) — no SciPy dependency."""
    if w < 8 or h < 8 or len(a) != w * h:
        # global mean/var fallback
        def lum(px):
            return 0.2126 * px[0] + 0.7152 * px[1] + 0.0722 * px[2]

        la = [lum(p) for p in a]
        lb = [lum(p) for p in b]
        n = len(la)
        ma = sum(la) / n
        mb = sum(lb) / n
        va = sum((x - ma) ** 2 for x in la) / n
        vb = sum((x - mb) ** 2 for x in lb) / n
        cov = sum((la[i] - ma) * (lb[i] - mb) for i in range(n)) / n
        c1, c2 = (0.01 * 255) ** 2, (0.03 * 255) ** 2
        return ((2 * ma * mb + c1) * (2 * cov + c2)) / ((ma * ma + mb * mb + c1) * (va + vb + c2) + 1e-12)

    def lum_at(pxs, x, y):
        r, g, b = pxs[y * w + x]
        return 0.2126 * r + 0.7152 * g + 0.0722 * b

    scores = []
    c1, c2 = (0.01 * 255) ** 2, (0.03 * 255) ** 2
    for y0 in range(0, h - 7, 8):
        for x0 in range(0, w - 7, 8):
            la, lb = [], []
            for dy in range(8):
                for dx in range(8):
                    la.append(lum_at(a, x0 + dx, y0 + dy))
                    lb.append(lum_at(b, x0 + dx, y0 + dy))
            n = 64
            ma = sum(la) / n
            mb = sum(lb) / n
            va = sum((x - ma) ** 2 for x in la) / n
            vb = sum((x - mb) ** 2 for x in lb) / n
            cov = sum((la[i] - ma) * (lb[i] - mb) for i in range(n)) / n
            scores.append(((2 * ma * mb + c1) * (2 * cov + c2)) / ((ma * ma + mb * mb + c1) * (va + vb + c2) + 1e-12))
    return sum(scores) / len(scores) if scores else 0.0


def abs_error_mean(a, b) -> float:
    n = len(a)
    if n == 0:
        return float("inf")
    acc = 0.0
    for (r1, g1, b1), (r2, g2, b2) in zip(a, b):
        acc += abs(r1 - r2) + abs(g1 - g2) + abs(b1 - b2)
    return acc / (n * 3)


def edge_error(a, b, w: int, h: int) -> float:
    """Mean absolute difference of simple horizontal/vertical gradients."""
    if w < 2 or h < 2:
        return 0.0

    def lum(px):
        return 0.2126 * px[0] + 0.7152 * px[1] + 0.0722 * px[2]

    acc = 0.0
    n = 0
    for y in range(h):
        for x in range(w - 1):
            ga = abs(lum(a[y * w + x + 1]) - lum(a[y * w + x]))
            gb = abs(lum(b[y * w + x + 1]) - lum(b[y * w + x]))
            acc += abs(ga - gb)
            n += 1
    for y in range(h - 1):
        for x in range(w):
            ga = abs(lum(a[(y + 1) * w + x]) - lum(a[y * w + x]))
            gb = abs(lum(b[(y + 1) * w + x]) - lum(b[y * w + x]))
            acc += abs(ga - gb)
            n += 1
    return acc / n if n else 0.0


def delta_e76_mean(a, b) -> float:
    """Cheap perceptual proxy: mean Euclidean RGB distance (not true Lab ΔE)."""
    n = len(a)
    if n == 0:
        return float("inf")
    acc = 0.0
    for (r1, g1, b1), (r2, g2, b2) in zip(a, b):
        acc += math.sqrt((r1 - r2) ** 2 + (g1 - g2) ** 2 + (b1 - b2) ** 2)
    return acc / n


def main() -> int:
    ap = argparse.ArgumentParser(description="Raster Ultra 1.11 image compare")
    ap.add_argument("--ref", required=True, type=Path)
    ap.add_argument("--test", required=True, type=Path)
    ap.add_argument("--rmse-max", type=float, default=12.0)
    ap.add_argument("--psnr-min", type=float, default=26.0)
    ap.add_argument("--ssim-min", type=float, default=0.90)
    ap.add_argument("--json", type=Path, default=None)
    args = ap.parse_args()

    rw, rh, rp = load_image(args.ref)
    tw, th, tp = load_image(args.test)
    if (rw, rh) != (tw, th):
        print(f"FAIL size mismatch ref={rw}x{rh} test={tw}x{th}", file=sys.stderr)
        return 2

    e = abs_error_mean(rp, tp)
    r = rmse(rp, tp)
    p = psnr(r)
    s = ssim_approx(rp, tp, rw, rh)
    edge = edge_error(rp, tp, rw, rh)
    de = delta_e76_mean(rp, tp)
    result = {
        "ref": str(args.ref),
        "test": str(args.test),
        "width": rw,
        "height": rh,
        "abs_error_mean": e,
        "rmse": r,
        "psnr": p,
        "ssim": s,
        "edge_error": edge,
        "perceptual_delta_rgb": de,
        "pass": r <= args.rmse_max and p >= args.psnr_min and s >= args.ssim_min,
        "thresholds": {
            "rmse_max": args.rmse_max,
            "psnr_min": args.psnr_min,
            "ssim_min": args.ssim_min,
        },
    }
    if args.json:
        args.json.write_text(json.dumps(result, indent=2) + "\n")
    status = "PASS" if result["pass"] else "FAIL"
    print(f"{status} RMSE={r:.3f} PSNR={p:.2f}dB SSIM={s:.4f} abs={e:.3f} edge={edge:.3f} dE~={de:.3f}")
    return 0 if result["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
