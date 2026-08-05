#!/usr/bin/env bash
# World districts + proxy mesh smoke checks (FreeUSD manifest path).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
cd "$ROOT"

echo "[test_districts] checking sources..."
WD="$(idtech3_require_file modules/world/world_district.cpp src/world/world_district.cpp)"
idtech3_require_file modules/world/world_district.h src/world/world_district.h >/dev/null
CL_D="$(idtech3_require_file runtime/client/world/cl_district.cpp src/client/world/cl_district.cpp)"
idtech3_require_file runtime/client/world/cl_district.h src/client/world/cl_district.h >/dev/null
CL_MAIN="$(idtech3_require_file runtime/client/core/cl_main.c src/client/core/cl_main.c)"
CL_GF="$(idtech3_require_file runtime/client/core/cl_gameframe.c src/client/core/cl_gameframe.c)"
CL_REF="$(idtech3_require_file runtime/client/core/cl_ref.c src/client/core/cl_ref.c)"

echo "[test_districts] grep API symbols..."
rg -q 'WorldDistrict_Init' "$WD"
rg -q 'WorldDistrict_UpdateView' "$WD"
rg -q 'WorldDistrict_Import' "$WD"
rg -q 'CL_District_Init' "$CL_D"
rg -q 'CL_District_Frame' "$CL_D"
rg -q 'BuildEngineSceneSnapshot' "$CL_D"
rg -q 'CL_District_Init' "$CL_MAIN"
rg -q 'CL_District_Frame' "$CL_GF"
rg -q 'CL_District_AddRefEntitiesToScene' "$CL_D"
rg -q 'CL_RenderSceneWithDistricts' "$CL_REF"
rg -q 'r_districtDraw' "$CL_D"
rg -q 'r_districtAsyncLoad' "$CL_D"
rg -q 'Jobs_SubmitWork' "$CL_D"
rg -q 'Defer_Add' "$CL_D"
rg -q 'r_districtAutoFull' "$WD"
rg -q 'full load deferred/failed' "$WD"
rg -q 'RDF_NOWORLDMODEL' "$CL_D"
rg -q 'd->proxyModel \|\| d->fullModel' "$CL_D"
ZONE="$ROOT/modules/world/world_zone.cpp"
ZONE_H="$ROOT/modules/world/world_zone.h"
test -f "$ZONE"
test -f "$ZONE_H"
rg -q 'WorldZone_UpdateView' "$ZONE"
rg -q 'WorldZone_Import' "$WD"
rg -q 'WorldDistrict_ZoneLoad' "$WD"
rg -q 'residencyMask' "$ZONE_H"
rg -q 'zoneLoadRadius' "$CL_D"
rg -q 'r_worldZoneBudget' "$ZONE"
rg -q 'world_zone.cpp' cmake/IdTech3QcommonExtensions.cmake
rg -q 'world_district.cpp' cmake/IdTech3QcommonExtensions.cmake
rg -q 'cl_district.cpp' cmake/client/ClientExtensionSources.cmake

echo "[test_districts] USDA fixtures..."
test -f tests/data/usd/world_playfield.usda
test -f tests/data/usd/world_proxies/north_proxy.usda
test -f tests/data/usd/world_proxies/south_proxy.usda
test -f tests/data/usd/world_districts/north.usda
rg -q 'District_North' tests/data/usd/world_playfield.usda
rg -q 'purpose = "proxy"' tests/data/usd/world_playfield.usda
rg -q 'residencyMask = 7' tests/data/usd/world_playfield.usda

echo "[test_districts] demo cfg..."
test -f examples/demo_game/mod/demo_districts.cfg
test -f examples/demo_game/mod/world/playfield.usda

echo "[test_districts] ok"
