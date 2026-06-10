#!/usr/bin/env python3
"""Validate GCC-FER manifest row count, enums, and file paths."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path

from cafer.config import CULTURES, EXPRESSIONS, NUM_CULTURES, NUM_EXPRESSIONS

EXPECTED_TOTAL = 23934


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate GCC-FER manifest")
    parser.add_argument("--manifest", required=True, help="CSV manifest path")
    parser.add_argument("--expect-total", type=int, default=0, help="Expected row count (0=skip)")
    parser.add_argument("--check-files", action="store_true", help="Verify paths exist")
    args = parser.parse_args()

    path = Path(args.manifest)
    if not path.is_file():
        raise SystemExit(f"Missing manifest: {path}")

    rows = 0
    bad_culture = 0
    bad_expr = 0
    missing = 0

    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows += 1
            cult = row["culture"].strip().lower()
            expr = row["expression"].strip().lower()
            if cult not in CULTURES:
                bad_culture += 1
            if expr not in EXPRESSIONS:
                bad_expr += 1
            if args.check_files and not Path(row["path"].strip()).exists():
                missing += 1

    print(f"[GCC-FER] rows={rows} cultures={NUM_CULTURES} expressions={NUM_EXPRESSIONS}")
    if args.expect_total and rows != args.expect_total:
        raise SystemExit(f"Expected {args.expect_total} rows, got {rows}")
    if bad_culture or bad_expr:
        raise SystemExit(f"Invalid labels: culture={bad_culture} expression={bad_expr}")
    if missing:
        print(f"[GCC-FER] WARN missing files: {missing}")
    print("[GCC-FER] validate_manifest OK")


if __name__ == "__main__":
    main()
