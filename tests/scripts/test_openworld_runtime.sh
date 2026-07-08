#!/usr/bin/env bash
# Runtime open-world smoke: load hub map, merge sector 0,0, trace platform.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

DATA="$ROOT/tests/data/openworld"
BASE="$DATA/base"
RELEASE="${RELEASE_DIR:-$ROOT/release}"
BUILD="${BUILD_DIR:-$ROOT/build-vk-Release}"
SERVER="$BUILD/idtech3_server"
if [[ ! -x "$SERVER" ]]; then
	SERVER="$RELEASE/idtech3_server"
fi
GEN_HUB="$ROOT/scripts/tools/gen_hub_bsp.py"
GEN_SECTOR="$ROOT/scripts/tools/gen_sector_bsp.py"
BAKE_NAV="$ROOT/scripts/bake_openworld_nav.sh"
BOOT_PK3="$BASE/z_openworld_smoke.pk3"

echo "[test_openworld_runtime] generate fixtures..."
mkdir -p "$BASE/maps" "$BASE/nav"
python3 "$GEN_HUB" "$BASE/maps/open_void.bsp"
python3 "$GEN_SECTOR" "$BASE/maps/sector_0_0.bsp" --cell-x 0 --cell-y 0 --visual
cp -f "$DATA/openworld_smoke.cfg" "$BASE/openworld_smoke.cfg"

if [[ ! -s "$BOOT_PK3" ]]; then
	echo "[test_openworld_runtime] create bootstrap pk3..."
	rm -f "$BOOT_PK3"
	( cd "$BASE" && zip -9 -q "$(basename "$BOOT_PK3")" default.cfg )
fi

if [[ -x "$BUILD/unit_openworld_nav" ]] || [[ -f "$BUILD/build.ninja" ]] || [[ -f "$BUILD/Makefile" ]]; then
	echo "[test_openworld_runtime] bake nav tile..."
	"$BAKE_NAV" "$BASE/maps/sector_0_0.bsp" "$BASE/nav/sector_0_0.nav" "$BUILD"
	test -f "$BASE/nav/sector_0_0.nav"
else
	echo "[test_openworld_runtime] skip nav bake (no build dir)"
fi

if [[ ! -x "$SERVER" ]]; then
	echo "[test_openworld_runtime] skip server trace (no $SERVER — build engine first)"
	exit 0
fi

echo "[test_openworld_runtime] dedicated collision smoke..."
output="$(timeout 30 "$SERVER" +set dedicated 1 +set com_hunkMegs 64 \
	+set fs_basepath "$DATA" +set fs_game "" \
	+set cm_stream 1 +set cm_streamMerge 1 +set com_openWorldSmoke 1 \
	+exec openworld_smoke.cfg 2>&1 || true)"

echo "$output" | rg -q 'OPENWORLD_SMOKE: OK' || {
	echo "$output" >&2
	echo "[test_openworld_runtime] FAIL: expected OPENWORLD_SMOKE: OK" >&2
	exit 1
}

if [[ -f "$BASE/nav/sector_0_0.nav" ]]; then
	size="$(wc -c < "$BASE/nav/sector_0_0.nav" | tr -d ' ')"
	test "$size" -gt 64
	echo "[test_openworld_runtime] nav fixture ok ($size bytes)"
fi

echo "[test_openworld_runtime] ok"
