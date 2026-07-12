#!/usr/bin/env bash
# Temporal / spatial upscale wiring checks
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

UPSCALE="$(idtech3_file renderers/vulkan/vk_upscale.c src/renderers/vulkan/vk_upscale.c)"
TR_INIT="$(idtech3_file renderers/vulkan/tr_init.c src/renderers/vulkan/tr_init.c)"
TR_MAIN="$(idtech3_file renderers/vulkan/tr_main.c src/renderers/vulkan/tr_main.c)"
TAA="$(idtech3_file renderers/vulkan/shaders/glsl/taa.frag src/renderers/vulkan/shaders/glsl/taa.frag)"
FRAME_END="$(idtech3_file renderers/vulkan/vk_frame_end.c src/renderers/vulkan/vk_frame_end.c)"

grep -q 'R_Upscale_WantTemporal' "$UPSCALE"
grep -q 'upscale_status' "$UPSCALE"
grep -q 'r_upscaleSharpness' "$UPSCALE"
grep -q 'R_Upscale_ApplyProjectionJitter' "$TR_MAIN"
grep -q 'R_Upscale_ApplyRenderScaleDefaults' "$TR_INIT"
grep -q 'R_Upscale_WantTemporal' "$FRAME_END"
grep -q 'lutParams.zw' "$TAA"
grep -q 'Halton' "$UPSCALE"
grep -q 'r_upscaleDisplayHistory' "$UPSCALE"
grep -q 'R_Upscale_WantDisplayHistory' "$UPSCALE"
FB="$(idtech3_file renderers/vulkan/vk_framebuffers.c src/renderers/vulkan/vk_framebuffers.c)"
grep -q 'taa history' "$FB"
grep -q 'Independent of SMAA\|independent of SMAA\|without r_ext_smaa\|TAA history FBs' "$FB"
test -f "$ROOT/examples/demo_game/mod/demo_upscale.cfg"
grep -q 'r_upscale' "$ROOT/examples/demo_game/mod/demo_upscale.cfg"

echo "test_upscale.sh: ok"
