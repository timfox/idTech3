#!/usr/bin/env python3
"""C↔Python parity checks for DaX benchmark constants."""

from __future__ import annotations

import sys

from config import (
    ANCHOR_MAGNIFICATIONS,
    EMBED_DIM,
    NUM_BENCHMARK_DATASETS,
    NUM_BENCHMARK_TASKS,
    NUM_CV_FOLDS,
    STAGE2_CROP_PAIRS,
)

C_TASKS = 161
C_DATASETS = 44
C_FOLDS = 20
C_EMBED = 1024
C_ANCHORS = (2.5, 5.0, 10.0, 20.0)
C_STAGE2 = ((512, 224), (384, 168), (768, 336))
# Category task counts from src/dax/dax_benchmark.c
C_CAT_SUM = 16 + 13 + 26 + 49 + 7 + 17 + 9 + 8 + 16


def main() -> int:
    failed = 0
    checks = [
        (NUM_BENCHMARK_TASKS, C_TASKS, "tasks"),
        (NUM_BENCHMARK_DATASETS, C_DATASETS, "datasets"),
        (NUM_CV_FOLDS, C_FOLDS, "folds"),
        (EMBED_DIM, C_EMBED, "embed"),
    ]
    for py, c, name in checks:
        if py != c:
            print(f"FAIL {name}: py={py} c={c}")
            failed += 1
    if sum(1 for _ in ANCHOR_MAGNIFICATIONS) != 4:
        failed += 1
    for a, c in zip(ANCHOR_MAGNIFICATIONS, C_ANCHORS):
        if abs(a - c) > 1e-6:
            failed += 1
    if STAGE2_CROP_PAIRS != C_STAGE2:
        failed += 1
    if C_CAT_SUM != C_TASKS:
        print(f"FAIL category sum {C_CAT_SUM} != {C_TASKS}")
        failed += 1
    if failed:
        print(f"test_parity dax: {failed} failed")
        return 1
    print("test_parity dax: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
