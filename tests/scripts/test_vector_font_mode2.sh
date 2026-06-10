#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <project-root>" >&2
  exit 2
fi

ROOT="$1"
fail() { echo "FAIL: $*" >&2; exit 1; }

require() {
  [ -f "$1" ] || fail "missing file: $1"
}

require "$ROOT/src/renderers/common/tr_vector_font_glyphlet.c"
require "$ROOT/src/renderers/vulkan/vk_vector_font.c"
require "$ROOT/src/renderers/vulkan/shaders/glsl/vert_ui_vector_glyphlet.vert"
require "$ROOT/src/renderers/vulkan/shaders/glsl/frag_ui_vector_glyphlet.frag"

rg -q 'RC_VECTOR_FONT_STRING' "$ROOT/src/renderers/vulkan/tr_local.h" || fail "RC_VECTOR_FONT_STRING missing"
rg -q 'RE_QueueVectorFontString' "$ROOT/src/renderers/vulkan/tr_cmds.c" || fail "RE_QueueVectorFontString missing"
rg -q 'RB_VectorFontString' "$ROOT/src/renderers/vulkan/tr_backend.c" || fail "RB_VectorFontString missing"
rg -q 'VECTOR_FONT_MODE_LOOP_BLINN' "$ROOT/src/renderers/common/tr_vector_font.c" || fail "mode 2 constant missing"
rg -q 'R_VectorGlyphlet_BuildFromSlot' "$ROOT/src/renderers/common/tr_vector_font_glyphlet.c" || fail "glyphlet builder missing"

echo "PASS: vector font mode 2 wiring present"
