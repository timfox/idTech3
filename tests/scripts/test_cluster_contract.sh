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
check "$CONTRACT" 'VK_CLUSTER_PARAMS_BYTES  56u' 'C/GPU params ABI is 56 bytes'
check "$CONTRACT" 'size must match the GLSL ClusterParams ABI' 'C header guards params ABI size'
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
check "$ROOT/renderers/vulkan/vk_forward_plus.c" 'vk_fp_sync_cluster_stats' 'cluster diagnostics synchronize GPU metadata before reading it'
check "$ROOT/renderers/vulkan/vk_forward_plus.c" 'qvkQueueWaitIdle' 'cluster diagnostics wait for cull completion'
check "$ROOT/renderers/vulkan/vk_forward_plus.c" 'auto-size from clusters' 'cluster index pool documents automatic capacity sizing'
check "$ROOT/renderers/vulkan/vk_forward_plus.c" 'total_clusters \* (uint64_t)max_lights' 'cluster index pool scales with configured cluster occupancy'
check "$ROOT/renderers/vulkan/vk_forward_plus.c" 'four-pass headroom' 'cluster index pool accounts for unified multi-pass culls'
check "$ROOT/renderers/vulkan/vk_forward_plus.c" 'srcAccessMask = VK_ACCESS_HOST_WRITE_BIT' 'cluster metadata reset is visible before GPU atomics'
check "$ROOT/renderers/vulkan/vk_forward_plus.c" 'avgStoredOcc' 'cluster diagnostics report final stored occupancy separately from allocation cursor'
check "$LIST" 'Cluster_FetchLightIndex' 'shared light-list fetch'
check "$UNIT" 'Cluster_LightSliceSpan' 'unit covers light slice spans'
check "$DOC" 'r_clusterZFar' 'docs mention zFar policy'
check "$DOC" 'Olsson' 'docs identify clustered-shading reference'
check "$ROOT/docs/CLUSTERED_LIGHTING.md" 'normal-cone clustering' 'normal-cluster follow-up is explicit'
check "$DOC" 'cluster_transparent_mark.comp' 'transparent active-cluster proof is documented'
check "$ROOT/renderers/vulkan/vk_forward_plus.c" 'r_clusterTransparentPrepass' 'transparent prepass ownership gate is explicit'
check "$ROOT/renderers/vulkan/vk_forward_plus.c" 'vk_cluster_transparent_begin_frame' 'transparent lifecycle resets at frame start'
check "$ROOT/renderers/vulkan/tr_backend.c" 'vk_cluster_transparent_note_submission' 'transparent owner is recorded at submission'
check "$ROOT/renderers/vulkan/tr_backend.c" 'vk_cluster_transparent_note_candidate' 'transparent candidates are counted at filter entry'
check "$ROOT/renderers/vulkan/vk_forward_plus.c" 'additiveAccepted' 'transparent additive ownership is reported'
check "$ROOT/renderers/vulkan/vk_forward_plus.c" 's_transparentFrameNumber == tr.frameCount' 'transparent ledger survives portal/stereo views'
check "$ROOT/renderers/vulkan/shaders/glsl/cluster_transparent_mark.comp" 'atomicOr' 'transparent proof marks active clusters atomically'
check "$ROOT/scripts/compile_shaders.sh" 'cluster_transparent_mark.comp' 'transparent proof shader is compiled'
check "$ROOT/CMakeLists.txt" 'unit_cluster_math' 'CMake registers unit_cluster_math'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi
echo "All cluster contract checks passed."
