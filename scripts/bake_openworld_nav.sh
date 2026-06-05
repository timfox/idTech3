#!/usr/bin/env bash
# Offline bake: sector BSP -> Detour nav tile (CI + local authoring).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BSP="${1:-$ROOT/tests/data/openworld/maps/sector_0_0.bsp}"
NAV="${2:-$ROOT/tests/data/openworld/nav/sector_0_0.nav}"
BUILD="${3:-$ROOT/build-vk-Release}"
BIN="$BUILD/unit_openworld_nav"

python3 "$ROOT/scripts/tools/gen_sector_bsp.py" "$BSP" --cell-x 0 --cell-y 0 --visual
mkdir -p "$(dirname "$NAV")"

if [[ ! -x "$BIN" ]]; then
	echo "[bake_openworld_nav] building unit_openworld_nav..."
	cmake --build "$BUILD" --target unit_openworld_nav
fi

"$BIN" "$BSP" "$NAV"
echo "[bake_openworld_nav] ok -> $NAV"
