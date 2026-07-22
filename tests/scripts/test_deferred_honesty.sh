#!/usr/bin/env bash
# Deferred Honesty — architecture naming, status command, composite vocabulary.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

DOC="$ROOT/docs/DEFERRED_HONESTY.md"
H="$ROOT/renderers/vulkan/vk_deferred_honesty.c"
HH="$ROOT/renderers/vulkan/vk_deferred_honesty.h"

[[ -f "$DOC" ]] || fail "missing DEFERRED_HONESTY.md"
grep -q 'HYBRID_ADDITIVE_DEFERRED' "$DOC" || fail "doc must name HYBRID_ADDITIVE_DEFERRED"
grep -q 'SceneBaseLit' "$DOC" || fail "doc must name SceneBaseLit"
grep -q 'deferred_status' "$DOC" || fail "doc must mention deferred_status"
pass "DEFERRED_HONESTY.md present"

[[ -f "$H" && -f "$HH" ]] || fail "vk_deferred_honesty module missing"
grep -q 'HYBRID_ADDITIVE_DEFERRED' "$H" || fail "honesty.c must emit HYBRID_ADDITIVE_DEFERRED"
grep -q 'deferred_status' "$H" || fail "deferred_status command missing"
grep -q 'R_GetDeferredEligibility' "$HH" || fail "R_GetDeferredEligibility missing"
grep -q 'R_TranslateClassicShaderToMaterial' "$HH" || fail "classic translation API missing"
grep -q 'r_deferredArchitecture' "$H" || fail "r_deferredArchitecture missing"
grep -q 'r_deferredCompositeMode' "$H" || fail "r_deferredCompositeMode missing"
pass "honesty module symbols"

grep -q 'vk_deferred_honesty' "$ROOT/renderers/vulkan/vk_render_path.c" || fail "render path must include honesty"
grep -q 'R_GetDeferredEligibility' "$ROOT/renderers/vulkan/vk_render_path.c" || fail "path select must call eligibility"
grep -q 'HYBRID_ADDITIVE' "$ROOT/renderers/vulkan/vk_deferred_gbuffer.c" || fail "gbuffer status must label hybrid"
pass "path + gbuffer wired to honesty"

# Must not claim complete deferred in honesty status.
if grep -q 'standard deferred opaque world' "$H"; then
	fail "honesty must not claim standard deferred opaque world"
else
	pass "no false 'standard deferred' claim in honesty.c"
fi

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All deferred honesty checks passed."
