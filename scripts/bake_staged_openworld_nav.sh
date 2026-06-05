#!/usr/bin/env bash
# Bake nav into demo pk3 staging (unit tool or committed fixture fallback).
set -euo pipefail

BUILD="${1:?build dir}"
BSP="${2:?sector bsp path}"
NAV="${3:?output nav path}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$BUILD/unit_openworld_nav"
FIXTURE="$ROOT/tests/data/openworld/nav/sector_0_0.nav"

mkdir -p "$(dirname "$NAV")"

if [[ -x "$BIN" ]]; then
	"$BIN" "$BSP" "$NAV"
	exit 0
fi

if [[ -f "$FIXTURE" ]]; then
	cp -f "$FIXTURE" "$NAV"
	echo "[bake_staged_openworld_nav] copied fixture -> $NAV"
	exit 0
fi

echo "[bake_staged_openworld_nav] skip (no unit_openworld_nav and no fixture)" >&2
exit 0
