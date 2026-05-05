#!/usr/bin/env bash
# Verify examples/demo_game mod files pack into a valid idtech3_demo.pk3 layout.
# Mirrors examples/demo_game/CMakeLists.txt staging (configs + vm/ui*.so when cc is available).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
MOD="$PROJECT_ROOT/examples/demo_game/mod"
BOOT="$PROJECT_ROOT/examples/demo_game/bootstrap_media"
INTER="$PROJECT_ROOT/fonts/Inter_28pt-Regular.ttf"
STUB="$PROJECT_ROOT/examples/demo_game/native/ui_skeleton_stub.c"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

command -v cmake >/dev/null 2>&1 || fail "cmake not in PATH"
command -v unzip >/dev/null 2>&1 || fail "unzip not in PATH"

for f in \
	autoexec.cfg \
	gameinfo.txt \
	demo_features.cfg \
	demo_js.cfg \
	demo_lua.cfg \
	demo_gameplay.cfg \
	readme_demo.txt \
	scripts/js/demo_hooks.js \
	scripts/js/README.txt \
	scripts/lua/demo_hooks.lua \
	scripts/demo_bootstrap.shader; do
	[ -f "$MOD/$f" ] || fail "missing mod file: $MOD/$f"
done

for f in \
	gfx/2d/bigchars.png \
	gfx/demo/bootstrap_white.png \
	gfx/demo/bootstrap_console.png \
	gfx/demo/bootstrap_flare.png \
	gfx/demo/bootstrap_shadow.png \
	fonts/LICENSE.txt \
	fonts/demo_console_sdf.png \
	fonts/demo_console_sdf.fnt; do
	[ -f "$BOOT/$f" ] || fail "missing bootstrap media: $BOOT/$f"
done

[ -f "$INTER" ] || fail "missing repo font: $INTER"
[ -f "$STUB" ] || fail "missing UI stub: $STUB"

OUT="$(mktemp)"
STAGE="$(mktemp -d)"
cleanup() { rm -f "$OUT"; rm -rf "$STAGE"; }
trap cleanup EXIT

UI_ZIP_PATH=""
UI_DOT_ZIP_PATH=""
if command -v cc >/dev/null 2>&1; then
	ARCH="$(uname -m 2>/dev/null || echo unknown)"
	case "$ARCH" in
	x86_64|amd64) UI_SO_NAME="uix86_64.so" ;;
	aarch64|arm64) UI_SO_NAME="uiaarch64.so" ;;
	i386|i686|x86) UI_SO_NAME="ui386.so" ;;
	*) UI_SO_NAME="" ;;
	esac
	if [[ -n "$UI_SO_NAME" ]]; then
		mkdir -p "$STAGE/vm"
		cc -shared -fPIC -Wall -Wextra -pedantic -I"$PROJECT_ROOT/src" \
			-o "$STAGE/vm/$UI_SO_NAME" "$STUB" || fail "failed to compile UI stub"
		case "$ARCH" in
		x86_64|amd64) UI_DOT_NAME="ui.x86_64.so" ;;
		aarch64|arm64) UI_DOT_NAME="ui.aarch64.so" ;;
		i386|i686|x86) UI_DOT_NAME="ui.i386.so" ;;
		*) UI_DOT_NAME="" ;;
		esac
		if [[ -n "$UI_DOT_NAME" ]]; then
			cp "$STAGE/vm/$UI_SO_NAME" "$STAGE/vm/$UI_DOT_NAME"
			UI_DOT_ZIP_PATH="vm/$UI_DOT_NAME"
		fi
		UI_ZIP_PATH="vm/$UI_SO_NAME"
	fi
fi

mkdir -p "$STAGE/scripts/js"
mkdir -p "$STAGE/gfx/2d" "$STAGE/gfx/demo" "$STAGE/fonts"
cp "$MOD/autoexec.cfg" "$MOD/gameinfo.txt" "$MOD/demo_features.cfg" "$MOD/demo_js.cfg" "$MOD/demo_lua.cfg" \
	"$MOD/demo_gameplay.cfg" "$MOD/readme_demo.txt" "$STAGE/"
cp "$MOD/scripts/js/demo_hooks.js" "$MOD/scripts/js/README.txt" "$STAGE/scripts/js/"
mkdir -p "$STAGE/scripts/lua"
cp "$MOD/scripts/lua/demo_hooks.lua" "$STAGE/scripts/lua/"
cp "$MOD/scripts/demo_bootstrap.shader" "$STAGE/scripts/demo_bootstrap.shader"
cp "$BOOT/gfx/2d/bigchars.png" "$STAGE/gfx/2d/bigchars.png"
cp "$BOOT/gfx/demo/bootstrap_white.png" "$BOOT/gfx/demo/bootstrap_console.png" \
	"$BOOT/gfx/demo/bootstrap_flare.png" "$BOOT/gfx/demo/bootstrap_shadow.png" "$STAGE/gfx/demo/"
cp "$INTER" "$STAGE/fonts/Inter_28pt-Regular.ttf"
cp "$BOOT/fonts/LICENSE.txt" "$BOOT/fonts/demo_console_sdf.png" \
	"$BOOT/fonts/demo_console_sdf.fnt" "$STAGE/fonts/"

TAR_ARGS=(
	autoexec.cfg gameinfo.txt demo_features.cfg demo_js.cfg demo_lua.cfg demo_gameplay.cfg readme_demo.txt
	scripts/js/demo_hooks.js scripts/js/README.txt
	scripts/lua/demo_hooks.lua
	scripts/demo_bootstrap.shader
	gfx/2d/bigchars.png
	gfx/demo/bootstrap_white.png
	gfx/demo/bootstrap_console.png
	gfx/demo/bootstrap_flare.png
	gfx/demo/bootstrap_shadow.png
	fonts/Inter_28pt-Regular.ttf
	fonts/LICENSE.txt
	fonts/demo_console_sdf.png
	fonts/demo_console_sdf.fnt
)
if [[ -n "$UI_ZIP_PATH" ]]; then
	TAR_ARGS+=( "$UI_ZIP_PATH" "$UI_DOT_ZIP_PATH" )
fi

cmake -E chdir "$STAGE" cmake -E tar cf "$OUT" --format=zip "${TAR_ARGS[@]}"

[ -s "$OUT" ] || fail "pk3 empty"

listing="$(unzip -l "$OUT")"
for needle in \
	autoexec.cfg \
	gameinfo.txt \
	demo_features.cfg \
	demo_js.cfg \
	demo_lua.cfg \
	demo_gameplay.cfg \
	readme_demo.txt \
	scripts/js/demo_hooks.js \
	scripts/js/README.txt \
	scripts/lua/demo_hooks.lua \
	scripts/demo_bootstrap.shader \
	gfx/2d/bigchars.png \
	gfx/demo/bootstrap_white.png \
	gfx/demo/bootstrap_console.png \
	gfx/demo/bootstrap_flare.png \
	gfx/demo/bootstrap_shadow.png \
	fonts/Inter_28pt-Regular.ttf \
	fonts/LICENSE.txt \
	fonts/demo_console_sdf.png \
	fonts/demo_console_sdf.fnt; do
	if [[ "$listing" != *"$needle"* ]]; then
		fail "pk3 listing missing: $needle"
	fi
done

if [[ -n "$UI_ZIP_PATH" ]]; then
	if [[ "$listing" != *"$UI_ZIP_PATH"* ]]; then
		fail "pk3 listing missing native UI: $UI_ZIP_PATH"
	fi
	if [[ -n "${UI_DOT_ZIP_PATH:-}" ]]; then
		if [[ "$listing" != *"$UI_DOT_ZIP_PATH"* ]]; then
			fail "pk3 listing missing native UI dotted alias: $UI_DOT_ZIP_PATH"
		fi
	fi
else
	echo "SKIP: vm/ui*.so not built (no cc or unknown uname -m); CMake packaging still includes stub when demo_ui_skeleton builds"
fi

echo "OK: demo_game pk3 layout ($(wc -c < "$OUT") bytes)"