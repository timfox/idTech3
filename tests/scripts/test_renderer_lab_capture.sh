#!/usr/bin/env bash
# Foundation Consolidation: reference lab scenes + capture checklist.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/RENDERER_LAB.md"
LAB="$ROOT/renderers/vulkan/vk_reference_lab.c"
LABH="$ROOT/renderers/vulkan/vk_reference_lab.h"
CFG="$ROOT/config/vulkan_overlay_raster_ultra_1_11_reference_lab.cfg"

[[ -f "$DOC" ]] || fail "missing RENDERER_LAB.md"
grep -q 'reference_lab_status' "$DOC" || fail "doc must mention reference_lab_status"
grep -q 'reference_lab_scenes' "$DOC" || fail "doc must mention reference_lab_scenes"
grep -q 'renderer_validate_frame' "$DOC" || fail "doc capture list must include renderer_validate_frame"
grep -q 'bookmark' "$DOC" || fail "doc must mention camera bookmarks"
pass "RENDERER_LAB.md capture list present"

[[ -f "$LABH" ]] || fail "missing vk_reference_lab.h"
grep -q 'VK_REFLAB_SCENE_COUNT' "$LABH" || fail "scene enum missing"
grep -q 'VK_REFLAB_SCENE_HDR_PRESENTATION' "$LABH" || fail "HDR presentation scene missing"
pass "reference lab header present"

grep -q 'reference_lab_status' "$LAB" || fail "reference_lab_status command missing"
grep -q 'reference_lab_scenes' "$LAB" || fail "reference_lab_scenes command missing"
grep -q 'vk_reference_lab_begin_frame' "$ROOT/renderers/vulkan/vk_frame_submit.c" || fail "lab begin_frame not wired"
pass "reference lab wired into frame submit"

[[ -f "$CFG" ]] || fail "missing vulkan_overlay_raster_ultra_1_11_reference_lab.cfg"
grep -q 'r_referenceLab' "$CFG" || fail "overlay cfg must set r_referenceLab"
pass "reference lab overlay cfg present"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All renderer lab capture checks passed."
