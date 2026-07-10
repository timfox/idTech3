#!/usr/bin/env bash
# Fog bioaerosol ecology module wiring checks (Evans et al. 2019).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
cd "$ROOT"

echo "[test_fog_biology] checking sources..."
FB="$(idtech3_require_file modules/world/fog_biology.cpp src/world/fog_biology.cpp)"
idtech3_require_file modules/world/fog_biology.h src/world/fog_biology.h >/dev/null
CL_GF="$(idtech3_require_file runtime/client/core/cl_gameframe.c src/client/core/cl_gameframe.c)"
LUA_B="$(idtech3_file runtime/game/g_lua_bindings.c src/game/g_lua_bindings.c)"

rg -q 'fog_biology.cpp' CMakeLists.txt

echo "[test_fog_biology] grep API symbols..."
rg -q 'FogBiology_Init' "$FB"
rg -q 'FogBiology_Frame' "$FB"
rg -q 'FogBiology_GetMarineInfluence' "$FB"
rg -q 'r_fogBiology' "$FB"
rg -q 'FogBiology_Init' "$CL_GF"
rg -q 'FogBiology_Frame' "$CL_GF"
rg -q 'fog_biology_status' "$FB"
rg -q 'fog_biology_paper' "$FB"
rg -q 'gramNegativeFraction' "$FB"
rg -q 'fog_bio_ocean_otu' "$CL_GF"
rg -q 'FogBiology_SetPlayerOrigin' "$CL_GF"
rg -q 'fog_bio_phase' "$CL_GF"
rg -q 'r_fogBiologySyncCoastKm' "$FB"
rg -q 'l_fogBio_getCoastKm' "$LUA_B"
test -f examples/demo_game/mod/demo_fog_biology_openworld.cfg || { echo "missing demo_fog_biology_openworld.cfg"; exit 1; }
rg -q 'l_fogBio_getCommunity' "$LUA_B"
rg -q 'l_fogBio_poll' "$LUA_B"
rg -q 'fog_bio_pathogen_risk' "$CL_GF"
test -f examples/demo_game/mod/demo_fog_biology.cfg || { echo "missing demo_fog_biology.cfg"; exit 1; }
test -f examples/demo_game/mod/demo_fog_biology_namib.cfg || { echo "missing demo_fog_biology_namib.cfg"; exit 1; }

if [[ -x "${ROOT}/build-vk-Release/unit_fog_biology" ]]; then
	echo "[test_fog_biology] running unit_fog_biology..."
	"${ROOT}/build-vk-Release/unit_fog_biology"
elif [[ -x "${ROOT}/build/unit_fog_biology" ]]; then
	"${ROOT}/build/unit_fog_biology"
else
	echo "[test_fog_biology] unit_fog_biology not built; symbol checks only"
fi

echo "[test_fog_biology] ok"
