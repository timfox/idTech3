#!/usr/bin/env bash
# Wiring test: CBT terrain draw path + splat hooks.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

TERRAIN_C="${ROOT}/renderers/vulkan/vk_terrain.c"
TERRAIN_H="${ROOT}/renderers/vulkan/vk_terrain.h"
BACKEND="${ROOT}/renderers/vulkan/tr_backend.c"
DOC="${ROOT}/docs/CBT_TERRAIN.md"
SHADER="${ROOT}/renderers/vulkan/tr_shader.c"
FRAG="${ROOT}/renderers/vulkan/shaders/glsl/terrain/terrain.frag"
DEMO_CFG="${ROOT}/examples/demo_game/mod/demo_cbt_splat.cfg"

[ -f "$TERRAIN_C" ] || fail "missing vk_terrain.c"
[ -f "$TERRAIN_H" ] || fail "missing vk_terrain.h"
[ -f "$DOC" ] || fail "missing CBT_TERRAIN.md"
rg -q 'r_cbtTerrain' "$TERRAIN_C" || fail "r_cbtTerrain cvar missing"
rg -q 'cbt_status|terrain_status' "$TERRAIN_C" || fail "cbt_status/terrain_status missing"
rg -q 'cbt_load' "$TERRAIN_C" || fail "cbt_load missing"
rg -q 'CBTerrain_HasMetadata' "$TERRAIN_C" || fail "metadata gate missing"
rg -q 's_heightSamples|CBTerrain_SampleHeightUV' "$TERRAIN_C" || fail "heightmap sampling missing"
rg -q 'CBTerrain_UpdateLOD' "$TERRAIN_C" || fail "LOD update missing"
rg -q 'CBTerrain_Frame' "$BACKEND" || fail "backend must call CBTerrain_Frame"
rg -q 'qvkCmdDispatch' "$TERRAIN_C" || fail "compute dispatch missing"
rg -q 'materialBlend splat|splatMap' "$SHADER" || fail "splat shader keywords missing"
rg -q 'use_splat|splatMap' "$FRAG" || fail "terrain.frag splat path missing"
rg -q 'r_cbtTerrain|cbt_load|tiled heightfield|Raster Ultra 1.14' "$DOC" || fail "docs incomplete"
[ -f "$DEMO_CFG" ] || fail "missing demo_cbt_splat.cfg"

echo "test_cbt_terrain: passed"
