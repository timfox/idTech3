#!/usr/bin/env bash
# RB_EndSurface must prepare vegetation wind before tess.shader->optimalStageIteratorFunc().
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TR_SHADE_FILE="${1:-$PROJECT_ROOT/src/renderers/vulkan/tr_shade.c}"
FRAME_SUBMIT_FILE="${2:-$PROJECT_ROOT/src/renderers/vulkan/vk_frame_submit.c}"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

[ -f "$TR_SHADE_FILE" ] || fail "missing $TR_SHADE_FILE"
[ -f "$FRAME_SUBMIT_FILE" ] || fail "missing $FRAME_SUBMIT_FILE"

prepare_line="$(grep -En 'vk_vegetation_wind_prepare_draw[[:space:]]*\(' "$TR_SHADE_FILE" | head -1 | cut -d: -f1)"
iterator_line="$(grep -En 'optimalStageIteratorFunc[[:space:]]*\(' "$TR_SHADE_FILE" | head -1 | cut -d: -f1)"

if [ -z "$prepare_line" ]; then
	fail "tr_shade.c missing vk_vegetation_wind_prepare_draw()"
fi
if [ -z "$iterator_line" ]; then
	fail "tr_shade.c missing optimalStageIteratorFunc()"
fi
if [ "$prepare_line" -ge "$iterator_line" ]; then
	fail "prepare must precede optimalStageIteratorFunc (prepare=$prepare_line iterator=$iterator_line)"
fi

if grep -Eq 'vk_vegetation_wind_prepare_draw[[:space:]]*\(' "$FRAME_SUBMIT_FILE"; then
	fail "vk_frame_submit.c must not call vk_vegetation_wind_prepare_draw()"
fi

echo "PASS: test_vk_vegetation_dispatch_order"
