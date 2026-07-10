#!/usr/bin/env bash
# RB_EndSurface must prepare vegetation wind before tess.shader->optimalStageIteratorFunc().
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$SCRIPT_DIR/idtech3_test_paths.sh"
idtech3_test_paths_init "$PROJECT_ROOT"

TR_SHADE_DEFAULT="$(idtech3_file renderers/vulkan/tr_shade.c src/renderers/vulkan/tr_shade.c)"
FRAME_SUBMIT_DEFAULT="$(idtech3_file renderers/vulkan/vk_frame_submit.c src/renderers/vulkan/vk_frame_submit.c)"
TR_SHADE_FILE="${1:-$TR_SHADE_DEFAULT}"
FRAME_SUBMIT_FILE="${2:-$FRAME_SUBMIT_DEFAULT}"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

[ -f "$TR_SHADE_FILE" ] || fail "missing $TR_SHADE_FILE"
[ -f "$FRAME_SUBMIT_FILE" ] || fail "missing $FRAME_SUBMIT_FILE"

prepare_line="$(rg -n 'vk_vegetation_wind_prepare_draw\s*\(' "$TR_SHADE_FILE" | cut -d: -f1)"
iterator_line="$(rg -n 'optimalStageIteratorFunc\s*\(' "$TR_SHADE_FILE" | head -1 | cut -d: -f1)"

if [ -z "$prepare_line" ]; then
	fail "tr_shade.c missing vk_vegetation_wind_prepare_draw()"
fi
if [ -z "$iterator_line" ]; then
	fail "tr_shade.c missing optimalStageIteratorFunc()"
fi
if [ "$prepare_line" -ge "$iterator_line" ]; then
	fail "prepare must precede optimalStageIteratorFunc (prepare=$prepare_line iterator=$iterator_line)"
fi

if rg -n 'vk_vegetation_wind_prepare_draw\s*\(' "$FRAME_SUBMIT_FILE" >/dev/null; then
	fail "vk_frame_submit.c must not call vk_vegetation_wind_prepare_draw()"
fi

echo "PASS: test_vk_vegetation_dispatch_order"
