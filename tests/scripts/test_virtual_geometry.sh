#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

fail() { echo "FAIL: $*" >&2; exit 1; }

ML="$ROOT/renderers/vulkan/vk_meshlets.c"
REG_H="$ROOT/renderers/vulkan/vk_pass_registry.h"
REG_C="$ROOT/renderers/vulkan/vk_pass_registry.c"
STABLE="$ROOT/config/modern_vulkan_stable.cfg"
SAFE="$ROOT/config/gfx_safe.cfg"
DOC="$ROOT/docs/MESHLETS.md"

grep -q 'r_virtualGeometry = ri.Cvar_Get( "r_virtualGeometry", "1"' "$ML" || fail "virtual geometry master cvar must default on"
grep -q 'r_virtualGeometry && !r_virtualGeometry->integer' "$ML" || fail "virtual geometry cvar must gate meshlets"
grep -q 'vk_spine_pass_begin( VK_SPINE_PASS_VIRTUAL_GEOMETRY_CULL )' "$ML" || fail "meshlet cull must enter Spine"
grep -q 'vk_spine_pass_begin( VK_SPINE_PASS_VIRTUAL_GEOMETRY_DRAW )' "$ML" || fail "meshlet MDI draw must enter Spine"
grep -q 'vkCmdDrawIndexedIndirect' "$ML" || fail "virtual geometry production path must use indexed MDI"
grep -q 'meshShaderNVReady' "$ML" || fail "status must report mesh shader readiness"

grep -q 'VK_SPINE_RES_VIRTUAL_GEOMETRY_MESHLETS' "$REG_H" || fail "virtual geometry meshlet resource enum missing"
grep -q 'VK_SPINE_RES_VIRTUAL_GEOMETRY_INDIRECT' "$REG_H" || fail "virtual geometry indirect resource enum missing"
grep -q 'VK_SPINE_PASS_VIRTUAL_GEOMETRY_CULL' "$REG_H" || fail "virtual geometry cull pass enum missing"
grep -q 'VK_SPINE_PASS_VIRTUAL_GEOMETRY_DRAW' "$REG_H" || fail "virtual geometry draw pass enum missing"
grep -q '"virtual_geometry_cull"' "$REG_C" || fail "virtual geometry cull pass desc missing"
grep -q '"virtual_geometry_draw"' "$REG_C" || fail "virtual geometry draw pass desc missing"

grep -q 'seta r_virtualGeometry 1' "$STABLE" || fail "stable profile must enable virtual geometry"
grep -q 'seta r_meshlets 1' "$STABLE" || fail "stable profile must enable meshlets"
grep -q 'seta r_meshletsMdiDraw 1' "$STABLE" || fail "stable profile must enable MDI draw"
grep -q 'seta r_meshletsLod 1' "$STABLE" || fail "stable profile must enable meshlet LOD"
grep -q 'seta r_virtualGeometry 0' "$SAFE" || fail "safe profile must disable virtual geometry"

grep -q 'Virtual Geometry' "$DOC" || fail "meshlet docs must describe virtual geometry"
grep -q 'Northlight-inspired' "$DOC" || fail "meshlet docs must document inspiration scope"

echo "test_virtual_geometry.sh: ok"
