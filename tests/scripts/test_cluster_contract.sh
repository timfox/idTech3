#!/usr/bin/env bash
# Clustered Hybrid M2: cluster contract + math wiring gates.
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

CONTRACT="$(idtech3_file renderers/vulkan/vk_cluster_contract.h src/renderers/vulkan/vk_cluster_contract.h)"
GLSL="$(idtech3_file renderers/vulkan/shaders/glsl/cluster_contract.glsl src/renderers/vulkan/shaders/glsl/cluster_contract.glsl)"
MATH="$(idtech3_file renderers/vulkan/vk_cluster_math.cpp src/renderers/vulkan/vk_cluster_math.cpp)"
FP_CLUSTER="$(idtech3_file renderers/vulkan/shaders/glsl/forward_plus_cluster.glsl src/renderers/vulkan/shaders/glsl/forward_plus_cluster.glsl)"
FP_CULL="$(idtech3_file renderers/vulkan/shaders/glsl/forward_plus_tile_cull.comp src/renderers/vulkan/shaders/glsl/forward_plus_tile_cull.comp)"
LIST="$(idtech3_file renderers/vulkan/shaders/glsl/cluster_light_list.glsl src/renderers/vulkan/shaders/glsl/cluster_light_list.glsl)"
UNIT="$ROOT/tests/unit/test_cluster_math.c"
DOC="$ROOT/docs/CLUSTERED_LIGHTING.md"

check "$CONTRACT" 'gpuClusterHeader_t' 'C header has gpuClusterHeader_t'
check "$CONTRACT" 'gpuClusterParams_t' 'C header has gpuClusterParams_t'
check "$CONTRACT" 'Cluster_DeriveLogZScaleBias' 'C declares log Z derive'
check "$CONTRACT" 'Cluster_LightSliceSpan' 'C declares light slice span'
check "$GLSL" 'Cluster_ViewDepthToSlice' 'GLSL ViewDepthToSlice'
check "$GLSL" 'Cluster_IndexFromPixelAndViewDepth' 'GLSL IndexFromPixel'
check "$GLSL" 'Cluster_LightOverlapsSlice' 'GLSL light/slice overlap'
check "$GLSL" 'Cluster_LightSliceSpan' 'GLSL light slice span'
check "$MATH" 'Cluster_DeriveLogZScaleBias' 'CPU math implements derive'
check "$MATH" 'Cluster_IndexFromTileAndSlice' 'CPU math implements tile/slice index'
check "$MATH" 'Cluster_LightSliceSpan' 'CPU math implements light span'
check "$FP_CLUSTER" 'cluster_contract.glsl' 'forward_plus_cluster wraps contract'
check "$FP_CLUSTER" 'fp_light_slice_span' 'forward_plus_cluster exposes light span wrapper'
check "$FP_CULL" 'fp_light_slice_span' 'compute culler bins by light slice span'
check "$LIST" 'Cluster_FetchLightIndex' 'shared light-list fetch'
check "$UNIT" 'Cluster_LightSliceSpan' 'unit covers light slice spans'
check "$DOC" 'r_clusterZFar' 'docs mention zFar policy'
check "$ROOT/CMakeLists.txt" 'unit_cluster_math' 'CMake registers unit_cluster_math'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi
echo "All cluster contract checks passed."
