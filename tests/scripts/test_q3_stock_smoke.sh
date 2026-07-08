#!/usr/bin/env bash
# Runtime smoke: stock Quake III Arena map load via retail cgame.qvm (needs baseq3 pk3s).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
RELEASE="${1:-$ROOT/release}"
CLIENT="$RELEASE/idtech3"
TIMEOUT_SEC="${Q3_STOCK_SMOKE_TIMEOUT:-20}"
BASEPATH="${Q3_STOCK_BASEPATH:-$RELEASE}"

if [ ! -x "$CLIENT" ]; then
	echo "SKIP: no client at $CLIENT"
	exit 0
fi

if ! command -v timeout >/dev/null 2>&1; then
	echo "SKIP: timeout not available"
	exit 0
fi

BASEGAME="${Q3_STOCK_BASEGAME:-baseq3}"
MAP="${Q3_STOCK_MAP:-q3dm1}"

if ! find "$BASEPATH" -maxdepth 3 \( -name 'pak*.pk3' -o -name '*.pk3' \) 2>/dev/null | head -1 | grep -q .; then
	echo "SKIP: no retail pk3 data under $BASEPATH (set Q3_STOCK_BASEPATH or install baseq3)"
	exit 0
fi

run_map() {
	local map="$1"
	local log
	log="$(mktemp)"
	set +e
	timeout "$TIMEOUT_SEC" "$CLIENT" \
		+set fs_basepath "$BASEPATH" \
		+set fs_basegame "$BASEGAME" \
		+set sv_pure 0 \
		+set r_fullscreen 0 \
		+set com_idleSleep 0 \
		+set cl_autoGraphicsProfile 1 \
		+set activeAction "+forward; +attack" \
		+map "$map" >"$log" 2>&1
	local rc=$?
	set -e

	if grep -qiE 'Client/Server game mismatch|program tried to (read|write) out of data segment|program tried to bypass data segment|ERR_DROP' "$log"; then
		echo "FAIL: stock map $map — fatal error in log"
		grep -iE 'Client/Server game mismatch|program tried to|ERR_DROP' "$log" | head -5
		rm -f "$log"
		return 1
	fi

	if ! grep -qE 'cl_autoGraphicsProfile: classic baseq3|stock baseq3 mode' "$log"; then
		echo "WARN: stock map $map — classic profile log not seen (cfg may be missing from base/)"
	fi

	if ! grep -q 'spawn CM AABB' "$log"; then
		echo "WARN: stock map $map — spawn CM validation log not seen"
	fi
	if grep -q 'spawn CM AABB.*inside=NO' "$log" || grep -q 'merged_sectors=[1-9]' "$log"; then
		echo "FAIL: stock map $map — spawn outside CM or sector overlay active"
		grep -E 'spawn CM|merged_sectors|WARNING: stock spawn' "$log" | head -5
		rm -f "$log"
		return 1
	fi

	if ! grep -q 'spawn origin' "$log"; then
		echo "FAIL: stock map $map — no spawn origin log (rc=$rc)"
		tail -20 "$log"
		rm -f "$log"
		return 1
	fi

	echo "PASS: stock map $map loaded ($(grep 'spawn origin' "$log" | tail -1))"
	rm -f "$log"
	return 0
}

failures=0
MAPS="$MAP"
if [ "${Q3_STOCK_EXTRA_MAPS:-0}" = "1" ]; then
	MAPS="$MAP q3tourney1"
fi
for map in $MAPS; do
	if ! run_map "$map"; then
		failures=$((failures + 1))
	fi
done

if [ "$failures" -ne 0 ]; then
	echo "$failures stock map smoke test(s) failed"
	exit 1
fi

echo "test_q3_stock_smoke: passed"
