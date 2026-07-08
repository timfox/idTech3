#!/usr/bin/env bash
# Phase C sector stream end-to-end fidelity (collision, visual BSP, nav walkable, MP sync list).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

DATA="$ROOT/tests/data/openworld"
BASE="$DATA/base"
RELEASE="${RELEASE_DIR:-$ROOT/release}"
BUILD="${BUILD_DIR:-$ROOT/build-vk-Release}"
SERVER="$BUILD/idtech3_server"
UNIT_NAV="$BUILD/unit_openworld_nav"
GEN_HUB="$ROOT/scripts/tools/gen_hub_bsp.py"
GEN_SECTOR="$ROOT/scripts/tools/gen_sector_bsp.py"
BAKE_NAV="$ROOT/scripts/bake_openworld_nav.sh"
BOOT_PK3="$BASE/z_openworld_smoke.pk3"

echo "[test_sector_stream_fidelity] generate multi-sector fixtures..."
mkdir -p "$BASE/maps" "$BASE/nav"
python3 "$GEN_HUB" "$BASE/maps/open_void.bsp"
python3 "$GEN_SECTOR" "$BASE/maps/sector_0_0.bsp" --cell-x 0 --cell-y 0 --visual
python3 "$GEN_SECTOR" "$BASE/maps/sector_1_0.bsp" --cell-x 1 --cell-y 0 --visual
cp -f "$DATA/openworld_smoke_fidelity.cfg" "$BASE/openworld_smoke_fidelity.cfg"

if [[ ! -s "$BOOT_PK3" ]]; then
	rm -f "$BOOT_PK3"
	( cd "$BASE" && zip -9 -q "$(basename "$BOOT_PK3")" default.cfg )
fi

echo "[test_sector_stream_fidelity] nav bake + walkable probe..."
if [[ -x "$UNIT_NAV" ]] || [[ -f "$BUILD/build.ninja" ]] || [[ -f "$BUILD/Makefile" ]]; then
	"$BAKE_NAV" "$BASE/maps/sector_0_0.bsp" "$BASE/nav/sector_0_0.nav" "$BUILD"
	"$BAKE_NAV" "$BASE/maps/sector_1_0.bsp" "$BASE/nav/sector_1_0.nav" "$BUILD"
	test -f "$BASE/nav/sector_0_0.nav"
	test -f "$BASE/nav/sector_1_0.nav"
	if [[ -x "$UNIT_NAV" ]]; then
		"$UNIT_NAV" "$BASE/maps/sector_0_0.bsp" "$BASE/nav/sector_0_0.nav" 0 0 2048 2048 128
		"$UNIT_NAV" "$BASE/maps/sector_1_0.bsp" "$BASE/nav/sector_1_0.nav" 1 0 6144 2048 128
	fi
else
	echo "[test_sector_stream_fidelity] skip nav walkable probe (no build dir)"
fi

if [[ ! -x "$SERVER" ]]; then
	SERVER="$RELEASE/idtech3_server"
fi
if [[ ! -x "$SERVER" ]]; then
	echo "[test_sector_stream_fidelity] skip runtime fidelity (no idtech3_server — build engine first)"
	exit 0
fi

echo "[test_sector_stream_fidelity] dedicated runtime (collision + visual lumps + sync + stress)..."
output="$(timeout 45 "$SERVER" +set dedicated 1 +set com_hunkMegs 64 \
	+set fs_basepath "$DATA" +set fs_game "" \
	+set cm_stream 1 +set cm_streamMerge 1 +set com_openWorldSmoke 1 \
	+exec openworld_smoke_fidelity.cfg 2>&1 || true)"

echo "$output" | rg -q 'OPENWORLD_FIDELITY: OK' || {
	echo "$output" >&2
	echo "[test_sector_stream_fidelity] FAIL: expected OPENWORLD_FIDELITY: OK" >&2
	exit 1
}

echo "[test_sector_stream_fidelity] ok"
