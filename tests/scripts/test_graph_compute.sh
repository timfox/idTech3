#!/usr/bin/env bash
# Sector graph + Vulkan graph compute spike wiring checks.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "[test_graph_compute] checking sources..."
for f in \
	src/world/sector_graph.cpp \
	src/world/sector_graph.h \
	src/renderers/vulkan/vk_graph_bfs.c \
	src/renderers/vulkan/shaders/glsl/graph/graph_bfs_expand.comp \
	src/world/world_residency.cpp
do
	test -f "$f" || { echo "missing $f"; exit 1; }
done

echo "[test_graph_compute] grep API symbols..."
rg -q 'SectorGraph_Init' src/world/sector_graph.cpp
rg -q 'SectorGraph_UpdateReachability' src/world/sector_graph.cpp
rg -q 'SectorGraph_IsReachable' src/world/sector_graph.cpp
rg -q 'SectorGraph_StreamReachEnabled' src/world/world_residency.cpp
rg -q 'r_graphStreamReach' src/world/sector_graph.cpp
rg -q 'r_graphCompute' src/world/sector_graph.cpp
rg -q 'R_GraphBfs_Init' src/renderers/vulkan/vk_graph_bfs.c
rg -q 'graph_bfs_expand_cs' scripts/compile_shaders.sh
rg -q 'graph_bfs_expand_cs' src/renderers/vulkan/vk.h
rg -q 'graph_reach_test' src/world/sector_graph.cpp
rg -q 'SectorGraph_GetHopDistance' src/world/sector_graph.cpp
rg -q 'graph_bfs_crossover' src/world/sector_graph.cpp
rg -q 'ClusterGraph_RebuildFromMap' src/qcommon/cluster_graph.cpp
rg -q 'r_graphClusterReach' src/qcommon/cluster_graph.cpp
rg -q 'ClusterGraph_UpdateReachability' src/client/world/cl_openworld.cpp

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
