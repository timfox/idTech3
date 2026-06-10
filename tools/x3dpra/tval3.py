"""Minimal TVAL3-style L2-TV solver for 2D x3DPRA (Section V Fig 5)."""

from __future__ import annotations

from typing import Tuple

import numpy as np
from scipy import sparse
from scipy.sparse.linalg import LinearOperator, cg, lsqr


def _grad2(u: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
    gx = np.zeros_like(u)
    gy = np.zeros_like(u)
    gx[:-1, :] = u[1:, :] - u[:-1, :]
    gy[:, :-1] = u[:, 1:] - u[:, :-1]
    return gx, gy


def _div2(gx: np.ndarray, gy: np.ndarray) -> np.ndarray:
    out = np.zeros_like(gx)
    out[1:, :] += gx[1:, :] - gx[:-1, :]
    out[0, :] += gx[0, :]
    out[:, 1:] += gy[:, 1:] - gy[:, :-1]
    out[:, 0] += gy[:, 0]
    return out


def _shrink(g: np.ndarray, tau: float) -> np.ndarray:
    n = np.abs(g)
    return np.sign(g) * np.maximum(n - tau, 0.0)


def _matvec_wt_w(x: np.ndarray, W: sparse.csr_matrix) -> np.ndarray:
    return np.asarray(W.T @ (W @ x)).ravel()


def reconstruct_tval3(
    W: sparse.csr_matrix,
    y: np.ndarray,
    shape: Tuple[int, int],
    gamma: float = 0.001,
    max_iter: int = 120,
    mu: float = 1.0,
) -> np.ndarray:
    """Split-Bregman L2-TV on 2D grid (matrix-free data term when n > 400)."""
    nx, ny = shape
    n = nx * ny
    alpha = np.zeros(n)
    bx = np.zeros((nx, ny))
    by = np.zeros((nx, ny))
    Wty = np.asarray(W.T @ y).ravel()
    use_dense = n <= 400

    if use_dense:
        WtW = (W.T @ W).tocsr()
        A = WtW + sparse.eye(n, format="csr")
    else:
        WtW = None
        A = None

    def solve_alpha(tv_term: np.ndarray) -> np.ndarray:
        rhs = Wty + mu * tv_term.ravel()
        if use_dense:
            return spsolve_dense(A + mu * sparse.eye(n, format="csr"), rhs)
        op = LinearOperator(
            (n, n),
            matvec=lambda v: _matvec_wt_w(v, W) + mu * v,
            dtype=np.float64,
        )
        sol, _ = cg(op, rhs, maxiter=80, rtol=1e-6)
        return sol

    for _ in range(max_iter):
        a2 = alpha.reshape(nx, ny)
        dx, dy = _grad2(a2)
        sx = _shrink(dx + bx, gamma / mu)
        sy = _shrink(dy + by, gamma / mu)
        bx = bx + dx - sx
        by = by + dy - sy
        tv_term = _div2(sx, sy)
        alpha = solve_alpha(tv_term)
        alpha = np.maximum(alpha, 0.0)

    return alpha.reshape(nx, ny)


def spsolve_dense(A: sparse.csr_matrix, rhs: np.ndarray) -> np.ndarray:
    from scipy.sparse.linalg import spsolve

    return spsolve(A, rhs)


def reconstruct_lsqr_tv(
    W: sparse.csr_matrix,
    y: np.ndarray,
    shape: Tuple[int, int],
    gamma: float = 0.001,
    tv_iters: int = 40,
) -> np.ndarray:
    """LSQR data fit + few TV gradient steps (fast 2D baseline for Section V)."""
    nx, ny = shape
    sol, *_ = lsqr(W, y, atol=1e-10, btol=1e-10, iter_lim=800)
    a = np.maximum(sol.reshape(nx, ny), 0.0)
    if gamma <= 0:
        return a
    from tvreg import tv_grad_huber_3d

    for _ in range(tv_iters):
        g = tv_grad_huber_3d(a[..., None], 0.05)[..., 0]
        a = np.maximum(a - gamma * g, 0.0)
    return a
