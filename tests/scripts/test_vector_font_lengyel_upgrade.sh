#!/usr/bin/env bash
# Static gates for atlas-free Lengyel vector font upgrades.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

VF="$ROOT/renderers/common/tr_vector_font.c"
SH="$ROOT/renderers/vulkan/shaders/glsl/frag_ui_vector_text.frag"
LOAD="$ROOT/renderers/vulkan/tr_shader_load.inc"
SHAPE="$ROOT/renderers/common/tr_vector_font_shape.c"

grep -q 'VecOutline_SortCurvesByMaxY' "$VF" || fail 'Y-sorted curve list missing'
grep -q 'VecOutline_SortCurvesByMaxX' "$VF" || fail 'X-sorted curve list missing'
grep -q 'VecOutline_EnsureTexelCapacity' "$VF" || fail 'curve storage must grow with actual outline complexity'
if grep -q 'VECTOR_MAX_CURVE_SEGS \\* GLYPHS_PER_FONT' "$VF"; then
	fail 'curve storage still reserves the all-glyph worst case'
fi
grep -q 'r_vectorFontCoverage' "$VF" || fail 'coverage cvar'
grep -q 'vector_font_status' "$VF" || fail 'vector_font_status command'
grep -q 'state=INCOMPLETE' "$VF" || fail 'certification must report unsupported production requirements honestly'
grep -q 'startY' "$SH" || fail 'shader dual list startY'
grep -q 'premultiplied\|Premultiplied' "$SH" || fail 'premul output'
grep -q 'GLS_SRCBLEND_ONE' "$LOAD" || fail 'premul blend ONE'
grep -q 'R_VectorFont_ShapeRun' "$SHAPE" || fail 'shape API'
[[ -f "$ROOT/docs/VECTOR_FONT_RENDERING.md" ]] || fail 'VECTOR_FONT_RENDERING.md'
[[ -f "$ROOT/docs/VECTOR_FONT_COVERAGE.md" ]] || fail 'VECTOR_FONT_COVERAGE.md'
[[ -f "$ROOT/docs/VECTOR_FONT_WINDING.md" ]] || fail 'VECTOR_FONT_WINDING.md'

[[ $failures -eq 0 ]] || { echo "$failures failed"; exit 1; }
echo "All vector_font_lengyel_upgrade checks passed."
