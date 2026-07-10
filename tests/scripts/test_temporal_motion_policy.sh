#!/usr/bin/env bash
# Wiring test: temporal motion policy (per-entity vs global unreliable motion).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
failures=0

check() {
  if ! grep -q "$2" "$1" 2>/dev/null; then
    echo "FAIL: $3"
    failures=$((failures + 1))
  else
    echo "PASS: $3"
  fi
}

TR_LOCAL="$(idtech3_file renderers/vulkan/tr_local.h src/renderers/vulkan/tr_local.h)"
VK_VIEW_STATE="$(idtech3_file renderers/vulkan/vk_view_state.c src/renderers/vulkan/vk_view_state.c)"
VK_POSTFX_PARAMS="$(idtech3_file renderers/vulkan/vk_postfx_params.c src/renderers/vulkan/vk_postfx_params.c)"
VK_FRAME_END="$(idtech3_file renderers/vulkan/vk_frame_end.c src/renderers/vulkan/vk_frame_end.c)"

check "$TR_LOCAL" 'r_temporalCpuSkinPrev' 'r_temporalCpuSkinPrev extern'
check "$VK_VIEW_STATE" 'vk_entity_poison_global_motion' 'global vs local motion split'
check "$VK_VIEW_STATE" 'vk_entity_has_gpu_deform_motion' 'GPU deform motion detect'
check "$VK_POSTFX_PARAMS" 'unreliableMotionThisFrame' 'postfx TAA confidence gate'
check "$VK_FRAME_END" 'allow_taa' 'TAA allow path present'

if grep -q '!vk.temporal.unreliableMotionThisFrame' "$VK_FRAME_END" 2>/dev/null; then
  echo "FAIL: vk_frame_end must not skip TAA on unreliableMotionThisFrame"
  failures=$((failures + 1))
else
  echo "PASS: TAA not whole-frame skipped on entity motion"
fi

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All temporal motion policy wiring checks passed."
