#!/usr/bin/env python3
"""
Derived Data Cache helpers for map builds.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import argparse
import hashlib
import json
import sys
from datetime import datetime
from pathlib import Path


CACHE_FILE = Path(__file__).resolve().parents[1] / "content" / "ddc" / "map-cache.json"
MAP_OUTPUT_EXTS = ("bsp", "aas", "info")


def load_cache():
    if not CACHE_FILE.exists():
        return {"maps": {}}
    try:
        return json.loads(CACHE_FILE.read_text())
    except json.JSONDecodeError:
        return {"maps": {}}


def persist_cache(cache):
    CACHE_FILE.parent.mkdir(parents=True, exist_ok=True)
    CACHE_FILE.write_text(json.dumps(cache, indent=2))


def compute_hash(path: Path):
    hasher = hashlib.sha256()
    with path.open("rb") as fh:
        while chunk := fh.read(8192):
            hasher.update(chunk)
    return hasher.hexdigest()


def needs_rebuild(root: Path, map_name: str) -> int:
    map_dir = root / "content" / "maps"
    map_file = map_dir / f"{map_name}.map"
    if not map_file.exists():
        print(f"asset_ddc: source map missing: {map_file}", file=sys.stderr)
        return 2

    cache = load_cache()
    entry = cache.get("maps", {}).get(map_name)
    current_hash = compute_hash(map_file)
    if not entry or entry.get("hash") != current_hash:
        print(f"asset_ddc: {map_name} needs rebuild (map changed)", file=sys.stderr)
        return 0

    for ext in MAP_OUTPUT_EXTS:
        output = map_dir / f"{map_name}.{ext}"
        if not output.exists():
            print(f"asset_ddc: {map_name}.{ext} missing, rebuilding", file=sys.stderr)
            return 0

    print(f"asset_ddc: {map_name} is cached and up-to-date")
    return 1


def update_entry(root: Path, map_name: str) -> int:
    map_dir = root / "content" / "maps"
    map_file = map_dir / f"{map_name}.map"
    if not map_file.exists():
        print(f"asset_ddc: cannot update missing {map_file}", file=sys.stderr)
        return 1

    cache = load_cache()
    entry = {
        "hash": compute_hash(map_file),
        "outputs": {},
        "updated_at": datetime.utcnow().isoformat() + "Z",
    }
    for ext in MAP_OUTPUT_EXTS:
        output = map_dir / f"{map_name}.{ext}"
        if output.exists():
            entry["outputs"][ext] = output.stat().st_size

    cache.setdefault("maps", {})[map_name] = entry
    persist_cache(cache)
    print(f"asset_ddc: stored cache entry for {map_name}")
    return 0


def show_status():
    cache = load_cache()
    entries = cache.get("maps", {})
    print(f"asset_ddc: {len(entries)} map(s) tracked in cache")
    for name, meta in sorted(entries.items()):
        ts = meta.get("updated_at", "unknown")
        print(f"  - {name} (updated {ts})")
    return 0


def main():
    parser = argparse.ArgumentParser(description="Simple DDC bridge for maps")
    parser.add_argument(
        "command",
        choices=["needs-rebuild", "update", "status"],
        help="DDC operation",
    )
    parser.add_argument(
        "map",
        nargs="?",
        help="Map name (without extension) for needs-rebuild/update",
    )
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]

    if args.command == "status":
        sys.exit(show_status())

    if not args.map:
        parser.error("map name required for this command")

    if args.command == "needs-rebuild":
        sys.exit(needs_rebuild(root, args.map))
    elif args.command == "update":
        sys.exit(update_entry(root, args.map))


if __name__ == "__main__":
    main()
