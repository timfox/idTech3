#!/usr/bin/env bash
# Clustered Hybrid M2: hybrid parity + inspect wiring (headless grep; GPU capture manual).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
failures=0

check() {
  if ! grep -q "$2" "$1" 2>/dev/null; then
    echo "FAIL: $3"; failures=$((failures + 1))
  else
    echo "PASS: $3"
  fi
}

PATH_C="$(idtech3_file renderers/vulkan/vk_render_path.c src/renderers/vulkan/vk_render_path.c)"
COMP="$(idtech3_file renderers/vulkan/shaders/glsl/deferred_lighting_composite.frag src/renderers/vulkan/shaders/glsl/deferred_lighting_composite.frag)"
FP="$(idtech3_file renderers/vulkan/vk_forward_plus.c src/renderers/vulkan/vk_forward_plus.c)"
GEN="$(idtech3_file renderers/vulkan/shaders/glsl/gen_frag.tmpl src/renderers/vulkan/shaders/glsl/gen_frag.tmpl)"
COMMON="$(idtech3_file renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl src/renderers/vulkan/shaders/glsl/deferred_lighting_common.glsl)"
CFG="$ROOT/config/demo_cluster_lab.cfg"
DOC="$ROOT/docs/CLUSTERED_LIGHTING.md"

check "$PATH_C" '"0", "8"' 'hybridCompare range 0-8'
check "$COMP" 'hybridCompare == 2u' 'composite abs RGB mode'
check "$COMP" 'hybridCompare == 6u' 'composite cluster mismatch mode'
check "$FP" 'cluster_inspect' 'cluster_inspect command'
check "$FP" 'hybrid_compare_status' 'hybrid_compare_status command'
check "$FP" 'r_clusterForceBuildFailure' 'force build failure cheat'
check "$FP" 'r_clusterForceOverflow' 'force overflow cheat'
check "$GEN" 'Cluster_FetchLightIndex' 'Forward+ uses shared fetch'
check "$COMMON" 'Cluster_FetchLightIndex' 'deferred uses shared fetch'
check "$GEN" 'fp_dbg >= 5.5' 'debug mode 6 Z-slice path'
check "$CFG" 'r_clusterCompactLists' 'demo lab cfg'
check "$DOC" 'Clustered Lighting' 'CLUSTERED_LIGHTING.md exists'

# Ownership gates must still pass.
bash "$ROOT/tests/scripts/test_render_path_ownership.sh"

if [[ $failures -ne 0 ]]; then echo "$failures check(s) failed"; exit 1; fi
echo "All hybrid cluster parity checks passed."
