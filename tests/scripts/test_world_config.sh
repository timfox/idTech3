#!/usr/bin/env bash
# Smoke checks for World Config (map-state transitions).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

test -f "$ROOT/docs/WORLD_CONFIG.md" || fail "docs missing"
test -f "$ROOT/modules/world/world_config.cpp" || fail "world_config.cpp missing"
test -f "$ROOT/modules/world/world_config.h" || fail "world_config.h missing"
test -f "$ROOT/runtime/server/world/sv_world_config.c" || fail "sv_world_config.c missing"
test -f "$ROOT/tests/data/world/testmap.wcfg" || fail "fixture wcfg missing"
test -f "$ROOT/config/vulkan_overlay_world_config.cfg" || fail "overlay missing"

rg -q 'CS_ENGINE_WORLD_CONFIG' "$ROOT/runtime/game/bg_public.h" || fail "CS_ENGINE_WORLD_CONFIG"
rg -q 'WorldConfig_SetActive' "$ROOT/modules/world/world_config.cpp" || fail "SetActive"
rg -q 'info_director_spawn' "$ROOT/runtime/game/systems/g_entity_bridge.c" || fail "director spawn wire"
rg -q 'r_worldConfigEpoch' "$ROOT/renderers/vulkan/vk_temporal.c" || fail "temporal epoch"
rg -q 'WorldConfig_FormatSectorBsp|WorldConfig_ResolveReadable' "$ROOT/engine/core/cm_stream.c" || fail "cm_stream hook"
rg -q 'R_BspStream_FormatSectorMap|r_worldConfigGeoSuffix' "$ROOT/renderers/vulkan/tr_bsp_stream.c" || fail "bsp stream hook"
rg -q 'r_worldConfigNavSuffix' "$ROOT/modules/navigation/nav_recast.cpp" || fail "nav hook"
rg -q 'CL_WorldConfig_OnConfigstring' "$ROOT/runtime/client/world/cl_openworld.cpp" || fail "client CS apply"
rg -q 'WorldConfig' "$ROOT/runtime/game/scripting/g_lua_registration.inc" || fail "Lua WorldConfig"
rg -q 'world_config\.cpp' "$ROOT/cmake/IdTech3QcommonExtensions.cmake" || fail "cmake source"

echo "OK: World Config smoke checks passed"
