#!/usr/bin/env bash
# Verify examples/demo_game mod files pack into a valid idtech3_demo.pk3 layout.
# Uses cmake -E tar (same layout as examples/demo_game/CMakeLists.txt — keep lists in sync).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
MOD="$PROJECT_ROOT/examples/demo_game/mod"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

command -v cmake >/dev/null 2>&1 || fail "cmake not in PATH"
command -v unzip >/dev/null 2>&1 || fail "unzip not in PATH"

for f in \
	autoexec.cfg \
	demo_features.cfg \
	demo_js.cfg \
	demo_gameplay.cfg \
	readme_demo.txt \
	scripts/js/demo_hooks.js \
	scripts/js/README.txt; do
	[ -f "$MOD/$f" ] || fail "missing mod file: $MOD/$f"
done

OUT="$(mktemp)"
cleanup() { rm -f "$OUT"; }
trap cleanup EXIT

cmake -E chdir "$MOD" cmake -E tar cf "$OUT" --format=zip \
	autoexec.cfg demo_features.cfg demo_js.cfg demo_gameplay.cfg readme_demo.txt \
	scripts/js/demo_hooks.js scripts/js/README.txt

[ -s "$OUT" ] || fail "pk3 empty"

listing="$(unzip -l "$OUT")"
for needle in \
	autoexec.cfg \
	demo_features.cfg \
	demo_js.cfg \
	demo_gameplay.cfg \
	readme_demo.txt \
	scripts/js/demo_hooks.js \
	scripts/js/README.txt; do
	if [[ "$listing" != *"$needle"* ]]; then
		fail "pk3 listing missing: $needle"
	fi
done

echo "OK: demo_game pk3 layout ($(wc -c < "$OUT") bytes)"
