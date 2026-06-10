#!/usr/bin/env bash
# Wiring test: temporal motion policy (per-entity vs global unreliable motion).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

check() {
  if ! grep -q "$2" "$1" 2>/dev/null; then
    echo "FAIL: $3"
    failures=$((failures + 1))
  else
    echo "PASS: $3"
  fi
}

check "$ROOT/src/renderers/vulkan/tr_local.h" 'r_temporalCpuSkinPrev' 'r_temporalCpuSkinPrev extern'
check "$ROOT/src/renderers/vulkan/vk_view_state.c" 'vk_entity_poison_global_motion' 'global vs local motion split'
check "$ROOT/src/renderers/vulkan/vk_view_state.c" 'vk_entity_has_gpu_deform_motion' 'GPU deform motion detect'
check "$ROOT/src/renderers/vulkan/vk_postfx_params.c" 'unreliableMotionThisFrame' 'postfx TAA confidence gate'
check "$ROOT/src/renderers/vulkan/vk_frame_end.c" 'allow_taa' 'TAA allow path present'

if grep -q '!vk.temporal.unreliableMotionThisFrame' "$ROOT/src/renderers/vulkan/vk_frame_end.c" 2>/dev/null; then
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
