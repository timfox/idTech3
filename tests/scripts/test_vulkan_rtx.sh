#!/usr/bin/env bash
# Wiring test: Vulkan RTX BLAS/TLAS + hybrid frame path scaffolding.
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

check "$ROOT/src/renderers/vulkan/extensions/rtx/vk_rtx.c" 'r_rtxTlasUpdate' 'TLAS update cvar wiring'
check "$ROOT/src/renderers/vulkan/extensions/rtx/vk_rtx.c" 'VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR' 'TLAS UPDATE build mode'
check "$ROOT/src/renderers/vulkan/extensions/rtx/vk_rtx.c" 'ALLOW_UPDATE_BIT_KHR' 'TLAS ALLOW_UPDATE flag'
check "$ROOT/src/renderers/vulkan/extensions/rtx/vk_rtx.c" 'rtx_status' 'rtx_status console command'
check "$ROOT/src/renderers/vulkan/extensions/rtx/vk_rtx.c" 'vk_rtx_rebuild_entity_tlas' 'entity TLAS refresh path'
check "$ROOT/src/renderers/vulkan/extensions/rtx/vk_rtx_world.c" 'vk_rtx_world_pack' 'world BLAS pack'
check "$ROOT/src/renderers/vulkan/vk_render_pass.c" 'vk_hybrid1_active' 'hybrid frame path priority'
check "$ROOT/src/renderers/vulkan/shaders/glsl/rtx_demo.rchit" 'gl_InstanceCustomIndexEXT' 'instance-aware closest-hit'
check "$ROOT/src/renderers/vulkan/tr_init.c" 'r_rtxTlasUpdate' 'r_rtxTlasUpdate cvar registration'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All Vulkan RTX wiring checks passed."
