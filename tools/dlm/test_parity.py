#!/usr/bin/env python3
"""Parity checks for Fink deep-layered machine q(n) (Table I)."""

from __future__ import annotations

import math


def binom(n: int, r: int) -> float:
    if r < 0 or r > n:
        return 0.0
    num = 1.0
    den = 1.0
    for i in range(r):
        num *= n - i
        den *= i + 1
    return num / den


def transition(k: int) -> list[list[float]]:
    ell = 1 << k
    dim = ell + 1
    denom = float(ell**ell)
    A = [[0.0] * dim for _ in range(dim)]
    for i in range(dim):
        for j in range(dim):
            if i == 0 and j == 0:
                ipow = 1.0
            elif i == 0 or j == 0:
                ipow = 0.0 if j > 0 and i == 0 else (1.0 if j == 0 else 0.0)
            else:
                ipow = float(i) ** j
            if ell - i == 0 and ell - j == 0:
                tail = 1.0
            elif ell - j == 0:
                tail = float(ell - i) ** (ell - j)
            elif ell - i == 0:
                tail = 0.0 if ell - j > 0 else 1.0
            else:
                tail = float(ell - i) ** (ell - j)
            if i == 0 and j == 0:
                tail = float(ell) ** ell
                ipow = 1.0
            if i == 0 and j > 0:
                ipow = 0.0
                tail = float(ell) ** (ell - j) if ell - j >= 0 else 0.0
            if i > 0 and j == 0:
                ipow = 1.0
                tail = float(ell - i) ** ell
            A[i][j] = binom(ell, j) * (float(i) ** j if not (i == 0 and j == 0) else 1.0) * (
                float(ell - i) ** (ell - j) if not (i == 0 and j == 0) else float(ell) ** ell
            ) / denom
            if i == 0 and j == 0:
                A[i][j] = binom(ell, 0) * 1.0 * float(ell) ** ell / denom
            elif i == 0:
                A[i][j] = 0.0
            elif j == 0:
                A[i][j] = binom(ell, 0) * 1.0 * float(ell - i) ** ell / denom
            else:
                A[i][j] = binom(ell, j) * float(i) ** j * float(ell - i) ** (ell - j) / denom
    return A


def q_initial(k: int) -> list[float]:
    ell = 1 << k
    denom = float(2 ** (2**k))
    return [binom(ell, w) / denom for w in range(ell + 1)]


def evolve(k: int, depth: int) -> list[float]:
    A = transition(k)
    q = q_initial(k)
    for _ in range(depth - 1):
        q = [sum(A[i][j] * q[i] for i in range(len(q))) for j in range(len(q))]
    return q


def main() -> None:
    q12 = evolve(1, 2)
    assert math.isclose(q12[0], 6 / 16, rel_tol=0, abs_tol=1e-6)
    assert math.isclose(q12[1], 4 / 16, rel_tol=0, abs_tol=1e-6)
    assert math.isclose(q12[2], 6 / 16, rel_tol=0, abs_tol=1e-6)

    q22 = evolve(2, 2)
    assert math.isclose(q22[0], 680 / 4096, rel_tol=0, abs_tol=1e-6)
    assert math.isclose(q22[4], 680 / 4096, rel_tol=0, abs_tol=1e-6)

    lam2 = (4 * 3) / (4**2)
    assert math.isclose(lam2, 0.75, rel_tol=0, abs_tol=1e-9)
    print("test_parity.py: OK")


if __name__ == "__main__":
    main()
