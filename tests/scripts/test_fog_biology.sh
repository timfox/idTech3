#!/usr/bin/env bash
# Fog bioaerosol ecology module wiring checks (Evans et al. 2019).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "[test_fog_biology] checking sources..."
for f in \
	src/world/fog_biology.cpp \
	src/world/fog_biology.h \
	src/client/core/cl_gameframe.c
do
	test -f "$f" || { echo "missing $f"; exit 1; }
done

rg -q 'fog_biology.cpp' CMakeLists.txt

echo "[test_fog_biology] grep API symbols..."
rg -q 'FogBiology_Init' src/world/fog_biology.cpp
rg -q 'FogBiology_Frame' src/world/fog_biology.cpp
rg -q 'FogBiology_GetMarineInfluence' src/world/fog_biology.cpp
rg -q 'r_fogBiology' src/world/fog_biology.cpp
rg -q 'FogBiology_Init' src/client/core/cl_gameframe.c
rg -q 'FogBiology_Frame' src/client/core/cl_gameframe.c
rg -q 'fog_biology_status' src/world/fog_biology.cpp
rg -q 'fog_biology_paper' src/world/fog_biology.cpp
rg -q 'gramNegativeFraction' src/world/fog_biology.cpp
rg -q 'fog_bio_ocean_otu' src/client/core/cl_gameframe.c
rg -q 'FogBiology_SetPlayerOrigin' src/client/core/cl_gameframe.c
rg -q 'fog_bio_phase' src/client/core/cl_gameframe.c
rg -q 'r_fogBiologySyncCoastKm' src/world/fog_biology.cpp
rg -q 'l_fogBio_getCoastKm' src/game/g_lua_bindings.c
test -f examples/demo_game/mod/demo_fog_biology_openworld.cfg || { echo "missing demo_fog_biology_openworld.cfg"; exit 1; }
rg -q 'l_fogBio_getCommunity' src/game/g_lua_bindings.c
rg -q 'l_fogBio_poll' src/game/g_lua_bindings.c
rg -q 'fog_bio_pathogen_risk' src/client/core/cl_gameframe.c
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
