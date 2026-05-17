#!/usr/bin/env bash
# Regression test for Vulkan vegetation-wind dispatch placement.
# Ensures compute runs before draw for SURF_VEGETATION batches in RB_EndSurface.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TR_SHADE_FILE="${1:-$PROJECT_ROOT/src/renderers/vulkan/tr_shade.c}"
VK_FRAME_SUBMIT_FILE="${2:-$PROJECT_ROOT/src/renderers/vulkan/vk_frame_submit.c}"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

assert_file() {
	local file="$1"
	[ -f "$file" ] || fail "missing file: $file"
}

assert_file "$TR_SHADE_FILE"
assert_file "$VK_FRAME_SUBMIT_FILE"

if ! rg -n -U --multiline-dotall \
	'if\s*\(\s*PostFX_VegWind_IsEnabled\(\)\s*&&\s*tess\.shader\s*&&\s*\(\s*tess\.shader->surfaceFlags\s*&\s*SURF_VEGETATION\s*\)\s*\)\s*\{\s*vk_vegetation_wind_prepare_draw\(\);\s*\}' \
	"$TR_SHADE_FILE" >/dev/null; then
	fail "missing guarded vk_vegetation_wind_prepare_draw() block in tr_shade.c"
fi

prepare_count="$(rg -n 'vk_vegetation_wind_prepare_draw\s*\(' "$TR_SHADE_FILE" | wc -l | tr -d '[:space:]')"
if [ "$prepare_count" != "1" ]; then
	fail "expected exactly one vk_vegetation_wind_prepare_draw() in tr_shade.c, got $prepare_count"
fi

prepare_line="$(rg -n 'vk_vegetation_wind_prepare_draw\s*\(' "$TR_SHADE_FILE" | cut -d: -f1)"
draw_line="$(rg -n 'optimalStageIteratorFunc\s*\(' "$TR_SHADE_FILE" | head -1 | cut -d: -f1)"
if [ -z "$draw_line" ] || [ "$prepare_line" -ge "$draw_line" ]; then
	fail "expected vk_vegetation_wind_prepare_draw() before optimalStageIteratorFunc() (prepare=$prepare_line draw=$draw_line)"
fi

if rg -n 'vk_vegetation_wind_prepare_draw\s*\(' "$VK_FRAME_SUBMIT_FILE" >/dev/null; then
	fail "vk_frame_submit.c must not call vk_vegetation_wind_prepare_draw()"
fi

echo "PASS: test_vulkan_vegetation_dispatch_order"
