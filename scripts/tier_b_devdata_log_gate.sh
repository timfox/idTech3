#!/usr/bin/env bash
# Golden hash gate for Tier B devdata dedicated map loads.
# Compares stable filtered server log lines against golden_server_logs.tsv.
#
# Usage:
#   ./scripts/tier_b_devdata_log_gate.sh
#   UPDATE_GOLDEN=1 ./scripts/tier_b_devdata_log_gate.sh   # rewrite golden TSV
#
# Environment: GAME_BASE, RELEASE_DIR (same as renderer_regression_maps.sh)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DEV_BASE="${GAME_BASE:-$PROJECT_ROOT/docs/renderer_validation/devdata/rtest_base}"
RELEASE_DIR="${RELEASE_DIR:-$PROJECT_ROOT/release}"
GOLDEN="${GOLDEN:-$PROJECT_ROOT/docs/renderer_validation/devdata/golden_server_logs.tsv}"
SERVER="$RELEASE_DIR/idtech3_server"

# Stable lines for map-load fingerprint (avoid timestamps / frame noise).
export LOG_FILTER_RE='^(InitGame|CM_LoadMap|------- InitGame|Server: |Game version|fs_|Loading )'

if [ ! -f "$DEV_BASE/vm/qagame.qvm" ]; then
	echo "Error: devdata missing qagame.qvm at $DEV_BASE" >&2
	exit 2
fi

if [ ! -x "$SERVER" ] && [ ! -f "$SERVER" ]; then
	echo "Error: idtech3_server not found at $SERVER" >&2
	exit 2
fi

INSTALL_ROOT="$(cd "$(dirname "$DEV_BASE")" && pwd)"
BASE_NAME="$(basename "$DEV_BASE")"

filter_log() {
	grep -E "$LOG_FILTER_RE" 2>/dev/null | sed 's/\r$//' || true
}

hash_map_log() {
	local map="$1"
	local out
	out="$(timeout 45 "$SERVER" \
		+set dedicated 1 \
		+set fs_basepath "$INSTALL_ROOT" \
		+set fs_game "$BASE_NAME" \
		+set vm_game 2 \
		+set bot_enable 0 \
		+set com_hunkMegs 128 \
		+map "$map" \
		+quit 2>&1 < /dev/null || true)"
	printf '%s\n' "$out" | filter_log | sha256sum | awk '{print $1}'
}

read_golden_maps() {
	awk -F'\t' '
		/^#/ || NF < 2 { next }
		{ gsub(/^[ \t]+|[ \t]+$/, "", $1); gsub(/^[ \t]+|[ \t]+$/, "", $2); print $1 "\t" $2 }
	' "$GOLDEN"
}

if [ "${UPDATE_GOLDEN:-0}" = "1" ]; then
	{
		echo "# Tier B devdata: SHA-256 of filtered dedicated-server log lines per map."
		echo "# Regenerate: UPDATE_GOLDEN=1 ./scripts/tier_b_devdata_log_gate.sh"
		echo "# Filter: scripts/tier_b_devdata_log_gate.sh (LOG_FILTER_RE)"
		while IFS=$'\t' read -r map _; do
			[ -z "$map" ] && continue
			printf '%s\t%s\n' "$map" "$(hash_map_log "$map")"
		done < <(read_golden_maps)
	} > "${GOLDEN}.tmp"
	mv "${GOLDEN}.tmp" "$GOLDEN"
	echo "Updated golden: $GOLDEN"
	exit 0
fi

if [ ! -f "$GOLDEN" ]; then
	echo "Error: golden file missing: $GOLDEN" >&2
	exit 2
fi

echo "=== Tier B devdata log golden gate ==="
echo "  GAME_BASE=$DEV_BASE"
echo "  GOLDEN=$GOLDEN"
echo ""

FAIL=0
PASS=0

while IFS=$'\t' read -r map expected; do
	[ -z "$map" ] && continue
	got="$(hash_map_log "$map")"
	if [ "$got" = "$expected" ]; then
		PASS=$((PASS + 1))
		echo "  ✓ $map log hash"
	else
		FAIL=$((FAIL + 1))
		echo "  ✗ $map log hash mismatch" >&2
		echo "      expected: $expected" >&2
		echo "      got:      $got" >&2
		echo "      hint: UPDATE_GOLDEN=1 $0" >&2
	fi
done < <(read_golden_maps)

echo ""
if [ "$FAIL" -gt 0 ]; then
	echo "TIER B LOG GATE FAILED ($PASS ok, $FAIL bad)" >&2
	exit 1
fi

echo "TIER B LOG GATE PASSED ($PASS maps)"
