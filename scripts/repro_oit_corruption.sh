#!/usr/bin/env bash
# One-command repro for r_oit glyph/block corruption (OpenArena + Raster Ultra + FA).
# Usage: ./scripts/repro_oit_corruption.sh
# After load: oit_status ; walk to water/glass doorway with weapon visible.
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
	+exec demo_oit_isolation.cfg \
	+vid_restart keep_window \
	"$@"
