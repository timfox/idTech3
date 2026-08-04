#!/usr/bin/env bash
# Runtime smoke: OpenArena map load via the compatible QVM path.
#
# The test remains overridable with Q3_STOCK_BASEPATH/Q3_STOCK_BASEGAME/
# Q3_STOCK_MAP for retail Q3 or another compatible installation.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
RELEASE="${1:-$ROOT/release}"
CLIENT="$RELEASE/idtech3"
TIMEOUT_SEC="${Q3_STOCK_SMOKE_TIMEOUT:-20}"

if [ -n "${Q3_STOCK_BASEPATH:-}" ]; then
	BASEPATH="$Q3_STOCK_BASEPATH"
else
	BASEPATH="$RELEASE"
	for candidate in \
		"${OPENARENA_BASEPATH:-}" \
		"$ROOT/../OpenArena/release" \
		"$ROOT/../openarena" \
		"/usr/share/games/openarena"; do
		if [ -n "$candidate" ] && { [ -d "$candidate/openarena" ] || [ -d "$candidate/baseoa" ]; }; then
			BASEPATH="$candidate"
			break
		fi
	done
fi

if [ ! -x "$CLIENT" ]; then
	echo "SKIP: no client at $CLIENT"
	exit 0
fi

if ! command -v timeout >/dev/null 2>&1; then
	echo "SKIP: timeout not available"
	exit 0
fi

if [ -n "${Q3_STOCK_BASEGAME:-}" ]; then
	BASEGAME="$Q3_STOCK_BASEGAME"
elif [ -d "$BASEPATH/baseoa" ]; then
	BASEGAME="baseoa"
elif [ -d "$BASEPATH/openarena" ]; then
	BASEGAME="openarena"
else
	BASEGAME="openarena"
fi
MAP="${Q3_STOCK_MAP:-dm4ish}"

if ! find "$BASEPATH" -maxdepth 3 \( -name 'pak*.pk3' -o -name '*.pk3' \) 2>/dev/null | head -1 | grep -q .; then
	echo "SKIP: no OpenArena pk3 data under $BASEPATH (set Q3_STOCK_BASEPATH or install OpenArena)"
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
		+set r_mode 3 \
		+set r_fbo 0 \
		+set r_oit 0 \
		+set r_ssao 0 \
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

	if ! grep -qE 'cl_autoGraphicsProfile: classic (baseq3|openarena)|stock (baseq3|OpenArena) mode' "$log"; then
		echo "WARN: stock map $map — classic profile log not seen (cfg may be missing from OpenArena data)"
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
