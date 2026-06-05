#!/usr/bin/env python3
"""
SqueezeMe linear distillation (Algorithm 1, arXiv:2412.15171v4).

Input: JSONL or NPZ export with pose vectors and CNN decoder corrective maps.
Output: .sqz pack compatible with vk_squeezeme.c (see sqz_pack_demo.py layout).

Example:
  python3 scripts/squeezeme_distill.py \\
    --poses poses.npy --correctives corr.npy --out avatars/subject.sqz
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

import numpy as np

SQZ_MAGIC = b"SQZ1"
GCS = 64
CELLS = GCS * GCS
CHANNELS = 16
POSE_BASIS = 64


def pca_basis(x: np.ndarray, n_components: int) -> tuple[np.ndarray, np.ndarray]:
    """Return mean and principal directions (rows)."""
    mean = x.mean(axis=0)
    xc = x - mean
    _, _, vt = np.linalg.svd(xc, full_matrices=False)
    k = min(n_components, vt.shape[0])
    return mean.astype(np.float32), vt[:k].astype(np.float32)


def distill(poses: np.ndarray, correctives: np.ndarray, pose_dim: int) -> bytes:
    """
    poses: (N, pose_dim)
    correctives: (N, CELLS, CHANNELS) masked CNN outputs M(D(p))
    """
    n = poses.shape[0]
    if correctives.shape != (n, CELLS, CHANNELS):
        raise ValueError(f"correctives shape {correctives.shape} != {(n, CELLS, CHANNELS)}")

    pose_mean, pose_basis = pca_basis(poses, POSE_BASIS)
    if pose_basis.shape[0] < POSE_BASIS:
        pad = np.zeros((POSE_BASIS - pose_basis.shape[0], pose_dim), dtype=np.float32)
        pose_basis = np.vstack([pose_basis, pad])

    c = np.ones((n, POSE_BASIS + 1), dtype=np.float64)
    c[:, 1:] = (poses - pose_mean) @ pose_basis.T
    y = correctives.reshape(n, -1)
    basis_c, _, _, _ = np.linalg.lstsq(c, y, rcond=None)

    pose_mean_full = np.zeros(pose_dim, dtype=np.float32)
    pose_mean_full[: pose_mean.shape[0]] = pose_mean
    pose_basis_full = np.zeros((pose_dim, POSE_BASIS), dtype=np.float32)
    pose_basis_full[: pose_mean.shape[0], : pose_basis.shape[0]] = pose_basis.T

    template = correctives.mean(axis=0).astype(np.float32)
    mask = (np.abs(template).sum(axis=1) > 1e-6).astype(np.uint8)
    weights = np.zeros((CELLS, 24), dtype=np.float32)
    weights[:, 0] = 1.0
    bind = np.zeros((CELLS, 3), dtype=np.float32)

    hdr = struct.pack(
        "<4s7I",
        SQZ_MAGIC,
        1,
        1 | 2,
        12,
        int(mask.sum() * 16),
        GCS,
        pose_dim,
        POSE_BASIS,
    )
    parts = [
        hdr,
        pose_mean_full.tobytes(),
        pose_basis_full.tobytes(),
        basis_c.T.astype(np.float32).tobytes(),
        template.tobytes(),
        mask.tobytes(),
        weights.tobytes(),
        bind.tobytes(),
    ]
    return b"".join(parts)


def main() -> int:
    ap = argparse.ArgumentParser(description="SqueezeMe PCA linear distillation")
    ap.add_argument("--poses", required=True, help="poses .npy (N, pose_dim)")
    ap.add_argument("--correctives", required=True, help="correctives .npy (N, 64, 64, 16)")
    ap.add_argument("--out", required=True, help="output .sqz path")
    ap.add_argument("--pose-dim", type=int, default=128)
    args = ap.parse_args()

    poses = np.load(args.poses)
    corr = np.load(args.correctives)
    if corr.ndim == 4 and corr.shape[1:3] == (GCS, GCS):
        corr = corr.reshape(corr.shape[0], CELLS, CHANNELS)
    blob = distill(poses.astype(np.float32), corr.astype(np.float32), args.pose_dim)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(blob)
    print(f"distilled {poses.shape[0]} frames -> {out} ({len(blob)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
