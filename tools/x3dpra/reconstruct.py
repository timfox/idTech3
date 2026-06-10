#!/usr/bin/env python3
"""x3DPRA end-to-end synthetic reconstruction (Section V)."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

from config import FRESNEL_DELTA_D, OptConfig, VOXEL_NX, VOXEL_NY, VOXEL_NZ
from rss_io import background_subtract, load_measurements_npz, load_nodes_csv
from optimize import psnr, reconstruct_admm, reconstruct_matrix_free
from physics import build_weight_matrix, forward_measurements
from scene import ground_truth_alpha, nodes_2d_center_plane, nodes_3d_boundary, voxel_grid
from tval3 import reconstruct_lsqr_tv, reconstruct_tval3


def main() -> None:
    parser = argparse.ArgumentParser(description="x3DPRA volumetric RSS reconstruction")
    parser.add_argument("--object", choices=["circle", "square", "two_cylinders"], default="circle")
    parser.add_argument("--mode", choices=["2d", "3d"], default="3d")
    parser.add_argument("--solver", choices=["tval3", "lsqr_tv", "fista", "admm"], default="")
    parser.add_argument("--nx", type=int, default=0)
    parser.add_argument("--ny", type=int, default=0)
    parser.add_argument("--nz", type=int, default=0)
    parser.add_argument("--nodes", type=Path, default=None)
    parser.add_argument("--measurements", type=Path, default=None)
    parser.add_argument("--background", type=Path, default=None)
    parser.add_argument("--noise-db", type=float, default=0.5)
    parser.add_argument("--tv-gamma", type=float, default=None)
    parser.add_argument("--max-iter", type=int, default=None)
    parser.add_argument("--psnr-floor", type=float, default=0.0, help="CI regression floor (dB)")
    parser.add_argument("--output", type=Path, default=Path("x3dpra_reconstruction.npy"))
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    print(f"[x3DPRA] mode={args.mode} object={args.object}")

    if args.dry_run:
        print("[x3DPRA] dry-run OK")
        return

    nx = args.nx or (VOXEL_NX if args.mode == "3d" else VOXEL_NX)
    ny = args.ny or (VOXEL_NY if args.mode == "3d" else VOXEL_NY)
    nz = args.nz or (VOXEL_NZ if args.mode == "3d" else 1)

    if args.mode == "2d":
        voxels = voxel_grid(nx, ny, 1)
        gt = ground_truth_alpha(voxels, args.object)[..., 0]
        nodes = load_nodes_csv(args.nodes) if args.nodes else nodes_2d_center_plane(48)
        solver = args.solver or "lsqr_tv"
    else:
        voxels = voxel_grid(nx, ny, nz)
        gt = ground_truth_alpha(voxels, args.object)
        nodes = load_nodes_csv(args.nodes) if args.nodes else nodes_3d_boundary(16)
        solver = args.solver or "admm"

    print(f"[x3DPRA] building sparse weight matrix ({len(nodes)} nodes, grid {nx}x{ny}x{nz})...")
    W = build_weight_matrix(nodes, voxels, delta_d=FRESNEL_DELTA_D)
    gt_vec = gt.ravel()

    if args.measurements:
        y, p_bg = load_measurements_npz(args.measurements)
        if args.background and p_bg is None:
            bg_data = np.load(args.background)
            p_bg = np.asarray(bg_data["P_bg"]).ravel()
            p_obj = np.asarray(bg_data["P_obj"]).ravel()
            y = background_subtract(p_obj, p_bg)
    else:
        y = forward_measurements(W, gt_vec)

    if args.noise_db > 0 and not args.measurements:
        y = y + np.random.default_rng(42).normal(0.0, args.noise_db, size=y.shape)

    opt = OptConfig()
    if args.tv_gamma is not None:
        opt.tv_gamma = args.tv_gamma
    if args.max_iter is not None:
        opt.max_iter = args.max_iter

    shape = gt.shape if gt.ndim == 3 else (gt.shape[0], gt.shape[1], 1)
    print(f"[x3DPRA] solving ({solver}): links={W.shape[0]} voxels={W.shape[1]} gamma={opt.tv_gamma}")

    if solver == "tval3":
        est2 = reconstruct_tval3(W, y, (shape[0], shape[1]), gamma=opt.tv_gamma, max_iter=opt.max_iter)
        est = est2
    elif solver == "lsqr_tv":
        est2 = reconstruct_lsqr_tv(W, y, (shape[0], shape[1]), gamma=opt.tv_gamma, tv_iters=opt.max_iter)
        est = est2
    elif solver == "admm":
        est3 = reconstruct_admm(W, y, shape, opt)
        est = est3[..., 0] if args.mode == "2d" else est3
    else:
        est3 = reconstruct_matrix_free(W, y, shape, opt)
        est = est3[..., 0] if args.mode == "2d" else est3

    score = psnr(gt, est)
    print(f"[x3DPRA] PSNR={score:.2f} dB | max alpha est={est.max():.2f} gt={gt.max():.2f}")

    targets_path = Path(__file__).resolve().parent / "benchmarks" / "psnr_targets.json"
    if targets_path.is_file():
        targets = json.loads(targets_path.read_text())
        band = targets.get(args.object)
        if band:
            lo, hi = band["psnr_min"], band["psnr_max"]
            if score < lo or score > hi:
                print(f"[x3DPRA] WARN PSNR outside band [{lo}, {hi}]")

    if args.psnr_floor > 0 and score < args.psnr_floor:
        raise SystemExit(f"PSNR {score:.2f} below floor {args.psnr_floor}")

    np.save(args.output, est)
    print(f"[x3DPRA] saved {args.output}")


if __name__ == "__main__":
    main()
