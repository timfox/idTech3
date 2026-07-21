#!/usr/bin/env bash
# CSS-style UI filter / backdrop-filter blur compositor wiring checks
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

UIBLUR="$(idtech3_file renderers/vulkan/vk_ui_blur.c src/renderers/vulkan/vk_ui_blur.c)"
UIBLUR_H="$(idtech3_file renderers/vulkan/vk_ui_blur.h src/renderers/vulkan/vk_ui_blur.h)"
FRAME_END="$(idtech3_file renderers/vulkan/vk_frame_end.c src/renderers/vulkan/vk_frame_end.c)"
TR_BACKEND="$(idtech3_file renderers/vulkan/tr_backend.c src/renderers/vulkan/tr_backend.c)"
TR_CMDS="$(idtech3_file renderers/vulkan/tr_cmds.c src/renderers/vulkan/tr_cmds.c)"
TR_INIT="$(idtech3_file renderers/vulkan/tr_init.c src/renderers/vulkan/tr_init.c)"
TR_TYPES="$(idtech3_file renderers/common/tr_types.h src/renderers/common/tr_types.h)"
TR_PUBLIC="$(idtech3_file renderers/common/tr_public.h src/renderers/common/tr_public.h)"
UIFILTER="$(idtech3_file runtime/client/shell/ui_filter.c src/client/ui_filter.c)"
UICSS="$(idtech3_file runtime/client/shell/ui_css.c src/client/ui_css.c)"
JSDEBUG="$(idtech3_file engine/core/js_debug.c src/qcommon/js_debug.c)"
GLSL_DIR="$ROOT/renderers/vulkan/shaders/glsl"

# renderer core: data structures, cvars, dual path, pooled targets
grep -q 'uiTransientTexturePool_t' "$UIBLUR"
grep -q 'ui_filterDebug' "$UIBLUR"
grep -q 'ui_blurQuality' "$UIBLUR"
grep -q 'ui_blurMaxRadius' "$UIBLUR"
grep -q 'ui_blurDownsampleThreshold' "$UIBLUR"
grep -q 'ui_blurCache' "$UIBLUR"
grep -q 'ui_blur_status' "$UIBLUR"
grep -q 'uib_quantize_radius' "$UIBLUR"
grep -q 'uib_build_backdrop' "$UIBLUR"
grep -q 'pipeGauss' "$UIBLUR"
grep -q 'pipeDown' "$UIBLUR"
grep -q 'pipeUp' "$UIBLUR"
grep -q 'vk_ui_blur_execute' "$UIBLUR_H"

# gamma-pass hook: blur runs after tonemap, before overlay compose
grep -q 'vk_ui_blur_execute' "$FRAME_END"
grep -q 'vk_ui_blur_has_work' "$FRAME_END"

# command stream wiring
grep -q 'RC_UI_FILTER' "$TR_BACKEND"
grep -q 'vk_ui_blur_begin_frame' "$TR_BACKEND"
grep -q 'RE_UIBackdropBlur' "$TR_CMDS"
grep -q 'RE_UIFilterLayer' "$TR_CMDS"
grep -q 're.UIBackdropBlur = RE_UIBackdropBlur' "$TR_INIT"
grep -q 'vk_ui_blur_init' "$TR_INIT"
grep -q 'vk_ui_blur_shutdown' "$TR_INIT"

# public API structures
grep -q 'uiFilterChain_t' "$TR_TYPES"
grep -q 'uiFilterOp_t' "$TR_TYPES"
grep -q 'uiBackdropFilter_t' "$TR_TYPES"
grep -q 'uiCompositorLayer_t' "$TR_TYPES"
grep -q 'UIBackdropBlur' "$TR_PUBLIC"
grep -q 'UIFilterLayer' "$TR_PUBLIC"

# client wrapper + CSS parsing (filter / backdrop-filter / -webkit-backdrop-filter)
grep -q 'SCR_UIBackdropBlur' "$UIFILTER"
grep -q 'SCR_UIFilterLayer' "$UIFILTER"
grep -q 'UIFilter_ParseChain' "$UIFILTER"
grep -q 'cl_uiFilter' "$UIFILTER"
grep -q '"filter"' "$UICSS"
grep -q '"backdrop-filter"' "$UICSS"
grep -q -- '"-webkit-backdrop-filter"' "$UICSS"

# JS bindings
grep -q 'hudBackdropBlur' "$JSDEBUG"
grep -q 'hudFilterBlurPic' "$JSDEBUG"

# shaders: linear-space blur chain (sRGB decode/encode helpers) + mask composite
test -f "$GLSL_DIR/ui_blur_common.glsl"
test -f "$GLSL_DIR/ui_blur_sample.frag"
test -f "$GLSL_DIR/ui_blur_gauss.frag"
test -f "$GLSL_DIR/ui_blur_down.frag"
test -f "$GLSL_DIR/ui_blur_up.frag"
test -f "$GLSL_DIR/ui_blur_composite.frag"
grep -q 'uib_srgb_to_linear' "$GLSL_DIR/ui_blur_common.glsl"
grep -q 'uib_linear_to_srgb' "$GLSL_DIR/ui_blur_common.glsl"
grep -q 'uib_rounded_rect_sd' "$GLSL_DIR/ui_blur_composite.frag"

# validation scene
test -f "$ROOT/examples/demo_game/mod/demo_ui_blur.cfg"
test -f "$ROOT/examples/demo_game/mod/scripts/js/demo_ui_blur.js"
grep -q 'ui_blurQuality' "$ROOT/examples/demo_game/mod/demo_ui_blur.cfg"
grep -q 'hudBackdropBlur' "$ROOT/examples/demo_game/mod/scripts/js/demo_ui_blur.js"

echo "test_ui_blur.sh: ok"
