#!/usr/bin/env bash
# Consistent submodular sector residency wiring smoke checks.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "[test_openworld_residency] checking sources..."
for f in \
	src/world/world_residency.c \
	src/world/world_residency.h \
	src/world/world_open.c \
	src/server/sv_openworld.c \
	src/client/cl_openworld.cpp \
	src/world/world_district.c
do
	test -f "$f" || { echo "missing $f"; exit 1; }
done

echo "[test_openworld_residency] grep API symbols..."
rg -q 'WorldResidency_Init' src/world/world_residency.c
rg -q 'WorldResidency_UpdateView' src/world/world_residency.c
rg -q 'WorldResidency_UpdateServerOrigins' src/world/world_residency.c
rg -q 'WorldResidency_SelectCardinality' src/world/world_residency.c
rg -q 'WorldResidency_SetServerCollisionAllowList' src/world/world_residency.c
rg -q 'WorldResidency_SetDistrictFilter' src/world/world_residency.c
rg -q 'r_openWorldResidency' src/world/world_residency.c
rg -q 'sv_openWorldResidency' src/world/world_residency.c
rg -q 'r_openWorldResidencyMatroid' src/world/world_residency.c
rg -q 'WorldResidency_UpdateView' src/world/world_open.c
rg -q 'WorldResidency_UpdateServerOrigins' src/server/sv_openworld.c
rg -q 'WorldResidency_SetServerCollisionAllowList' src/client/cl_openworld.cpp
rg -q 'WorldResidency_SetDistrictFilter' src/world/world_district.c
rg -q 'world_residency.c' CMakeLists.txt

if [[ -x "${ROOT}/build-vk-Release/unit_world_residency" ]]; then
	echo "[test_openworld_residency] running unit_world_residency..."
	"${ROOT}/build-vk-Release/unit_world_residency"
elif [[ -x "${ROOT}/build/unit_world_residency" ]]; then
	"${ROOT}/build/unit_world_residency"
else
	echo "[test_openworld_residency] unit_world_residency not built (CMake BUILD_TESTS); symbol checks only"
fi

echo "[test_openworld_residency] ok"
