#!/usr/bin/env bash
# Sector graph + Vulkan graph compute spike wiring checks.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
cd "$ROOT"

echo "[test_graph_compute] checking sources..."
SG="$(idtech3_require_file modules/world/sector_graph.cpp src/world/sector_graph.cpp)"
idtech3_require_file modules/world/sector_graph.h src/world/sector_graph.h >/dev/null
BFS="$(idtech3_require_file renderers/vulkan/vk_graph_bfs.c src/renderers/vulkan/vk_graph_bfs.c)"
idtech3_require_file renderers/vulkan/shaders/glsl/graph/graph_bfs_expand.comp src/renderers/vulkan/shaders/glsl/graph/graph_bfs_expand.comp >/dev/null
RES="$(idtech3_require_file modules/world/world_residency.cpp src/world/world_residency.cpp)"
VK_H="$(idtech3_file renderers/vulkan/vk.h src/renderers/vulkan/vk.h)"
CG="$(idtech3_file engine/core/cluster_graph.cpp src/qcommon/cluster_graph.cpp)"
CL_OW="$(idtech3_file runtime/client/world/cl_openworld.cpp src/client/world/cl_openworld.cpp)"

echo "[test_graph_compute] grep API symbols..."
rg -q 'SectorGraph_Init' "$SG"
rg -q 'SectorGraph_UpdateReachability' "$SG"
rg -q 'SectorGraph_IsReachable' "$SG"
rg -q 'SectorGraph_StreamReachEnabled' "$RES"
rg -q 'r_graphStreamReach' "$SG"
rg -q 'r_graphCompute' "$SG"
rg -q 'R_GraphBfs_Init' "$BFS"
rg -q 'graph_bfs_expand_cs' scripts/compile_shaders.sh
rg -q 'graph_bfs_expand_cs' "$VK_H"
rg -q 'graph_reach_test' "$SG"
rg -q 'SectorGraph_GetHopDistance' "$SG"
rg -q 'graph_bfs_crossover' "$SG"
rg -q 'ClusterGraph_RebuildFromMap' "$CG"
rg -q 'r_graphClusterReach' "$CG"
rg -q 'ClusterGraph_UpdateReachability' "$CL_OW"

if [[ -x "${ROOT}/build-vk-Release/unit_sector_graph" ]]; then
	echo "[test_graph_compute] running unit_sector_graph..."
	"${ROOT}/build-vk-Release/unit_sector_graph"
elif [[ -x "${ROOT}/build/unit_sector_graph" ]]; then
	"${ROOT}/build/unit_sector_graph"
else
	echo "[test_graph_compute] unit_sector_graph not built; symbol checks only"
fi

if [[ -x "${ROOT}/build-vk-Release/unit_cluster_graph" ]]; then
	echo "[test_graph_compute] running unit_cluster_graph..."
	"${ROOT}/build-vk-Release/unit_cluster_graph"
elif [[ -x "${ROOT}/build/unit_cluster_graph" ]]; then
	"${ROOT}/build/unit_cluster_graph"
else
	echo "[test_graph_compute] unit_cluster_graph not built; symbol checks only"
fi

echo "[test_graph_compute] ok"
