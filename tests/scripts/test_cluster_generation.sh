#!/usr/bin/env bash
# Clustered Hybrid M2: generation publish + shared consumer assert.
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
DGB="$(idtech3_file renderers/vulkan/vk_deferred_gbuffer.c src/renderers/vulkan/vk_deferred_gbuffer.c)"

check "$FP" 'cluster_list_generation++' 'generation bumped after publish'
check "$FP" 'r_clusterForceStaleGeneration' 'stale generation cheat'
check "$FP" 'vk_cluster_assert_shared_consumers' 'assert implemented'
check "$FP" 'header/tile buffer handle' 'assert compares header handle'
check "$DGB" 'vk_cluster_assert_shared_consumers' 'deferred calls assert'
check "$FP" 'Cluster build failed' 'explicit fallback log'

if [[ $failures -ne 0 ]]; then echo "$failures check(s) failed"; exit 1; fi
echo "All cluster generation checks passed."
