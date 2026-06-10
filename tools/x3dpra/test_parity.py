#!/usr/bin/env python3
"""C↔Python parity checks for x3DPRA constants and physics."""

from __future__ import annotations

import math
import sys

from config import C0_DB, FRESNEL_DELTA_D, LAMBDA0
from physics import attenuation_from_permittivity, fresnel_mask, weight_entry

# Values mirrored from src/x3dpra/x3dpra.h and x3dpra_physics.c
C_LAMBDA0 = 0.125
C_C0 = 8.685889638
C_FRESNEL = 0.2


def main() -> int:
    failed = 0

    if abs(LAMBDA0 - C_LAMBDA0) > 1e-6:
        print(f"FAIL lambda0: py={LAMBDA0} c={C_LAMBDA0}")
        failed += 1
    if abs(C0_DB - C_C0) > 1e-4:
        print(f"FAIL C0: py={C0_DB} c={C_C0}")
        failed += 1
    if abs(FRESNEL_DELTA_D - C_FRESNEL) > 1e-6:
        print(f"FAIL fresnel_delta: py={FRESNEL_DELTA_D} c={C_FRESNEL}")
        failed += 1

    alpha = attenuation_from_permittivity(10.0, 1.0)
    if not (14.0 < alpha < 17.0):
        print(f"FAIL alpha eps=10+1j: {alpha}")
        failed += 1

    if not fresnel_mask(0.4, 0.4, 0.9, 0.2):
        print("FAIL fresnel inside")
        failed += 1
    if fresnel_mask(0.8, 0.8, 0.9, 0.01):
        print("FAIL fresnel outside")
        failed += 1

    k0 = 2.0 * math.pi / LAMBDA0
    w = weight_entry(1.0, k0)
    expected = C_C0 * k0 * 1.0
    if abs(w - expected) > 1e-3:
        print(f"FAIL weight_entry: got={w} expected={expected}")
        failed += 1

    if failed:
        print(f"test_parity x3dpra: {failed} failed")
        return 1
    print("test_parity x3dpra: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
