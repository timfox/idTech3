#!/usr/bin/env python3
"""Skeleton retarget mapping: source bone name -> target bone name (JSON table)."""
import argparse
import json
import sys

def main() -> int:
    p = argparse.ArgumentParser(description="Apply bone name mapping for glTF/IQM retarget")
    p.add_argument("mapping", help="JSON object: {\"source_bone\": \"target_bone\", ...}")
    p.add_argument("--list", action="store_true", help="Print mapping keys only")
    args = p.parse_args()
    with open(args.mapping, encoding="utf-8") as f:
        table = json.load(f)
    if args.list:
        for k, v in table.items():
            print(f"{k} -> {v}")
        return 0
    json.dump(table, sys.stdout, indent=2)
    print(file=sys.stderr)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
