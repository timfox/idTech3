#!/usr/bin/env python3
"""RadiusFPS benchmark: synthetic point clouds, reference FPS timing."""

from __future__ import annotations

import argparse
import math
import time
from typing import List, Tuple


def reference_fps(points: List[Tuple[float, float, float]], m: int, seed: int = 1) -> List[int]:
    n = len(points)
    dist = [float("inf")] * n
    state = seed if seed else 1
    state = state * 1664525 + 1013904223
    start = state % n
    indices = [start]
    dist[start] = 0.0

    def dist3(a, b):
        return math.sqrt((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2)

    for _ in range(1, m):
        last = indices[-1]
        lp = points[last]
        for i, p in enumerate(points):
            d = dist3(p, lp)
            if d < dist[i]:
                dist[i] = d
        best_i = 0
        best_d = -1.0
        for i, d in enumerate(dist):
            if d > best_d or (d == best_d and i < best_i):
                best_d = d
                best_i = i
        indices.append(best_i)
    return indices


def grid_cloud(n: int, scale: float = 10.0) -> List[Tuple[float, float, float]]:
    pts = []
    side = max(2, int(round(n ** (1 / 3))))
    step = scale / max(side - 1, 1)
    for ix in range(side):
        for iy in range(side):
            for iz in range(side):
                if len(pts) >= n:
                    return pts
                t = step * ix
                pts.append((t, math.sin(t * 0.37) * scale * 0.1, math.cos(t * 0.19) * scale * 0.1))
    return pts


def main() -> None:
    parser = argparse.ArgumentParser(description="RadiusFPS Python benchmark")
    parser.add_argument("--points", type=int, default=4096)
    parser.add_argument("--samples", type=int, default=256)
    parser.add_argument("--quick", action="store_true", help="CI mode (<5s)")
    args = parser.parse_args()

    n = min(args.points, 512 if args.quick else args.points)
    m = min(args.samples, 64 if args.quick else args.samples)

    pts = grid_cloud(n)
    t0 = time.perf_counter()
    idx = reference_fps(pts, m)
    ms = (time.perf_counter() - t0) * 1000.0

    print(f"[RadiusFPS] reference n={n} m={m} time={ms:.2f} ms first={idx[0]} last={idx[-1]}")
    if args.quick:
        assert len(idx) == m
        print("[RadiusFPS] quick OK")
    else:
        print("[RadiusFPS] For CPU/GPU native timing use console: radiusfps_sample")


if __name__ == "__main__":
    main()
