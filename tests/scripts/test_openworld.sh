#!/usr/bin/env bash
# Open-world streaming smoke checks (BSP sectors + per-chunk nav + billboard scatter).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "[test_openworld] checking sources..."
for f in \
	src/world/world_open.cpp \
	src/world/world_open.h \
	src/client/world/cl_openworld.cpp \
	src/qcommon/cm_stream.c
do
	test -f "$f" || { echo "missing $f"; exit 1; }
done

echo "[test_openworld] grep API symbols..."
rg -q 'WorldOpen_UpdateView' src/world/world_open.cpp
rg -q 'CM_Stream_UpdateView' src/qcommon/cm_stream.c
rg -q 'CM_Stream_MergeSector' src/qcommon/cm_stream_merge.c
rg -q 'CM_Stream_TraceMerged' src/qcommon/cm_stream_merge.c
rg -q 'cm_stream_merge.c' cmake/IdTech3QcommonExtensions.cmake || rg -q 'cm_stream_merge.c' CMakeLists.txt
rg -q 'Nav_LoadSectorTile' src/navigation/nav_recast.cpp
rg -q 'Nav_CreateOpenWorldMesh' src/navigation/nav_recast.cpp
rg -q 'CL_OpenWorld_Frame' src/client/world/cl_openworld.cpp
rg -q 'CL_OpenWorld_Init' src/client/core/cl_main.c
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
rg -q 'openworld_smoke' src/qcommon/com_openworld_smoke.c

echo "[test_openworld] demo cfg..."
test -f examples/demo_game/mod/demo_openworld.cfg
test -f examples/demo_game/mod/sprites/sector_0_0.ents

echo "[test_openworld] ok"
