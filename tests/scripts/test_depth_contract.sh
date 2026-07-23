#!/usr/bin/env bash
# Color Pipeline Phase 2.3.1: frozen depth contract.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/DEPTH_CONTRACT.md"
H="$ROOT/renderers/vulkan/vk_depth_contract.h"
C="$ROOT/renderers/vulkan/vk_depth_contract.c"
CLEAR="$ROOT/renderers/vulkan/vk_clear_attachments.c"
PIPE="$ROOT/renderers/vulkan/vk_pipeline_helpers.c"
PROJ="$ROOT/renderers/vulkan/tr_main.c"

[[ -f "$DOC" ]] || fail "missing DEPTH_CONTRACT.md"
[[ -f "$H" ]] || fail "missing vk_depth_contract.h"
[[ -f "$C" ]] || fail "missing vk_depth_contract.c"

grep -q 'depthContract_t' "$H" || fail "depthContract_t missing"
grep -q 'DEPTH_CONTRACT_VERSION' "$H" || fail "VERSION missing"
grep -q 'DEPTH_PROJECTION_PERSPECTIVE' "$H" || fail "projection enum missing"
grep -q 'VIEW_DEPTH_NEG_VIEW_Z' "$H" || fail "view-depth enum missing"
grep -q 'reversedZ' "$H" || fail "reversedZ missing"
grep -q 'zeroToOneClipDepth' "$H" || fail "clip depth missing"
grep -q 'clearDepth' "$H" || fail "clearDepth missing"
grep -q 'positiveViewDepthMode' "$H" || fail "positiveViewDepthMode missing"
pass "depthContract_t surface"

grep -q 'depth_contract_status' "$C" || fail "depth_contract_status missing"
grep -q 'depth_contract_validate' "$C" || fail "validate missing"
grep -q 'VK_COMPARE_OP_GREATER_OR_EQUAL' "$C" || fail "compare op freeze missing"
grep -q 'clearDepth = 0.0f' "$C" || fail "clear 0 freeze missing"
grep -q 'VIEW_DEPTH_NEG_VIEW_Z' "$C" || fail "certified view-depth missing"
pass "vk_depth_contract implementation"

grep -q 'reversed-Z\|reversedZ' "$DOC" || fail "doc reversed-Z"
grep -q 'GREATER_OR_EQUAL' "$DOC" || fail "doc compare"
grep -q 'neg_view_z\|VIEW_DEPTH_NEG_VIEW_Z\|-viewSpace.z' "$DOC" || fail "doc positive view-depth"
grep -q 'depth_contract_status' "$DOC" || fail "doc status command"
pass "DEPTH_CONTRACT.md"

# Live path must match freeze
grep -q 'depthStencil.depth = 0.0f' "$CLEAR" || fail "clear attachments must clear depth to 0"
grep -q 'VK_COMPARE_OP_GREATER_OR_EQUAL' "$PIPE" || fail "pipelines must use GREATER_OR_EQUAL"
grep -q 'projectionMatrix\[10\] = zNear / depth' "$PROJ" || fail "Vulkan projectionZ reversed-Z form"
pass "implementation matches freeze"

GLSL_VIEW="$ROOT/renderers/vulkan/shaders/glsl/depth_view.glsl"
if [[ -f "$GLSL_VIEW" ]]; then
	grep -q 'Depth_LinearizeReversedZ' "$GLSL_VIEW" || fail "depth_view.glsl missing linearize"
	pass "depth_view.glsl present (Phase 2.3.2)"
fi

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All depth contract freeze checks passed."
