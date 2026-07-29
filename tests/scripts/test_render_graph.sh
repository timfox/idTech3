#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }

RG_H="renderers/vulkan/vk_render_graph.h"
RG_C="renderers/vulkan/vk_render_graph.c"
REG_C="renderers/vulkan/vk_pass_registry.c"
DOC="docs/RENDER_GRAPH.md"

[[ -f "$RG_H" ]] || fail "missing $RG_H"
[[ -f "$RG_C" ]] || fail "missing $RG_C"
[[ -f "$DOC" ]] || fail "missing $DOC"

rg -q 'vk_render_graph_declare_pass' "$RG_H" "$RG_C" || fail "declare API missing"
rg -q 'vk_render_graph_compile' "$RG_H" "$RG_C" || fail "compile API missing"
rg -q 'vk_render_graph_execute' "$RG_H" "$RG_C" || fail "execute API missing"
rg -q 'vk_render_graph_set_pass_executor' "$RG_H" "$RG_C" || fail "executor API missing"
rg -q 'vk_render_graph_frame_violation_count' "$RG_H" "$RG_C" || fail "per-frame violation API missing"
rg -q 's_rg.initialized && !s_rg.compiled' "$RG_C" || fail "derived graph counters must compile stale observations"
rg -q 'render_graph_status' "$RG_C" || fail "status command missing"
rg -q 'render_graph_dot' "$RG_C" || fail "DOT status command missing"
rg -q 'vk_render_graph_dot_f' "$RG_H" "$RG_C" || fail "DOT API missing"

rg -q 'vk_rg_add_dependency' "$RG_C" || fail "dependency builder missing"
rg -q 'vk_rg_access_name' "$RG_C" || fail "dependency access labels missing"
rg -q 'lastWriter' "$RG_C" || fail "producer tracking missing"
rg -q 'lastReader' "$RG_C" || fail "read-before-write hazard tracking missing"
rg -q 'cycle detected' "$RG_C" || fail "cycle detection missing"
rg -q 'unresolved read' "$RG_C" || fail "unresolved read validation missing"
rg -q 'frameViolations' "$RG_C" || fail "status must report per-frame violations"
rg -q 'totalViolations' "$RG_C" || fail "status must report cumulative violations"
rg -q 'indegree' "$RG_C" || fail "topological compile missing"
rg -q 'VK_RG_MAX_DEPS' "$RG_C" || fail "bounded dependency storage missing"
rg -q 'digraph vulkan_render_graph' "$RG_C" || fail "DOT output missing"
rg -q 'VK_SPINE_RES_VIRTUAL_GEOMETRY_INDIRECT' "$RG_C" || fail "virtual geometry graph import missing"

rg -q '#include "vk_render_graph.h"' "$REG_C" || fail "pass registry must include render graph"
rg -q 'vk_render_graph_init' "$REG_C" || fail "registry init must init graph"
rg -q 'vk_render_graph_declare_pass' "$REG_C" || fail "registry must declare passes to graph"
rg -q 'vk_render_graph_begin_frame' "$REG_C" || fail "frame begin must begin graph"
rg -q 'vk_render_graph_observe_pass' "$REG_C" || fail "pass begin must observe graph pass"
rg -q 'vk_render_graph_end_frame' "$REG_C" || fail "frame end must compile graph"
rg -q 'vk_render_graph_shutdown' "$REG_C" || fail "registry shutdown must shutdown graph"

rg -q 'real render graph core' "$DOC" || fail "docs must describe real graph core"
rg -q 'Topologically compiles' "$DOC" || fail "docs must describe topological compile"
rg -q 'render_graph_dot' "$DOC" || fail "docs must describe DOT output"
rg -q 'unresolved-read' "$DOC" || fail "docs must describe unresolved read validation"

echo "PASS: test_render_graph.sh"
