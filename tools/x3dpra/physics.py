"""x3DPRA physics: contrast, Green's function Im(G), sparse weight matrix."""

from __future__ import annotations

import math
from typing import List

import numpy as np
from scipy import sparse

from config import C0_DB, DIPOLE_R, DOI_X, DOI_Y, DOI_Z, K0, LAMBDA0, ZETA0


def attenuation_from_permittivity(eps_r: float, eps_i: float) -> float:
    delta = eps_i / eps_r
    return 2.0 * math.pi * delta * math.sqrt(eps_r) / LAMBDA0


def contrast_from_permittivity(eps_r: float, eps_i: float) -> complex:
    srt = math.sqrt(eps_r)
    return complex(2.0 * (srt - 1.0), eps_i / srt)


def dipole_height(theta: float) -> float:
    scale = LAMBDA0 * math.sqrt(DIPOLE_R / (math.pi * ZETA0))
    return scale * math.sin(theta)


def scalar_green_im(dist: float, k0: float = K0) -> float:
    """Im(G) for scalar Helmholtz Green: sin(kr)/(4*pi*r)."""
    if dist < 1e-9:
        return 0.0
    return math.sin(k0 * dist) / (4.0 * math.pi * dist)


def incident_field(tx: np.ndarray, r: np.ndarray) -> float:
    d = r - tx
    dist = np.linalg.norm(d)
    if dist < 1e-9:
        return 1.0
    theta = math.acos(float(d[2] / dist))
    return (1.0 / dist) * dipole_height(theta)


def theta_at(rx: np.ndarray, src: np.ndarray) -> float:
    d = src - rx
    dist = np.linalg.norm(d)
    if dist < 1e-9:
        return math.pi * 0.5
    return math.acos(float(d[2] / dist))


def kernel_psi(tx: np.ndarray, rx: np.ndarray, voxel: np.ndarray, dv: float) -> float:
    """Imaginary part of Rytov kernel psi (Eq. 22-25)."""
    ei_rx = incident_field(tx, rx)
    ei_vox = incident_field(tx, voxel)
    dist = np.linalg.norm(rx - voxel)
    g_im = scalar_green_im(dist)
    h_rx = dipole_height(theta_at(rx, tx))
    h_vox = dipole_height(theta_at(rx, voxel))
    return h_vox * g_im * ei_vox * dv / (h_rx * ei_rx + 1e-12)


def weight_entry(psi_im: float, k0: float) -> float:
    """W_ln = C0 * k0^2 * Im(psi) / k0 = C0 * k0 * Im(psi) (Eq. 25)."""
    return C0_DB * k0 * psi_im


def fresnel_mask(r_mt_n: float, r_mr_n: float, r_mt_mr: float, delta_d: float) -> bool:
    return (r_mt_n + r_mr_n) < (r_mt_mr + delta_d)


def build_weight_matrix(
    nodes: np.ndarray,
    voxels: np.ndarray,
    delta_d: float = 0.2,
) -> sparse.csr_matrix:
    """Build sparse W in y = W @ delta_alpha + n (Eq. 25-27)."""
    flat = voxels.reshape(-1, 3)
    nx = voxels.shape[0]
    ny = voxels.shape[1]
    nz = voxels.shape[2]
    dx = float(voxels[1, 0, 0, 0] - voxels[0, 0, 0, 0]) if nx > 1 else DOI_X / max(nx, 1)
    dy = float(voxels[0, 1, 0, 1] - voxels[0, 0, 0, 1]) if ny > 1 else DOI_Y / max(ny, 1)
    dz = float(voxels[0, 0, 1, 2] - voxels[0, 0, 0, 2]) if nz > 1 else DOI_Z / max(nz, 1)
    dv = dx * dy * dz
    rows: List[int] = []
    cols: List[int] = []
    data: List[float] = []
    num_links = 0

    for mt in range(len(nodes)):
        for mr in range(mt + 1, len(nodes)):
            tx = nodes[mt]
            rx = nodes[mr]
            r_link = np.linalg.norm(rx - tx)
            row_vals = []
            row_cols = []
            for n, vox in enumerate(flat):
                r_mt_n = np.linalg.norm(vox - tx)
                r_mr_n = np.linalg.norm(vox - rx)
                if not fresnel_mask(r_mt_n, r_mr_n, r_link, delta_d):
                    continue
                psi_im = kernel_psi(tx, rx, vox, dv)
                w = weight_entry(psi_im, K0)
                if abs(w) > 1e-12:
                    row_vals.append(w)
                    row_cols.append(n)
            if row_vals:
                rows.extend([num_links] * len(row_vals))
                cols.extend(row_cols)
                data.extend(row_vals)
            num_links += 1

    return sparse.csr_matrix((data, (rows, cols)), shape=(num_links, flat.shape[0]))


def forward_measurements(W: sparse.csr_matrix, delta_alpha: np.ndarray) -> np.ndarray:
    return np.asarray(W @ delta_alpha).ravel()
