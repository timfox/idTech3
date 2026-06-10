#!/usr/bin/env python3
"""Import CST Studio Suite export into x3DPRA RSS NPZ (stub)."""

from __future__ import annotations

import argparse
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description="CST → x3DPRA measurements import")
    parser.add_argument("cst_export", help="Path to CST export file")
    parser.add_argument("--output", default="cst_measurements.npz", help="Output NPZ path")
    args = parser.parse_args()

    print(
        "[import_cst] CST import not configured in this repo.\n"
        "  Use tools/x3dpra/validate_forward.py to freeze analytic W as synthetic_M3D.npz,\n"
        "  or convert CST link powers manually to NPZ keys: delta_y_db or P_obj/P_bg.",
        file=sys.stderr,
    )
    print(f"  Requested: {args.cst_export} -> {args.output}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
