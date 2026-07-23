#!/usr/bin/env bash
# Color Pipeline Phase 2.2 — alpha encoding / normalization / accum audit (umbrella).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
pass() { echo "OK: $*"; }

H="$ROOT/renderers/vulkan/vk_oit_alpha.h"
C="$ROOT/renderers/vulkan/vk_oit_alpha.c"
GLSL="$ROOT/renderers/vulkan/shaders/glsl/oit_source_normalize.glsl"
ACCUM="$ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag"
DOC="$ROOT/docs/WBOIT_ALPHA_ENCODING.md"
VIEW="$ROOT/renderers/vulkan/vk_view_state.c"

[[ -f "$H" ]] || fail "missing vk_oit_alpha.h"
[[ -f "$C" ]] || fail "missing vk_oit_alpha.c"
[[ -f "$GLSL" ]] || fail "missing oit_source_normalize.glsl"
[[ -f "$DOC" ]] || fail "missing WBOIT_ALPHA_ENCODING.md"

for e in OIT_SOURCE_ALPHA_STRAIGHT OIT_SOURCE_ALPHA_PREMULTIPLIED OIT_SOURCE_ALPHA_OPAQUE \
	OIT_SOURCE_ALPHA_ADDITIVE OIT_SOURCE_ALPHA_MASKED OIT_SOURCE_ALPHA_MULTIPLICATIVE OIT_SOURCE_ALPHA_UNKNOWN; do
	grep -q "$e" "$H" || fail "enum $e missing"
done
pass "source-alpha encodings"

grep -q 'NormalizeOitSource' "$GLSL" || fail "NormalizeOitSource missing"
grep -q 'unassociatedRadiance' "$GLSL" || fail "unassociatedRadiance missing"
grep -q 'associatedRadiance' "$GLSL" || fail "associatedRadiance missing"
grep -q 'materialTransparencyInfo_t' "$H" || fail "materialTransparencyInfo_t missing"
grep -q 'oit_alpha_status' "$C" || fail "oit_alpha_status missing"
grep -q 'material_alpha_status' "$C" || fail "material_alpha_status missing"
grep -q 'classic_alpha_translate_status' "$C" || fail "classic_alpha_translate_status missing"
grep -q 'OIT_ALPHA_EDGE_CERTIFIED' "$H" || fail "EDGE_CERTIFIED missing"
grep -q 'r_oitFaultDoublePremultiply' "$C" || fail "fault injection missing"
grep -q 'r_transparentEdgePolicy' "$C" || fail "edge policy missing"
pass "alpha module + commands"

grep -q 'oit_source_normalize.glsl' "$ACCUM" || fail "accum must include normalizer"
grep -q 'NormalizeOitSource' "$ACCUM" || fail "accum must call NormalizeOitSource"
grep -q 'unassociatedRadiance' "$ACCUM" || fail "accum must use unassociated radiance"
grep -q 'litRgb \* alpha, alpha ) \* w' "$ACCUM" || fail "accum equation drift"
# Double-premultiply form must not appear as lit * alpha * alpha
if grep -qE 'litRgb \* alpha \* alpha|associatedRadiance \* alpha \* w' "$ACCUM"; then
	fail "suspected double alpha multiply in accum"
fi
pass "accum consumes normalized unassociated once"

grep -q 'alphaPack' "$VIEW" || fail "push constant alphaPack missing"
grep -q 'vk_oit_alpha_pack_push' "$VIEW" || fail "pack push missing"
grep -q 'vk_oit_alpha_register' "$ROOT/renderers/vulkan/vk_transparency_route.c" || fail "alpha register wiring"
pass "runtime wiring"

grep -q 'unassociatedRadiance' "$DOC" || fail "doc internal representation"
grep -q 'OIT_SOURCE_ALPHA_STRAIGHT' "$DOC" || fail "doc encodings"
[[ -f "$ROOT/docs/WBOIT_SOURCE_NORMALIZATION.md" ]] || fail "missing SOURCE_NORMALIZATION doc"
[[ -f "$ROOT/docs/TRANSPARENT_TEXTURE_AUTHORING.md" ]] || fail "missing AUTHORING doc"
[[ -f "$ROOT/docs/CLASSIC_SHADER_ALPHA_TRANSLATION.md" ]] || fail "missing CLASSIC doc"
pass "documentation"

# Sub-checks (Phase 2.2 script list)
for t in \
	test_oit_source_encoding.sh \
	test_oit_straight_normalization.sh \
	test_oit_premul_normalization.sh \
	test_oit_double_premultiply.sh \
	test_oit_missing_alpha_multiply.sh \
	test_oit_zero_alpha_rgb.sh \
	test_oit_alpha_filtering.sh \
	test_oit_alpha_mips.sh \
	test_oit_alpha_atlas.sh \
	test_oit_emissive_opacity.sh \
	test_oit_classic_alpha_translation.sh \
	test_oit_encoding_equivalence.sh \
	test_oit_single_layer_source_over.sh \
	test_oit_alpha_faults.sh \
	test_oit_alpha_runtime_validation.sh
do
	path="$ROOT/tests/scripts/$t"
	if [[ ! -x "$path" ]]; then
		fail "$t not executable"
		continue
	fi
	"$path" || fail "$t failed"
done

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All OIT alpha contract checks passed."
