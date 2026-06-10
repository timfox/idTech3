#!/usr/bin/env bash
# Wiring test: deferred G-buffer fill + lighting + composite (r_renderMode 1).
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

DGB="$ROOT/src/renderers/vulkan/vk_deferred_gbuffer.c"
LIT="$ROOT/src/renderers/vulkan/shaders/glsl/deferred_lighting.comp"
COMP="$ROOT/src/renderers/vulkan/shaders/glsl/deferred_lighting_composite.frag"

check "$ROOT/src/renderers/vulkan/tr_init.c" 'r_deferredSpecular = ri.Cvar_Get' 'r_deferredSpecular cvar'
check "$DGB" 'vk_deferred_composite_push_t' 'composite push constants'
check "$DGB" 'deferred_gbuffer_albedo_view' 'composite scene base descriptor'
check "$COMP" 'sceneBaseTex' 'composite scene base sampler'
check "$COMP" 'pc.additive' 'composite additive blend'
check "$LIT" 'pc.specular' 'deferred lighting specular toggle'
check "$LIT" 'specularAcc' 'deferred specular accumulation'
check "$ROOT/src/renderers/vulkan/tr_backend.c" 'vk_deferred_lighting_apply_after_geometry' 'backend lighting hook'
check "$ROOT/scripts/compile_shaders.sh" 'deferred_lighting_composite_fs' 'composite shader registered'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All deferred lighting wiring checks passed."
