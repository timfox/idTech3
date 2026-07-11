#!/usr/bin/env bash
# Wiring test: multi-material PBR height-blend keywords, flags, frag markers, cvars, docs.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

TR_SHADER="${ROOT}/renderers/vulkan/tr_shader.c"
TR_SHADE="${ROOT}/renderers/vulkan/tr_shade.c"
TR_INIT="${ROOT}/renderers/vulkan/tr_init.c"
VK_H="${ROOT}/renderers/vulkan/vk.h"
FRAG="${ROOT}/renderers/vulkan/shaders/glsl/gen_frag.tmpl"
PIPE="${ROOT}/renderers/vulkan/vk_create_pipeline.c"
DOC="${ROOT}/docs/MATERIAL_BLEND.md"
DEMO_SHADER="${ROOT}/examples/demo_game/mod/scripts/demo_material_blend.shader"

[ -f "$DOC" ] || fail "missing docs/MATERIAL_BLEND.md"
[ -f "$DEMO_SHADER" ] || fail "missing demo_material_blend.shader"

rg -q 'materialBlend' "$TR_SHADER" || fail "tr_shader.c missing materialBlend parse"
rg -q 'layerMap' "$TR_SHADER" || fail "tr_shader.c missing layerMap"
rg -q 'layerIdx > 7|layerIdx < 1 \|\| layerIdx > 7' "$TR_SHADER" || fail "layerMap should accept 1..7"
rg -q 'blend_albedo\[8\]' "$FRAG" || fail "gen_frag missing blend_albedo[8] set-19 arrays"
rg -q 'VK_DESC_PBR_BLEND_LAYERS' "$VK_H" || fail "vk.h missing VK_DESC_PBR_BLEND_LAYERS"
rg -q 'materialBlendWeightsHi|frag_color1' "$FRAG" || fail "dual weight stream (frag_color1) missing"
rg -q 'MATERIAL_PAINT_FLAG_STREAM2' "${ROOT}/renderers/vulkan/tr_material_paint.h" || fail "paint stream2 missing"
rg -q 'layerNormalHeightMap' "$TR_SHADER" || fail "tr_shader.c missing layerNormalHeightMap"
rg -q 'blendSharpness' "$TR_SHADER" || fail "tr_shader.c missing blendSharpness"
rg -q 'PBR_HAS_MATERIAL_BLEND' "$VK_H" || fail "vk.h missing PBR_HAS_MATERIAL_BLEND"
rg -q 'material_blend_layers' "$PIPE" || fail "vk_create_pipeline.c missing material_blend_layers"
rg -q 'materialBlendWeights' "$FRAG" || fail "gen_frag.tmpl missing materialBlendWeights"
rg -q 'material_height_mask' "$FRAG" || fail "gen_frag.tmpl missing material_height_mask"
rg -q 'pbrMaterialBlend' "$FRAG" || fail "gen_frag.tmpl missing pbrMaterialBlend UBO field"
rg -q 'r_materialBlend' "$TR_INIT" || fail "tr_init.c missing r_materialBlend"
rg -q 'Material blend:' "$TR_INIT" || fail "tr_init.c missing Material blend startup log"
rg -q 'materialBlend' "$TR_SHADE" || fail "tr_shade.c missing materialBlend binding/SH gate"
rg -q 'CGEN_EXACT_VERTEX' "$TR_SHADER" || fail "tr_shader.c should force CGEN_EXACT_VERTEX for blend"
rg -q 'materialBlend vertex' "$DEMO_SHADER" || fail "demo shader missing materialBlend vertex"
rg -q 'MATERIAL_BLEND' "${ROOT}/docs/PBR_TEXTURES.md" || fail "PBR_TEXTURES.md should link MATERIAL_BLEND"
rg -q 'r_materialBlend' "${ROOT}/AGENTS.md" || fail "AGENTS.md missing r_materialBlend gotcha"

echo "test_material_blend: passed"
