"""Ground-truth attenuation volumes and transceiver layouts (Section V)."""

from __future__ import annotations

import math
from typing import Literal

import numpy as np

from config import DOI_X, DOI_Y, DOI_Z, OBJECTS, VOXEL_NX, VOXEL_NY, VOXEL_NZ


def voxel_grid(nx=VOXEL_NX, ny=VOXEL_NY, nz=VOXEL_NZ) -> np.ndarray:
    xs = np.linspace(-DOI_X / 2, DOI_X / 2, nx, endpoint=False) + DOI_X / (2 * nx)
    ys = np.linspace(-DOI_Y / 2, DOI_Y / 2, ny, endpoint=False) + DOI_Y / (2 * ny)
    zs = np.linspace(-DOI_Z / 2, DOI_Z / 2, nz, endpoint=False) + DOI_Z / (2 * nz)
    return np.stack(np.meshgrid(xs, ys, zs, indexing="ij"), axis=-1)


def ground_truth_alpha(
    voxels: np.ndarray,
    kind: Literal["circle", "square", "two_cylinders"],
) -> np.ndarray:
    spec = OBJECTS[kind]
    alpha = np.zeros(voxels.shape[:-1], dtype=np.float64)
    x, y, z = voxels[..., 0], voxels[..., 1], voxels[..., 2]

    if kind == "circle":
        mask = (x**2 + y**2 <= 0.15**2) & (np.abs(z) <= spec["height"] / 2)
        alpha[mask] = spec["alpha"]
    elif kind == "square":
        mask = (np.abs(x) <= 0.18) & (np.abs(y) <= 0.18) & (np.abs(z) <= spec["height"] / 2)
        alpha[mask] = spec["alpha"]
    elif kind == "two_cylinders":
        cyl = x**2 + y**2 <= 0.2**2
        bottom = cyl & (z >= -0.325) & (z <= -0.075)
        top = cyl & (z >= 0.075) & (z <= 0.325)
        alpha[bottom | top] = spec["alpha"]
    return alpha


def nodes_2d_center_plane(num: int = 48) -> np.ndarray:
    angles = np.linspace(0, 2 * math.pi, num, endpoint=False)
    r = DOI_X / 2
    nodes = np.zeros((num, 3))
    nodes[:, 0] = r * np.cos(angles)
    nodes[:, 1] = r * np.sin(angles)
    return nodes


def nodes_3d_boundary(num_per_height: int = 16) -> np.ndarray:
    heights = [0.0, -0.15, 0.15]
    nodes = []
    for z in heights:
        ring = nodes_2d_center_plane(num_per_height)
        ring[:, 2] = z
        nodes.append(ring)
    return np.vstack(nodes)
