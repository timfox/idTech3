#!/usr/bin/env bash
# Foundation Consolidation: deferred vs Forward+ material path routing.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/GBUFFER_2.md"
RP="$ROOT/renderers/vulkan/vk_render_path.c"
RPH="$ROOT/renderers/vulkan/vk_render_path.h"

grep -q 'R_SelectSurfaceRenderPath' "$DOC" || fail "GBUFFER_2.md must mention R_SelectSurfaceRenderPath"
grep -q 'render_path_status' "$DOC" || fail "doc must mention render_path_status"
pass "material routing documented"

[[ -f "$RPH" ]] || fail "missing vk_render_path.h"
grep -q 'R_SelectSurfaceRenderPath' "$RPH" || fail "header must declare R_SelectSurfaceRenderPath"
grep -q 'RENDER_PATH_DEFERRED_OPAQUE' "$RPH" || fail "deferred path enum missing"
grep -q 'RENDER_PATH_FORWARD_PLUS_OPAQUE' "$RPH" || fail "Forward+ path enum missing"
pass "render path header present"

grep -q 'R_SelectSurfaceRenderPath' "$RP" || fail "R_SelectSurfaceRenderPath implementation missing"
grep -q 'render_path_status' "$RP" || fail "render_path_status command missing"
grep -q 'r_materialPathReason' "$RP" || fail "r_materialPathReason cvar missing"
grep -q 'R_SelectMaterialRenderPath' "$RP" || fail "R_SelectMaterialRenderPath missing"
pass "material routing implementation wired"

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All material routing checks passed."
