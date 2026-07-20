#!/usr/bin/env python3
"""Raster Ultra 1.11 — artifact detectors on captures (black/solid/NaN proxies)."""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

# Reuse loader from compare_frame
sys.path.insert(0, str(Path(__file__).resolve().parent))
from compare_frame import load_image  # noqa: E402


def detect(path: Path) -> dict:
    w, h, pixels = load_image(path)
    n = len(pixels)
    if n == 0:
        return {"path": str(path), "artifacts": ["empty"], "pass": False}

    # Flat / solid-color
    sample = pixels[:: max(1, n // 4096)]
    uniq = len(set(sample))
    lum = [0.2126 * r + 0.7152 * g + 0.0722 * b for r, g, b in sample]
    mean_l = sum(lum) / len(lum)
    black = mean_l < 2.0
    white = mean_l > 250.0
    solid = uniq <= 4

    # Horizontal band heuristic: row means variance spike
    row_means = []
    for y in range(0, h, max(1, h // 64)):
        row = pixels[y * w : (y + 1) * w]
        if not row:
            continue
        row_means.append(sum(0.2126 * r + 0.7152 * g + 0.0722 * b for r, g, b in row) / len(row))
    band = False
    if len(row_means) > 4:
        diffs = [abs(row_means[i] - row_means[i - 1]) for i in range(1, len(row_means))]
        band = max(diffs) > 40.0 and (sum(diffs) / len(diffs)) < 8.0

    # Checkerboard corruption: high alternating energy
    checker = False
    if w > 4 and h > 4:
        alt = 0
        for y in range(0, min(h, 64), 2):
            for x in range(0, min(w, 64), 2):
                p0 = pixels[y * w + x]
                p1 = pixels[y * w + x + 1]
                alt += abs(p0[0] - p1[0]) + abs(p0[1] - p1[1]) + abs(p0[2] - p1[2])
        checker = alt / (32 * 32) > 120.0

    # Overexposure / black crush (histogram extremes)
    crushed = sum(1 for v in lum if v < 5.0) / len(lum)
    blown = sum(1 for v in lum if v > 250.0) / len(lum)

    artifacts = []
    if black:
        artifacts.append("black_frame")
    if white:
        artifacts.append("solid_white")
    if solid and not black and not white:
        artifacts.append("solid_color")
    if band:
        artifacts.append("horizontal_bands")
    if checker:
        artifacts.append("checkerboard_corruption")
    if crushed > 0.85 and not black:
        artifacts.append("black_crush")
    if blown > 0.85 and not white:
        artifacts.append("overexposure")

    return {
        "path": str(path),
        "width": w,
        "height": h,
        "mean_luminance": mean_l,
        "unique_sample_colors": uniq,
        "artifacts": artifacts,
        "pass": len(artifacts) == 0,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("images", nargs="+", type=Path)
    ap.add_argument("--json", type=Path, default=None)
    args = ap.parse_args()
    results = [detect(p) for p in args.images]
    ok = all(r["pass"] for r in results)
    if args.json:
        args.json.write_text(json.dumps({"results": results, "pass": ok}, indent=2) + "\n")
    for r in results:
        print(f"{'PASS' if r['pass'] else 'FAIL'} {r['path']}: {r['artifacts'] or 'clean'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
