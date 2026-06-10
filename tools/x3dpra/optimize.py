"""Matrix-free TVReg-style 3D reconstruction (ADMM split, no W^T W materialization)."""

from __future__ import annotations

from typing import Callable, Tuple

import numpy as np
from scipy import sparse
from scipy.sparse.linalg import LinearOperator, cg

from config import OptConfig
from tvreg import tv_grad_huber_3d, tv_huber_3d_fast


def psnr(gt: np.ndarray, est: np.ndarray) -> float:
    mse = np.mean((gt - est) ** 2)
    if mse <= 1e-12:
        return 99.0
    peak = max(float(gt.max()), 1e-6)
    return 10.0 * np.log10((peak * peak) / mse)


def _matvec_wt_w(x: np.ndarray, W: sparse.csr_matrix) -> np.ndarray:
    return np.asarray(W.T @ (W @ x)).ravel()


def reconstruct_matrix_free(
    W: sparse.csr_matrix,
    y: np.ndarray,
    shape: Tuple[int, int, int],
    opt: OptConfig | None = None,
) -> np.ndarray:
    """FISTA with matrix-free data gradient via W^T(Wx-y)."""
    opt = opt or OptConfig()
    nx, ny, nz = shape
    n = nx * ny * nz
    alpha = np.zeros(n)
    t = 1.0
    z = alpha.copy()
    Wty = np.asarray(W.T @ y).ravel()

    def grad_data(vec: np.ndarray) -> np.ndarray:
        return _matvec_wt_w(vec, W) - Wty

    # Lipschitz estimate via power iteration (small n for CI)
    v = np.random.default_rng(0).standard_normal(n)
    v /= np.linalg.norm(v) + 1e-12
    for _ in range(8):
        v = _matvec_wt_w(v, W)
        nv = np.linalg.norm(v)
        if nv < 1e-12:
            break
        v /= nv
    lipschitz = float(np.dot(v, _matvec_wt_w(v, W))) + 1e-6
    step = opt.step_size / lipschitz

    for _ in range(opt.max_iter):
        grad = grad_data(z)
        a3 = z.reshape(nx, ny, nz) - step * grad.reshape(nx, ny, nz)
        grad_tv = tv_grad_huber_3d(a3, opt.huber_tau)
        a3 = a3 - step * opt.tv_gamma * grad_tv
        a3 = np.maximum(a3, 0.0)
        alpha_new = a3.ravel()
        t_new = 0.5 * (1.0 + np.sqrt(1.0 + 4.0 * t * t))
        z = alpha_new + ((t - 1.0) / t_new) * (alpha_new - alpha)
        alpha = alpha_new
        t = t_new

    return alpha.reshape(nx, ny, nz)


def reconstruct_admm(
    W: sparse.csr_matrix,
    y: np.ndarray,
    shape: Tuple[int, int, int],
    opt: OptConfig | None = None,
) -> np.ndarray:
    """Huber-TV ADMM with CG on normal equations."""
    opt = opt or OptConfig()
    nx, ny, nz = shape
    n = nx * ny * nz
    x = np.zeros(n)
    z = np.zeros(n)
    u = np.zeros(n)
    rho = 1.0
    Wty = np.asarray(W.T @ y).ravel()

    def A_matvec(v: np.ndarray) -> np.ndarray:
        return _matvec_wt_w(v, W) + rho * v

    A = LinearOperator((n, n), matvec=A_matvec, dtype=np.float64)

    for _ in range(min(opt.max_iter, 60)):
        rhs = Wty + rho * (z - u)
        x, info = cg(A, rhs, maxiter=50, rtol=1e-5)
        if info != 0:
            break
        x3 = x.reshape(nx, ny, nz)
        z_new = x3 - u.reshape(nx, ny, nz) - opt.tv_gamma / rho * tv_grad_huber_3d(x3, opt.huber_tau)
        z_new = np.maximum(z_new, 0.0)
        z = z_new.ravel()
        u = u + x - z

    return z.reshape(nx, ny, nz)


def objective(W, y, alpha_vec, shape, opt: OptConfig) -> float:
    nx, ny, nz = shape
    r = y - W @ alpha_vec
    a3 = alpha_vec.reshape(nx, ny, nz)
    return 0.5 * float(r @ r) + opt.tv_gamma * tv_huber_3d_fast(a3, opt.huber_tau)
