#!/usr/bin/env python3
"""C↔Python parity for GCC-FER Table II totals."""

from __future__ import annotations

import sys

from cafer.config import NUM_CULTURES, NUM_EXPRESSIONS

# src/gccfer/gccfer_dataset.c
C_CULTURES = 4
C_EXPRESSIONS = 7
C_TOTAL = 23934
CULTURE_EXPR = [
    [1041, 299, 597, 1206, 986, 881, 791],
    [747, 257, 481, 890, 630, 908, 643],
    [844, 285, 303, 1288, 2326, 1109, 471],
    [828, 437, 255, 2040, 1977, 1163, 251],
]
EXPR_TOTALS = [3460, 1278, 1636, 5424, 5919, 4061, 2156]


def main() -> int:
    failed = 0
    if NUM_CULTURES != C_CULTURES:
        print(f"FAIL cultures: {NUM_CULTURES}")
        failed += 1
    if NUM_EXPRESSIONS != C_EXPRESSIONS:
        print(f"FAIL expressions: {NUM_EXPRESSIONS}")
        failed += 1

    table_sum = sum(sum(row) for row in CULTURE_EXPR)
    if table_sum != C_TOTAL:
        print(f"FAIL table sum {table_sum} != {C_TOTAL}")
        failed += 1

    for c, row in enumerate(CULTURE_EXPR):
        if sum(row) != sum(CULTURE_EXPR[c]):
            failed += 1
    expr_sum = sum(EXPR_TOTALS)
    if expr_sum != C_TOTAL:
        print(f"FAIL expression totals {expr_sum} != {C_TOTAL}")
        failed += 1

    if failed:
        print(f"test_parity gccfer: {failed} failed")
        return 1
    print(f"test_parity gccfer: OK (N={C_TOTAL})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
