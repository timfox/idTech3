#!/usr/bin/env bash
# Wiring test: material paint sidecar + Studio panel + cvars.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

PAINT_C="${ROOT}/renderers/vulkan/tr_material_paint.c"
PAINT_H="${ROOT}/renderers/vulkan/tr_material_paint.h"
STUDIO="${ROOT}/renderers/vulkan/inspector/vk_imgui_studio_panels.cpp"
DOC="${ROOT}/docs/MATERIAL_BLEND.md"
BSP="${ROOT}/renderers/vulkan/tr_bsp.c"

[ -f "$PAINT_C" ] || fail "missing tr_material_paint.c"
[ -f "$PAINT_H" ] || fail "missing tr_material_paint.h"
rg -q 'MATERIAL_PAINT_MAGIC' "$PAINT_H" || fail "paint magic missing"
rg -q 'r_materialPaint' "$PAINT_C" || fail "r_materialPaint cvar missing"
rg -q 'paint_save' "$PAINT_C" || fail "paint_save command missing"
rg -q 'R_MaterialPaint_OnMapLoad' "$BSP" || fail "BSP must call OnMapLoad"
rg -q 'VkImgui_DrawStudioPaintPanel' "$STUDIO" || fail "Studio Paint panel missing"
rg -q 'maps/.*\.paint|Material paint' "$DOC" || fail "docs must mention .paint"
rg -q 'r_materialPaint' "${ROOT}/AGENTS.md" || fail "AGENTS.md gotcha missing"

echo "test_material_paint: passed"
