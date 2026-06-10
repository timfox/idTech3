"""3D Huber TV (TVReg-style) and 2D TV regularization."""

from __future__ import annotations

import numpy as np


def tv_huber_3d_fast(alpha: np.ndarray, tau: float) -> float:
    def edge(a, b):
        d = b - a
        n2 = np.abs(d)
        return np.where(n2 >= tau, n2 - 0.5 * tau, (n2 * n2) / (2.0 * tau))

    tv = 0.0
    tv += edge(alpha[:-1], alpha[1:]).sum()
    tv += edge(alpha[:, :-1], alpha[:, 1:]).sum()
    tv += edge(alpha[:, :, :-1], alpha[:, :, 1:]).sum()
    return float(tv)


def tv_grad_huber_3d(alpha: np.ndarray, tau: float) -> np.ndarray:
    """Subgradient of 3D Huber TV for gradient descent."""
    grad = np.zeros_like(alpha)
    nx, ny, nz = alpha.shape

    def contrib(a, b, idx_a, idx_b):
        d = b - a
        n = abs(d)
        if n < 1e-12:
            return
        if n >= tau:
            scale = 1.0 / n
        else:
            scale = d / tau
        grad[idx_a] -= scale
        grad[idx_b] += scale

    for iz in range(nz):
        for iy in range(ny):
            for ix in range(nx):
                v = alpha[ix, iy, iz]
                if ix + 1 < nx:
                    contrib(v, alpha[ix + 1, iy, iz], (ix, iy, iz), (ix + 1, iy, iz))
                if iy + 1 < ny:
                    contrib(v, alpha[ix, iy + 1, iz], (ix, iy, iz), (ix, iy + 1, iz))
                if iz + 1 < nz:
                    contrib(v, alpha[ix, iy, iz + 1], (ix, iy, iz), (ix, iy, iz + 1))
    return grad


def tv_2d(alpha2d: np.ndarray) -> float:
    gx = np.diff(alpha2d, axis=0)
    gy = np.diff(alpha2d, axis=1)
    return float(np.sum(gx * gx) + np.sum(gy * gy))
