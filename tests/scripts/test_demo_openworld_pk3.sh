#!/usr/bin/env bash
# Verify demo pk3 staging includes open-world hub/sector/nav + demo_openworld.cfg.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

MOD="$ROOT/examples/demo_game/mod"
STAGE="$(mktemp -d)"
OUT="$(mktemp)"
BUILD="${BUILD_DIR:-$ROOT/build-vk-Release}"
cleanup() { rm -rf "$STAGE" "$OUT"; }
trap cleanup EXIT

fail() { echo "[test_demo_openworld_pk3] FAIL: $*" >&2; exit 1; }

command -v unzip >/dev/null 2>&1 || fail "unzip not in PATH"
command -v python3 >/dev/null 2>&1 || fail "python3 not in PATH"

echo "[test_demo_openworld_pk3] source files..."
for f in \
	"$MOD/demo_openworld.cfg" \
	"$MOD/sprites/sector_0_0.ents" \
	"scripts/tools/gen_hub_bsp.py" \
	"scripts/tools/gen_sector_bsp.py" \
	"scripts/bake_staged_openworld_nav.sh"
do
	test -f "$f" || fail "missing $f"
done

echo "[test_demo_openworld_pk3] stage open-world assets..."
mkdir -p "$STAGE/maps" "$STAGE/nav" "$STAGE/sprites"
cp "$MOD/demo_openworld.cfg" "$STAGE/"
cp "$MOD/sprites/sector_0_0.ents" "$STAGE/sprites/"
python3 scripts/tools/gen_hub_bsp.py "$STAGE/maps/open_void.bsp"
python3 scripts/tools/gen_sector_bsp.py "$STAGE/maps/sector_0_0.bsp" --cell-x 0 --cell-y 0 --visual
bash scripts/bake_staged_openworld_nav.sh "$BUILD" "$STAGE/maps/sector_0_0.bsp" "$STAGE/nav/sector_0_0.nav"
test -f "$STAGE/nav/sector_0_0.nav" || fail "nav tile missing"

echo "[test_demo_openworld_pk3] pack zip..."
(
	cd "$STAGE"
	cmake -E tar cf "$OUT" --format=zip \
		demo_openworld.cfg \
		sprites/sector_0_0.ents \
		maps/open_void.bsp \
		maps/sector_0_0.bsp \
		nav/sector_0_0.nav
)

listing="$(unzip -l "$OUT")"
for needle in \
	demo_openworld.cfg \
	sprites/sector_0_0.ents \
	maps/open_void.bsp \
	maps/sector_0_0.bsp \
	nav/sector_0_0.nav
do
	echo "$listing" | rg -q "$needle" || fail "pk3 listing missing $needle"
done

size="$(wc -c < "$STAGE/nav/sector_0_0.nav" | tr -d ' ')"
test "$size" -gt 64 || fail "nav tile too small ($size bytes)"

echo "[test_demo_openworld_pk3] ok"

BUILT_PK3="${DEMO_PK3:-$BUILD/idtech3_demo.pk3}"
if [[ -f "$BUILT_PK3" ]]; then
	echo "[test_demo_openworld_pk3] verify built pk3: $BUILT_PK3"
	built_listing="$(unzip -l "$BUILT_PK3")"
	for needle in \
		demo_openworld.cfg \
		sprites/sector_0_0.ents \
		maps/open_void.bsp \
		maps/sector_0_0.bsp \
		nav/sector_0_0.nav
	do
		echo "$built_listing" | rg -q "$needle" || fail "built pk3 missing $needle"
	done
	echo "[test_demo_openworld_pk3] built pk3 ok"
fi
