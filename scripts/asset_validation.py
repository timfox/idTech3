#!/usr/bin/env python3
"""
Asset validation helpers for the idTech3 automation pipeline.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import argparse
import sys
from pathlib import Path


def read_manifest(root: Path):
    manifest_path = root / "content" / "manifest.txt"
    if not manifest_path.exists():
        return None, f"missing manifest ({manifest_path})"
    entries = set()
    for raw in manifest_path.read_text().splitlines():
        line = raw.strip()
        if line:
            entries.add(line)
    return entries, None


def check_manifest_entries(root: Path, entries):
    errors = []
    for entry in sorted(entries):
        asset = root / entry
        if not asset.exists():
            errors.append(f"manifest entry missing on disk: {entry}")
    return errors


def check_map_outputs(root: Path, manifest_entries):
    errors = []
    map_dir = root / "content" / "maps"
    if not map_dir.is_dir():
        return [f"missing maps directory: {map_dir}"]

    for map_file in sorted(map_dir.glob("*.map")):
        map_name = map_file.stem
        for ext in ("bsp", "aas", "info"):
            artifact = map_dir / f"{map_name}.{ext}"
            if not artifact.exists():
                errors.append(f"{map_name}.{ext} missing after build")
        required_entry = f"maps/{map_name}.bsp"
        if manifest_entries is not None and required_entry not in manifest_entries:
            errors.append(f"{required_entry} missing from manifest")
    return errors


def validate_assets(root: Path):
    issues = []
    manifest_entries, manifest_err = read_manifest(root)
    if manifest_err:
        issues.append(manifest_err)
    else:
        issues.extend(check_manifest_entries(root, manifest_entries))

    issues.extend(check_map_outputs(root, manifest_entries))

    return issues


def main():
    parser = argparse.ArgumentParser(description="Run asset validation lints.")
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Repository root (default: workspace root)",
    )
    parser.add_argument(
        "command",
        choices=["validate"],
        help="Validation command to execute",
    )
    args = parser.parse_args()
    root = args.root.resolve()

    if args.command == "validate":
        issues = validate_assets(root)
        if issues:
            print("asset_validation: failed:")
            for issue in issues:
                print(f"  - {issue}")
            sys.exit(1)
        print("asset_validation: all checks passed")
        sys.exit(0)


if __name__ == "__main__":
    main()
