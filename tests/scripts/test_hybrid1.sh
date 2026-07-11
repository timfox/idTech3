#!/usr/bin/env bash
# Hybrid Rendering 1 scaffolding checks
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

HYBRID="$(idtech3_file renderers/vulkan/extensions/rtx/vk_hybrid1.c src/renderers/vulkan/extensions/rtx/vk_hybrid1.c)"
TR_INIT="$(idtech3_file renderers/vulkan/tr_init.c src/renderers/vulkan/tr_init.c)"
SPIRV="$(idtech3_file renderers/vulkan/extensions/rtx/vk_hybrid1_spirv.inc src/renderers/vulkan/extensions/rtx/vk_hybrid1_spirv.inc)"
HIT="$(idtech3_file renderers/vulkan/shaders/glsl/hybrid1/hybrid1_hit.glsl src/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_hit.glsl)"
DIFFUSE="$(idtech3_file renderers/vulkan/shaders/glsl/hybrid1/hybrid1_diffuse.rgen src/renderers/vulkan/shaders/glsl/hybrid1/hybrid1_diffuse.rgen)"

grep -q 'vk_hybrid1_record_pass' "$HYBRID"
grep -q 'r_hybrid1_historyClamp' "$TR_INIT"
grep -q 'hybrid1_temporal.comp' "$ROOT/scripts/compile_shaders.sh"
test -f "$ROOT/docs/HYBRID_RENDERING1.md"
test -f "$SPIRV"
test -f "$HIT"
grep -q 'WorldAlbedoSSBO' "$HIT"
grep -q 'r_hybrid1_ibl' "$TR_INIT"
grep -q 'r_hybrid1_motion' "$TR_INIT"
grep -q 'hybrid1_reset' "$HYBRID"
grep -q 'hybrid1_reload' "$HYBRID"
grep -q 'HYBRID1_ConsumeCvarResets' "$HYBRID"
grep -q 'vk_rtx_bind_world_albedo_ssbo' "$HYBRID"
test -f "$DIFFUSE"
test -f "$ROOT/examples/demo_game/mod/demo_hybrid1.cfg"

echo "test_hybrid1.sh: ok"
