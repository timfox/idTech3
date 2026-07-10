#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <project-root>" >&2
  exit 2
fi

ROOT="$1"
# shellcheck source=idtech3_test_paths.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
fail() { echo "FAIL: $*" >&2; exit 1; }

require() {
  [ -f "$1" ] || fail "missing file: $1"
}

require "$(idtech3_file renderers/common/tr_vector_font_glyphlet.c src/renderers/common/tr_vector_font_glyphlet.c)"
require "$(idtech3_file renderers/vulkan/vk_vector_font.c src/renderers/vulkan/vk_vector_font.c)"
require "$(idtech3_file renderers/vulkan/shaders/glsl/vert_ui_vector_glyphlet.vert src/renderers/vulkan/shaders/glsl/vert_ui_vector_glyphlet.vert)"
require "$(idtech3_file renderers/vulkan/shaders/glsl/frag_ui_vector_glyphlet.frag src/renderers/vulkan/shaders/glsl/frag_ui_vector_glyphlet.frag)"

TR_LOCAL="$(idtech3_file renderers/vulkan/tr_local.h src/renderers/vulkan/tr_local.h)"
TR_CMDS="$(idtech3_file renderers/vulkan/tr_cmds.c src/renderers/vulkan/tr_cmds.c)"
TR_BACKEND="$(idtech3_file renderers/vulkan/tr_backend.c src/renderers/vulkan/tr_backend.c)"
TR_VECTOR_FONT="$(idtech3_file renderers/common/tr_vector_font.c src/renderers/common/tr_vector_font.c)"
TR_VECTOR_FONT_GLYPHLET="$(idtech3_file renderers/common/tr_vector_font_glyphlet.c src/renderers/common/tr_vector_font_glyphlet.c)"

rg -q 'RC_VECTOR_FONT_STRING' "$TR_LOCAL" || fail "RC_VECTOR_FONT_STRING missing"
rg -q 'RE_QueueVectorFontString' "$TR_CMDS" || fail "RE_QueueVectorFontString missing"
rg -q 'RB_VectorFontString' "$TR_BACKEND" || fail "RB_VectorFontString missing"
rg -q 'VECTOR_FONT_MODE_LOOP_BLINN' "$TR_VECTOR_FONT" || fail "mode 2 constant missing"
rg -q 'R_VectorGlyphlet_BuildFromSlot' "$TR_VECTOR_FONT_GLYPHLET" || fail "glyphlet builder missing"

echo "PASS: vector font mode 2 wiring present"
