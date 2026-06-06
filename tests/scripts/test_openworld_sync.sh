#!/usr/bin/env bash
# Open-world MP sector sync + renderer BSP stream smoke checks.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "[test_openworld_sync] checking sources..."
for f in \
	src/renderers/vulkan/tr_bsp_stream.c \
	src/server/sv_openworld.c \
	src/qcommon/cm_stream.c \
	src/client/cl_openworld.cpp
do
	test -f "$f" || { echo "missing $f"; exit 1; }
done

echo "[test_openworld_sync] grep API symbols..."
rg -q 'CS_ENGINE_OPENWORLD_SECTORS' src/game/bg_public.h
rg -q 'CM_Stream_BuildLoadedList' src/qcommon/cm_stream.c
rg -q 'SV_OpenWorld_SyncConfigstring' src/server/sv_openworld.c
rg -q 'sv_openWorldSync' src/server/sv_openworld.c
rg -q 'CL_OpenWorld_OnConfigstring' src/client/cl_openworld.cpp
rg -q 'cl_openWorldSync' src/client/cl_openworld.cpp
rg -q 'CL_OpenWorld_SyncUnloadRemoved' src/client/cl_openworld.cpp
rg -q 'cl_openWorldLastSync' src/client/cl_openworld.cpp
rg -q 'WorldOpen_UnloadSectorLayers' src/world/world_open.c
rg -q 'RE_BspStream_MergeSector' src/renderers/vulkan/tr_bsp_stream.c
rg -q 'R_BspStream_LoadSurfaceLumps' src/renderers/vulkan/tr_bsp_stream.c
rg -q 'R_BspStream_AddSurfaces' src/renderers/vulkan/tr_main.c
rg -q 'BspStreamMergeSector' src/renderers/common/tr_public.h
rg -q 'RE_BspStream_ClearAll' src/renderers/vulkan/tr_bsp_stream.c
rg -q 'r_bspStreamResident' src/renderers/vulkan/tr_bsp_stream.c
rg -q 'WorldResidency_SetServerCollisionAllowList' src/client/cl_openworld.cpp
rg -q 'WorldResidency_UpdateServerOrigins' src/server/sv_openworld.c
rg -q 'r_openWorldResidency' src/world/world_residency.c

echo "[test_openworld_sync] ok"
