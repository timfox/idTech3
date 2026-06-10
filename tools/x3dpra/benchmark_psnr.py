#!/usr/bin/env python3
"""Section V PSNR regression against tools/x3dpra/benchmarks/psnr_targets.json."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from config import FRESNEL_DELTA_D, OptConfig
from physics import build_weight_matrix, forward_measurements
from scene import ground_truth_alpha, nodes_2d_center_plane, nodes_3d_boundary, voxel_grid
from tval3 import reconstruct_lsqr_tv
from optimize import psnr, reconstruct_admm


def run_object(kind: str, cfg: dict, defaults: dict) -> float:
    mode = cfg.get("mode", defaults.get("mode", "2d"))
    nx = int(cfg.get("nx", defaults.get("nx", 30)))
    ny = int(cfg.get("ny", defaults.get("ny", 30)))
    nz = int(cfg.get("nz", defaults.get("nz", 1 if mode == "2d" else 5)))
    gamma = float(cfg.get("gamma", defaults.get("gamma", 0.04)))
    max_iter = int(cfg.get("max_iter", defaults.get("max_iter", 40)))
    solver = cfg.get("solver", defaults.get("solver", "lsqr_tv"))

    if mode == "2d":
        voxels = voxel_grid(nx, ny, 1)
        gt = ground_truth_alpha(voxels, kind)[..., 0]
        nodes = nodes_2d_center_plane(48)
        shape = (nx, ny, 1)
    else:
        voxels = voxel_grid(nx, ny, nz)
        gt = ground_truth_alpha(voxels, kind)
        nodes = nodes_3d_boundary(16)
        shape = (nx, ny, nz)

    W = build_weight_matrix(nodes, voxels, delta_d=FRESNEL_DELTA_D)
    y = forward_measurements(W, gt.ravel())

    if solver == "lsqr_tv":
        est = reconstruct_lsqr_tv(W, y, (shape[0], shape[1]), gamma=gamma, tv_iters=max_iter)
        if mode == "3d":
            raise ValueError(f"{kind}: lsqr_tv is 2D-only; use admm for 3D")
    elif solver == "admm":
        opt = OptConfig(tv_gamma=gamma, max_iter=max_iter)
        est3 = reconstruct_admm(W, y, shape, opt)
        est = est3 if mode == "3d" else est3[..., 0]
    else:
        raise ValueError(f"unsupported solver {solver!r} for benchmark")

    return psnr(gt, est)


def main() -> int:
    parser = argparse.ArgumentParser(description="x3DPRA PSNR benchmark")
    parser.add_argument("--gamma", type=float, default=None, help="Override all object gammas")
    parser.add_argument("--quick", action="store_true", help="circle only")
    args = parser.parse_args()

    targets_path = Path(__file__).parent / "benchmarks" / "psnr_targets.json"
    targets = json.loads(targets_path.read_text())
    defaults = {"mode": "2d", "nx": 30, "ny": 30, "gamma": 0.04, "max_iter": 40, "solver": "lsqr_tv"}
    if args.gamma is not None:
        defaults["gamma"] = args.gamma

    objects = ["circle"] if args.quick else list(targets.keys())
    failed = 0

    print("[x3DPRA bench] per-object bands from psnr_targets.json")
    for obj in objects:
        cfg = dict(targets[obj])
        if args.gamma is not None:
            cfg["gamma"] = args.gamma
        score = run_object(obj, cfg, defaults)
        lo, hi = cfg.get("psnr_min", 0), cfg.get("psnr_max", 99)
        mode = cfg.get("mode", "2d")
        grid = f"{cfg.get('nx', 30)}x{cfg.get('ny', 30)}"
        if mode == "3d":
            grid += f"x{cfg.get('nz', 5)}"
        ok = lo <= score <= hi
        tag = "OK" if ok else "FAIL"
        print(
            f"  {obj} ({mode} {grid} {cfg.get('solver', 'lsqr_tv')} "
            f"gamma={cfg.get('gamma', defaults['gamma'])}): "
            f"PSNR={score:.2f} dB [{lo}, {hi}] {tag}"
        )
        if not ok:
            failed += 1

    if failed:
        return 1
    print("[x3DPRA bench] all objects within band")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
