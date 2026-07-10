#!/usr/bin/env bash
# Open-world nav sector bake smoke checks.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
cd "$ROOT"

echo "[test_nav_bake] checking sources..."
NAV="$(idtech3_require_file modules/navigation/nav_recast.cpp src/navigation/nav_recast.cpp)"
idtech3_require_file modules/navigation/nav_recast.h src/navigation/nav_recast.h >/dev/null
NAV_BSP="$(idtech3_require_file modules/navigation/nav_bsp_extract.cpp src/navigation/nav_bsp_extract.cpp)"
SV_OW="$(idtech3_require_file runtime/server/sv_openworld.c src/server/sv_openworld.c)"
CL_OW="$(idtech3_require_file runtime/client/world/cl_openworld.cpp src/client/world/cl_openworld.cpp)"
SV_INIT="$(idtech3_require_file runtime/server/sv_init.c src/server/sv_init.c)"

echo "[test_nav_bake] grep API symbols..."
rg -q 'Nav_BakeSectorTile' "$NAV"
rg -q 'Nav_BakeSectorTileToPath' "$NAV"
rg -q 'Nav_BSP_ExtractFromSectorBuffer' "$NAV_BSP"
rg -q 'nav_bake_sector' "$CL_OW"
rg -q 'SV_OpenWorld_Frame' "$SV_OW"
rg -q 'SV_OpenWorld_Init' "$SV_INIT"
rg -q 'tileWidth = sectorTile' "$NAV"

echo "[test_nav_bake] offline bake script..."
test -f scripts/bake_openworld_nav.sh
test -f tests/data/openworld/nav/sector_0_0.nav

echo "[test_nav_bake] ok"
