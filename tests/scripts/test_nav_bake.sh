#!/usr/bin/env bash
# Open-world nav sector bake smoke checks.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "[test_nav_bake] checking sources..."
for f in \
	src/navigation/nav_recast.cpp \
	src/navigation/nav_recast.h \
	src/navigation/nav_bsp_extract.c \
	src/server/sv_openworld.c
do
	test -f "$f" || { echo "missing $f"; exit 1; }
done

echo "[test_nav_bake] grep API symbols..."
rg -q 'Nav_BakeSectorTile' src/navigation/nav_recast.cpp
rg -q 'Nav_BSP_ExtractFromSectorMap' src/navigation/nav_bsp_extract.c
rg -q 'nav_bake_sector' src/client/cl_openworld.cpp
rg -q 'SV_OpenWorld_Frame' src/server/sv_openworld.c
rg -q 'SV_OpenWorld_Init' src/server/sv_init.c
rg -q 'tileWidth = sectorTile' src/navigation/nav_recast.cpp

echo "[test_nav_bake] sector BSP prerequisite..."
test -f tests/data/openworld/maps/sector_0_0.bsp

echo "[test_nav_bake] ok"
