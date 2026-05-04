#!/usr/bin/env bash
# Regression test for Vulkan vegetation-wind dispatch placement.
# Ensures dispatch runs only after SURF_VEGETATION tess upload in RB_EndSurface.
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

# Guard the exact fix behavior: feature gate + vegetation surface gate + dispatch + clear.
if ! rg -n -U --multiline-dotall \
	'if\s*\(\s*PostFX_VegWind_IsEnabled\(\)\s*&&\s*tess\.shader\s*&&\s*\(\s*tess\.shader->surfaceFlags\s*&\s*SURF_VEGETATION\s*\)\s*\)\s*\{\s*vk_vegetation_wind_dispatch\(\);\s*vk_vegetation_clear_staging\(\);\s*\}' \
	"$TR_SHADE_FILE" >/dev/null; then
	fail "missing guarded veg-wind dispatch+clear block in tr_shade.c"
fi

dispatch_count="$(rg -n 'vk_vegetation_wind_dispatch\s*\(' "$TR_SHADE_FILE" | wc -l | tr -d '[:space:]')"
clear_count="$(rg -n 'vk_vegetation_clear_staging\s*\(' "$TR_SHADE_FILE" | wc -l | tr -d '[:space:]')"
if [ "$dispatch_count" != "1" ]; then
	fail "expected exactly one vk_vegetation_wind_dispatch() in tr_shade.c, got $dispatch_count"
fi
if [ "$clear_count" != "1" ]; then
	fail "expected exactly one vk_vegetation_clear_staging() in tr_shade.c, got $clear_count"
fi

dispatch_line="$(rg -n 'vk_vegetation_wind_dispatch\s*\(' "$TR_SHADE_FILE" | cut -d: -f1)"
clear_line="$(rg -n 'vk_vegetation_clear_staging\s*\(' "$TR_SHADE_FILE" | cut -d: -f1)"
if [ "$clear_line" -le "$dispatch_line" ]; then
	fail "expected staging clear after dispatch (dispatch=$dispatch_line clear=$clear_line)"
fi

# Frame-begin dispatch caused the regression; keep this file free of the call.
if rg -n 'vk_vegetation_wind_dispatch\s*\(' "$VK_FRAME_SUBMIT_FILE" >/dev/null; then
	fail "vk_frame_submit.c must not call vk_vegetation_wind_dispatch()"
fi

echo "PASS: test_vulkan_vegetation_dispatch_order"
