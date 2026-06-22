#!/usr/bin/env python3
"""Dense DK transfer parity checks for unit_dk_qsd."""

from __future__ import annotations

import math


def kernels(p: float):
    p1 = p
    p2 = p * (2.0 - p)
    w = {}
    v = {}
    for s0 in (0, 1):
        for s1 in (0, 1):
            n = s0 + s1
            pn = 0.0 if n == 0 else (p1 if n == 1 else p2)
            w[(s0, s1, 0)] = 1.0 - pn
            w[(s0, s1, 1)] = pn
    for s in (0, 1):
        pn = 0.0 if s == 0 else p1
        v[(s, 0)] = 1.0 - pn
        v[(s, 1)] = pn
    return w, v


def build_transfer(N: int, p: float):
    w, v = kernels(p)
    dim = 1 << N
    m_dim = 1 << (N - 1)
    T = [[0.0] * dim for _ in range(dim)]

    def bits(idx: int):
        return [(idx >> i) & 1 for i in range(N)]

    for xi in range(dim):
        xin = bits(xi)
        for xo in range(dim):
            xout = bits(xo)
            total = 0.0
            for m_idx in range(m_dim):
                m = [(m_idx >> k) & 1 for k in range(N - 1)]
                prob = 1.0
                for k in range(N - 1):
                    prob *= w[(xin[k], xin[k + 1], m[k])]
                prob *= v[(m[0], xout[0])]
                prob *= v[(m[N - 2], xout[N - 1])]
                for k in range(1, N - 1):
                    prob *= w[(m[k - 1], m[k], xout[k])]
                total += prob
            T[xo][xi] = total
    return T


def power_iterate(N: int, p: float, iters: int = 200):
    T = build_transfer(N, p)
    dim = 1 << N
    v = [0.0 if i == 0 else 1.0 / (dim - 1) for i in range(dim)]
    lam = 1.0
    for _ in range(iters):
        vnew = [0.0] * dim
        for j in range(dim):
            if v[j] <= 0:
                continue
            for i in range(dim):
                vnew[i] += T[i][j] * v[j]
        vnew[0] = 0.0
        z = sum(vnew)
        vnew = [x / z for x in vnew]
        lam = z
        v = vnew
    mean_n = sum(bin(i).count("1") * v[i] for i in range(dim))
    return lam, mean_n


def main() -> None:
    p = 0.60
    w, _ = kernels(p)
    lam8, n8 = power_iterate(8, p, iters=80)
    assert math.isclose(w[(1, 1, 1)], p * (2 - p), rel_tol=0, abs_tol=1e-9)
    assert 0.5 < lam8 < 0.999, lam8
    assert n8 < 25.0, n8
    print("test_parity.py: OK")


if __name__ == "__main__":
    main()
