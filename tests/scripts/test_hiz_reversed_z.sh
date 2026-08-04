#!/usr/bin/env bash
# Foundation Consolidation: Hi-Z pyramid + reversed-Z depth contract.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

HIZ="$ROOT/renderers/vulkan/vk_hiz.c"
HIZH="$ROOT/renderers/vulkan/vk_hiz.h"
VS="$ROOT/renderers/vulkan/vk_view_state.c"
PIPE="$ROOT/renderers/vulkan/vk_pipeline_helpers.c"

[[ -f "$HIZ" ]] || fail "missing vk_hiz.c"
grep -q 'hiz_status' "$HIZ" || fail "hiz_status missing"
grep -q 'r_hiZ' "$HIZ" || fail "r_hiZ cvar missing"
grep -q 'conservative\|Conservative' "$HIZ" || fail "conservative policy comment missing"
grep -q 'srcView = dstView' "$HIZ" || fail "Hi-Z mip 0 must keep srcMip descriptor valid"
grep -q 'writeCount = 3' "$HIZ" || fail "Hi-Z must update all compute descriptors on every mip"
grep -q 'Descriptor contents are read when the command executes' "$HIZ" || fail "Hi-Z must rebind descriptors after each mip update"
pass "Hi-Z module wired"

grep -q 'reversed-Z\|reversed-Z' "$VS" || fail "vk_view_state must document reversed-Z"
grep -q 'VK_COMPARE_OP_GREATER_OR_EQUAL' "$PIPE" || fail "depth compare must use GREATER_OR_EQUAL (reversed-Z)"
pass "reversed-Z depth compare present"

if [[ -f "$ROOT/renderers/vulkan/shaders/glsl/hiz_downsample.comp" ]]; then
	HIZ_CS="$ROOT/renderers/vulkan/shaders/glsl/hiz_downsample.comp"
	grep -q 'min(' "$HIZ_CS" || fail "hiz_downsample must use min for reversed-Z farthest"
	grep -q 'texelFetch( depthTex' "$HIZ_CS" || fail "first Hi-Z level must aggregate depth texels conservatively"
	grep -q 'srcLast' "$HIZ_CS" || fail "Hi-Z downsample must clamp odd-sized mip footprints"
	if grep -q 'texture( depthTex' "$HIZ_CS"; then
		fail "first Hi-Z level must not use filtered depth sampling"
	fi
	pass "hiz_downsample.comp conservative min present"
else
	grep -q 'hiz_downsample' "$ROOT/renderers/vulkan/shaders/spirv/shader_binding.c" || fail "shader binding must reference hiz_downsample"
	pass "hiz_downsample SPIR-V binding present (GLSL optional)"
fi

grep -q 'hiz_status' "$ROOT/docs/GPU_DRIVEN_RENDERING.md" || fail "GPU_DRIVEN doc must mention hiz_status"
pass "Hi-Z documented in GPU_DRIVEN_RENDERING.md"

if [[ -f "$ROOT/docs/DEPTH_CONTRACT.md" ]]; then
	grep -q 'reversedZ\|reversed-Z' "$ROOT/docs/DEPTH_CONTRACT.md" || fail "DEPTH_CONTRACT.md must document reversed-Z"
	grep -q 'depthContract_t' "$ROOT/renderers/vulkan/vk_depth_contract.h" || fail "depthContract_t missing"
	pass "Phase 2.3.1 depth contract present"
fi

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All Hi-Z reversed-Z checks passed."
