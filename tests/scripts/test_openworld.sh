#!/usr/bin/env bash
# Open-world streaming smoke checks (BSP sectors + per-chunk nav + billboard scatter).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
cd "$ROOT"

echo "[test_openworld] checking sources..."
WORLD_OPEN="$(idtech3_require_file modules/world/world_open.cpp src/world/world_open.cpp)"
idtech3_require_file modules/world/world_open.h src/world/world_open.h >/dev/null
CL_OW="$(idtech3_require_file runtime/client/world/cl_openworld.cpp src/client/world/cl_openworld.cpp)"
CM_STREAM="$(idtech3_require_file engine/core/cm_stream.c src/qcommon/cm_stream.c)"
CM_MERGE="$(idtech3_require_file engine/core/cm_stream_merge.c src/qcommon/cm_stream_merge.c)"
NAV="$(idtech3_require_file modules/navigation/nav_recast.cpp src/navigation/nav_recast.cpp)"
CL_MAIN="$(idtech3_require_file runtime/client/core/cl_main.c src/client/core/cl_main.c)"
SMOKE="$(idtech3_require_file engine/core/com_openworld_smoke.c src/qcommon/com_openworld_smoke.c)"

echo "[test_openworld] grep API symbols..."
rg -q 'WorldOpen_UpdateView' "$WORLD_OPEN"
rg -q 'CM_Stream_UpdateView' "$CM_STREAM"
rg -q 'CM_Stream_MergeSector' "$CM_MERGE"
rg -q 'CM_Stream_TraceMerged' "$CM_MERGE"
rg -q 'cm_stream_merge.c' cmake/IdTech3QcommonExtensions.cmake || rg -q 'cm_stream_merge.c' CMakeLists.txt
rg -q 'Nav_LoadSectorTile' "$NAV"
rg -q 'Nav_CreateOpenWorldMesh' "$NAV"
rg -q 'CL_OpenWorld_Frame' "$CL_OW"
rg -q 'CL_OpenWorld_Init' "$CL_MAIN"
rg -q 'world_open.cpp' cmake/IdTech3QcommonExtensions.cmake || rg -q 'world_open.cpp' CMakeLists.txt
rg -q 'cl_openworld.cpp' cmake/client/ClientExtensionSources.cmake || rg -q 'cl_openworld.cpp' CMakeLists.txt

echo "[test_openworld] scatter fixture..."
test -f tests/data/openworld/sprites/sector_0_0.ents
rg -q 'misc_billboard' tests/data/openworld/sprites/sector_0_0.ents

echo "[test_openworld] sector BSP fixture..."
test -f tests/data/openworld/maps/sector_0_0.bsp
test -f tests/data/openworld/maps/open_void.bsp
test -f scripts/tools/gen_hub_bsp.py
test -f scripts/tools/gen_sector_bsp.py
test -f tests/data/openworld/base/default.cfg
test -f scripts/bake_openworld_nav.sh
rg -q 'openworld_smoke' "$SMOKE"

echo "[test_openworld] demo cfg..."
test -f examples/demo_game/mod/demo_openworld.cfg
test -f examples/demo_game/mod/sprites/sector_0_0.ents
rg -q 'bsp_stream_status' examples/demo_game/mod/demo_openworld.cfg
rg -q 'r_bspStreamLod' examples/demo_game/mod/demo_openworld.cfg
STREAM="$(idtech3_require_file renderers/vulkan/tr_bsp_stream.c src/renderers/vulkan/tr_bsp_stream.c)"
rg -q 'bsp_stream_status' "$STREAM"
rg -q 'BspStream_Status_f' "$STREAM"

echo "[test_openworld] ok"
