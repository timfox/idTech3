#!/usr/bin/env bash
# Clustered Hybrid M2: overflow policy + capacity cvars.
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

FP="$(idtech3_file renderers/vulkan/vk_forward_plus.c src/renderers/vulkan/vk_forward_plus.c)"
COMP="$(idtech3_file renderers/vulkan/shaders/glsl/forward_plus_tile_cull.comp src/renderers/vulkan/shaders/glsl/forward_plus_tile_cull.comp)"

check "$FP" 'r_clusterOverflowPolicy' 'overflow policy cvar'
check "$FP" 'r_clusterMaxIndices' 'max indices cvar'
check "$FP" 'r_clusterMaxLightsPerCluster' 'max lights per cluster cvar'
check "$FP" 'cluster_status' 'cluster_status command'
check "$COMP" 'overflowPolicy' 'compute overflow policy'
check "$COMP" 'atomicAdd' 'compact atomic index alloc'
check "$COMP" 'MAX_COMPACT_PER_TILE' 'compact cap 32'
check "$COMP" 'ClusterFill' 'ClusterFill marker'

if [[ $failures -ne 0 ]]; then echo "$failures check(s) failed"; exit 1; fi
echo "All cluster overflow checks passed."
