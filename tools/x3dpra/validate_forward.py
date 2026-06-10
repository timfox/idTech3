#!/usr/bin/env python3
"""Validate analytic forward model W vs frozen fixture (CST surrogate)."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np

from config import FRESNEL_DELTA_D
from physics import build_weight_matrix, forward_measurements
from scene import ground_truth_alpha, nodes_3d_boundary, voxel_grid


def main() -> int:
    parser = argparse.ArgumentParser(description="x3DPRA forward validation")
    parser.add_argument("--fixture", type=Path, default=Path("fixtures/synthetic_M3D.npz"))
    parser.add_argument("--write-fixture", action="store_true")
    parser.add_argument("--object", default="circle")
    args = parser.parse_args()

    voxels = voxel_grid(30, 30, 8)
    gt = ground_truth_alpha(voxels, args.object).ravel()
    nodes = nodes_3d_boundary(16)
    W = build_weight_matrix(nodes, voxels, delta_d=FRESNEL_DELTA_D)
    y = forward_measurements(W, gt)

    fix_path = args.fixture
    fix_path.parent.mkdir(parents=True, exist_ok=True)

    if args.write_fixture or not fix_path.is_file():
        np.savez_compressed(fix_path, y=y, object=args.object, links=W.shape[0])
        print(f"[validate_forward] wrote {fix_path} links={W.shape[0]}")
        return 0

    ref = np.load(fix_path)
    y_ref = np.asarray(ref["y"]).ravel()
    if y_ref.shape != y.shape:
        print(f"FAIL shape {y.shape} vs fixture {y_ref.shape}")
        return 1
    rel = np.linalg.norm(y - y_ref) / (np.linalg.norm(y_ref) + 1e-12)
    print(f"[validate_forward] rel_err={rel:.4e}")
    if rel > 1e-3:
        print("FAIL forward drift exceeds 1e-3 (regenerate with --write-fixture)")
        return 1
    print("[validate_forward] OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
