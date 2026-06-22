#!/usr/bin/env bash
# Hybrid Rendering 1 scaffolding checks
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

grep -q 'vk_hybrid1_record_pass' "$ROOT/src/renderers/vulkan/extensions/rtx/vk_hybrid1.c"
grep -q 'r_hybrid1_historyClamp' "$ROOT/src/renderers/vulkan/tr_init.c"
grep -q 'hybrid1_temporal.comp' "$ROOT/scripts/compile_shaders.sh"
test -f "$ROOT/docs/HYBRID_RENDERING1.md"
test -f "$ROOT/src/renderers/vulkan/extensions/rtx/vk_hybrid1_spirv.inc"
test -f "$ROOT/src/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_hit.glsl"
grep -q 'r_hybrid1_ibl' "$ROOT/src/renderers/vulkan/tr_init.c"
grep -q 'r_hybrid1_motion' "$ROOT/src/renderers/vulkan/tr_init.c"
grep -q 'hybrid1_reset' "$ROOT/src/renderers/vulkan/extensions/rtx/vk_hybrid1.c"
grep -q 'hybrid1_reload' "$ROOT/src/renderers/vulkan/extensions/rtx/vk_hybrid1.c"
grep -q 'HYBRID1_ConsumeCvarResets' "$ROOT/src/renderers/vulkan/extensions/rtx/vk_hybrid1.c"
test -f "$ROOT/src/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_diffuse.rgen"
test -f "$ROOT/examples/demo_game/mod/demo_hybrid1.cfg"

echo "test_hybrid1.sh: ok"
