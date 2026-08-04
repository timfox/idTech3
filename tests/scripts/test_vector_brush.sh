#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
H="$ROOT/renderers/vulkan/extensions/scaffold/vk_vector_brush.h"
C="$ROOT/renderers/vulkan/extensions/scaffold/vk_vector_brush.c"
DOC="$ROOT/docs/VECTOR_BRUSHES.md"
CFG="$ROOT/config/vulkan_overlay_vector_brush.cfg"
SHADER="$ROOT/renderers/vulkan/shaders/glsl/vector_brush/vector_brush_resample.comp"

grep -q 'vkVectorBrushPoint_t' "$H"
grep -q 'target' "$H"
grep -q 'r_vectorBrushSpacing' "$C"
grep -q 'vector_brush_status' "$C"
grep -q 'target.*overlay' "$DOC"
grep -q 'r_vectorBrush 1' "$CFG"
grep -q 'BrushPoints' "$SHADER"
grep -q 'maxStampsPerEdge' "$SHADER"
grep -q 'vector_brush_resample.comp' "$ROOT/scripts/compile_shaders.sh"
echo "test_vector_brush.sh: ok"

