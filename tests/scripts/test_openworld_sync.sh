#!/usr/bin/env bash
# Open-world MP sector sync + renderer BSP stream smoke checks.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
cd "$ROOT"

echo "[test_openworld_sync] checking sources..."
BSP_STREAM="$(idtech3_require_file renderers/vulkan/tr_bsp_stream.c src/renderers/vulkan/tr_bsp_stream.c)"
SV_OW="$(idtech3_require_file runtime/server/world/sv_openworld.c src/server/sv_openworld.c)"
CM_STREAM="$(idtech3_require_file engine/core/cm_stream.c src/qcommon/cm_stream.c)"
CL_OW="$(idtech3_require_file runtime/client/world/cl_openworld.cpp src/client/world/cl_openworld.cpp)"
BG="$(idtech3_require_file runtime/game/bg_public.h src/game/bg_public.h)"
WORLD_OPEN="$(idtech3_require_file modules/world/world_open.cpp src/world/world_open.cpp)"
TR_MAIN="$(idtech3_require_file renderers/vulkan/tr_main.c src/renderers/vulkan/tr_main.c)"
TR_PUB="$(idtech3_require_file renderers/common/tr_public.h src/renderers/common/tr_public.h)"
RESIDENCY="$(idtech3_require_file modules/world/world_residency.cpp src/world/world_residency.cpp)"

echo "[test_openworld_sync] grep API symbols..."
rg -q 'CS_ENGINE_OPENWORLD_SECTORS' "$BG"
rg -q 'CM_Stream_BuildLoadedList' "$CM_STREAM"
rg -q 'SV_OpenWorld_SyncConfigstring' "$SV_OW"
rg -q 'sv_openWorldSync' "$SV_OW"
rg -q 'CL_OpenWorld_OnConfigstring' "$CL_OW"
rg -q 'cl_openWorldSync' "$CL_OW"
rg -q 'CL_OpenWorld_SyncUnloadRemoved' "$CL_OW"
rg -q 'cl_openWorldLastSync' "$CL_OW"
rg -q 'WorldOpen_UnloadSectorLayers' "$WORLD_OPEN"
rg -q 'RE_BspStream_MergeSector' "$BSP_STREAM"
rg -q 'R_BspStream_LoadSurfaceLumps' "$BSP_STREAM"
rg -q 'R_BspStream_AddSurfaces' "$TR_MAIN"
rg -q 'BspStreamMergeSector' "$TR_PUB"
rg -q 'RE_BspStream_ClearAll' "$BSP_STREAM"
rg -q 'R_BspStream_RebuildVbo' "$BSP_STREAM"
rg -q 'R_BspStream_CompactLightmaps' "$BSP_STREAM"
rg -q 'r_bspStreamResident' "$BSP_STREAM"
rg -q 'WorldResidency_SetServerCollisionAllowList' "$CL_OW"
rg -q 'WorldResidency_UpdateServerOrigins' "$SV_OW"
rg -q 'r_openWorldResidency' "$RESIDENCY"

echo "[test_openworld_sync] ok"
