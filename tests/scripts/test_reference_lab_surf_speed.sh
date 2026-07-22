#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "PASS: $*"; }

grep -q 'VK_REFLAB_SCENE_SURF_SPEED' "$ROOT/renderers/vulkan/vk_reference_lab.h" || fail "SURF_SPEED enum missing"
grep -q 'surf_speed' "$ROOT/renderers/vulkan/vk_reference_lab.c" || fail "surf_speed scene missing"
[[ -f "$ROOT/config/demo_reference_lab_surf_speed.cfg" ]] || fail "demo cfg missing"
grep -q 'renderer_validate_frame' "$ROOT/config/demo_reference_lab_surf_speed.cfg" || fail "cfg must mention validate"
grep -q 'renderer_validate_frame' "$ROOT/renderers/vulkan/vk_black_frame.c" || fail "validate command missing"
grep -q 'r_reflectionDebug' "$ROOT/renderers/vulkan/vk_selective_reflection.c" || fail "r_reflectionDebug missing"
grep -q 'r_gpuDrawCompare' "$ROOT/renderers/vulkan/vk_gpu_scene.c" || fail "r_gpuDrawCompare missing"
grep -q 'r_meshletsBspPilot' "$ROOT/renderers/vulkan/vk_meshlets.c" || fail "r_meshletsBspPilot missing"
[[ -f "$ROOT/renderers/vulkan/shaders/glsl/hiz_downsample.comp" ]] || fail "hiz_downsample.comp missing"
grep -q 'GbufEncodeOctahedral\|imageStore' "$ROOT/renderers/vulkan/shaders/glsl/hiz_downsample.comp" || \
	grep -q 'max(' "$ROOT/renderers/vulkan/shaders/glsl/hiz_downsample.comp" || fail "hiz_downsample.comp incomplete"
pass "Surf-speed lab + sprint cvars/scripts wired"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All reference-lab surf-speed checks passed."
echo "Soak (manual): exec demo_reference_lab_surf_speed.cfg ; cycle bookmarks ; renderer_validate_frame"
