#!/usr/bin/env bash
# Wiring test: deferred G-buffer fill + lighting + composite (r_renderMode 1).
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

DGB="$(idtech3_file renderers/vulkan/vk_deferred_gbuffer.c src/renderers/vulkan/vk_deferred_gbuffer.c)"
LIT="$(idtech3_file renderers/vulkan/shaders/glsl/deferred_lighting.comp src/renderers/vulkan/shaders/glsl/deferred_lighting.comp)"
COMP="$(idtech3_file renderers/vulkan/shaders/glsl/deferred_lighting_composite.frag src/renderers/vulkan/shaders/glsl/deferred_lighting_composite.frag)"
TR_INIT="$(idtech3_file renderers/vulkan/tr_init.c src/renderers/vulkan/tr_init.c)"
TR_BACKEND="$(idtech3_file renderers/vulkan/tr_backend.c src/renderers/vulkan/tr_backend.c)"

check "$TR_INIT" 'r_deferredSpecular = ri.Cvar_Get' 'r_deferredSpecular cvar'
check "$TR_INIT" 'r_renderMode 1/2' 'G-buffer cvar documents mode 1/2 sidecar'
check "$DGB" 'r_renderMode->integer == 1 || r_renderMode->integer == 2' 'G-buffer active in mode 1/2'
check "$DGB" 'r_renderMode->integer == 1 && r_forwardPlus' 'deferred lighting remains mode 1 only'
check "$DGB" 'vk_deferred_composite_push_t' 'composite push constants'
check "$DGB" 'deferred_gbuffer_albedo_view' 'composite scene base descriptor'
check "$COMP" 'sceneBaseTex' 'composite scene base sampler'
check "$COMP" 'pc.additive' 'composite additive blend'
check "$LIT" 'pc.specular' 'deferred lighting specular toggle'
check "$LIT" 'specularAcc' 'deferred specular accumulation'
check "$TR_BACKEND" 'vk_deferred_lighting_apply_after_geometry' 'backend lighting hook'
check "$ROOT/scripts/compile_shaders.sh" 'deferred_lighting_composite_fs' 'composite shader registered'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All deferred lighting wiring checks passed."
