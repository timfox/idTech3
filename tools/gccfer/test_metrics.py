#!/usr/bin/env python3
"""Smoke test GCC-FER UAR/WAR metrics (no torch required)."""

from __future__ import annotations

import sys

import numpy as np

from metrics import unweighted_average_recall, weighted_average_recall


def main() -> int:
    # Perfect predictions → 100% UAR/WAR
    y = [0, 1, 2, 0, 1, 2]
    uar = unweighted_average_recall(y, y, 3)
    war = weighted_average_recall(y, y, 3)
    if abs(uar - 100.0) > 1e-6 or abs(war - 100.0) > 1e-6:
        print(f"FAIL perfect: UAR={uar} WAR={war}")
        return 1

    # One class always wrong → UAR drops for that class
    pred = [0, 1, 0, 0, 1, 0]
    uar_bad = unweighted_average_recall(y, pred, 3)
    if uar_bad >= 100.0:
        print(f"FAIL imperfect UAR={uar_bad}")
        return 1

    print(f"test_metrics gccfer: OK (UAR imperfect={uar_bad:.1f}%)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
