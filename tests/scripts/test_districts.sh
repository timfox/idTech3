#!/usr/bin/env bash
# World districts + proxy mesh smoke checks (FreeUSD manifest path).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "[test_districts] checking sources..."
for f in \
	src/world/world_district.cpp \
	src/world/world_district.h \
	src/client/world/cl_district.cpp \
	src/client/world/cl_district.h
do
	test -f "$f" || { echo "missing $f"; exit 1; }
done

echo "[test_districts] grep API symbols..."
rg -q 'WorldDistrict_Init' src/world/world_district.cpp
rg -q 'WorldDistrict_UpdateView' src/world/world_district.cpp
rg -q 'WorldDistrict_Import' src/world/world_district.cpp
rg -q 'CL_District_Init' src/client/world/cl_district.cpp
rg -q 'CL_District_Frame' src/client/world/cl_district.cpp
rg -q 'BuildEngineSceneSnapshot' src/client/world/cl_district.cpp
rg -q 'CL_District_Init' src/client/core/cl_main.c
rg -q 'CL_District_Frame' src/client/cl_gameframe.c
rg -q 'CL_District_AddRefEntitiesToScene' src/client/world/cl_district.cpp
rg -q 'CL_RenderSceneWithDistricts' src/client/cl_ref.c
rg -q 'r_districtDraw' src/client/world/cl_district.cpp
rg -q 'world_district.cpp' CMakeLists.txt
rg -q 'cl_district.cpp' CMakeLists.txt

echo "[test_districts] USDA fixtures..."
test -f tests/data/usd/world_playfield.usda
test -f tests/data/usd/world_proxies/north_proxy.usda
test -f tests/data/usd/world_proxies/south_proxy.usda
test -f tests/data/usd/world_districts/north.usda
rg -q 'District_North' tests/data/usd/world_playfield.usda
rg -q 'purpose = "proxy"' tests/data/usd/world_playfield.usda

echo "[test_districts] demo cfg..."
test -f examples/demo_game/mod/demo_districts.cfg
test -f examples/demo_game/mod/world/playfield.usda

echo "[test_districts] ok"
