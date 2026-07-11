#!/usr/bin/env bash
# Wiring test: MD3 .md3.paint sidecar (no MD3 format change).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

PAINT_C="${ROOT}/renderers/vulkan/tr_material_paint.c"
MODEL="${ROOT}/renderers/vulkan/tr_model.c"
SURF="${ROOT}/renderers/vulkan/tr_surface.c"
QFILES="${ROOT}/engine/core/qfiles.h"
DOC="${ROOT}/docs/MATERIAL_BLEND.md"

rg -q 'R_MaterialPaint_LoadMD3' "$PAINT_C" || fail "LoadMD3 missing"
rg -q 'R_MaterialPaint_LoadMD3' "$MODEL" || fail "R_LoadMD3 must call LoadMD3"
rg -q 'md3PaintColors' "$SURF" || fail "RB_SurfaceMesh must fill vertexColors from paint"
rg -q '\.md3\.paint|MD3 paint' "$DOC" || fail "docs must mention MD3 paint"
# Assert MD3 vertex struct was not extended with color (sidecar only).
rg -q 'md3XyzNormal_t' "$QFILES" || fail "md3XyzNormal_t missing"
if rg -n 'typedef struct md3XyzNormal' -A6 "$QFILES" | rg -q 'color|paint|rgba'; then
  fail "md3XyzNormal_t must not gain color fields (use .md3.paint sidecar)"
fi

echo "test_md3_paint: passed"
