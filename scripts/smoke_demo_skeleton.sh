#!/usr/bin/env bash
# Quick check: demo playfield loads through renderer init (needs display + GPU for Vulkan client).
# Usage: ./scripts/smoke_demo_skeleton.sh [PLAYFIELD_DIR]
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLAY="${1:-$ROOT/examples/demo_skeleton}"
LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT
IDTECH3_USE_RUN_VULKAN="${IDTECH3_USE_RUN_VULKAN:-0}"
export IDTECH3_USE_RUN_VULKAN
if ! "$ROOT/examples/demo_skeleton/run_demo_client.sh" "$PLAY" +set com_introPlayed 1 +quit >"$LOG" 2>&1; then
	echo "FAIL: demo client exited non-zero" >&2
	tail -40 "$LOG" >&2
	exit 1
fi
if ! grep -qF 'finished R_Init' "$LOG"; then
	echo "FAIL: renderer did not finish R_Init" >&2
	tail -60 "$LOG" >&2
	exit 1
fi
if grep -qF 'WARNING: no shader files found' "$LOG"; then
	echo "FAIL: no shader files (bootstrap pack missing?)" >&2
	exit 1
fi
if grep -qF 'RE_RegisterFont: Unable to read font' "$LOG"; then
	echo "FAIL: Inter font missing from idtech3_demo.pk3" >&2
	exit 1
fi
echo "OK: demo skeleton smoke (R_Init complete, bootstrap shaders/fonts present)"
