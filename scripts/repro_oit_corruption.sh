#!/usr/bin/env bash
# Deterministic repro for r_oit rectangular/band corruption (OpenArena + Raster Ultra).
# Usage: ./scripts/repro_oit_corruption.sh [extra +args]
# After load: oit_status ; walk to water/glass doorway with weapon visible.
# Isolation: see config/repro_oit_corruption.cfg header.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ROOT}/release/idtech3"
if [[ ! -x "$BIN" ]]; then
	echo "missing $BIN — build with ./scripts/compile_engine.sh vulkan" >&2
	exit 1
fi
exec "$BIN" \
	+set fs_game openarena \
	+set sv_pure 0 \
	+exec modern_raster_ultra.cfg \
	+exec vulkan_overlay_frequency_aware.cfg \
	+exec repro_oit_corruption.cfg \
	+vid_restart keep_window \
	"$@"
