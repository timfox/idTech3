#!/usr/bin/env bash
# Consistent submodular sector residency wiring smoke checks.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
cd "$ROOT"

echo "[test_openworld_residency] checking sources..."
RESIDENCY="$(idtech3_require_file modules/world/world_residency.cpp src/world/world_residency.cpp)"
idtech3_require_file modules/world/world_residency.h src/world/world_residency.h >/dev/null
WORLD_OPEN="$(idtech3_require_file modules/world/world_open.cpp src/world/world_open.cpp)"
SV_OW="$(idtech3_require_file runtime/server/world/sv_openworld.c src/server/sv_openworld.c)"
CL_OW="$(idtech3_require_file runtime/client/world/cl_openworld.cpp src/client/world/cl_openworld.cpp)"
DISTRICT="$(idtech3_require_file modules/world/world_district.cpp src/world/world_district.cpp)"

echo "[test_openworld_residency] grep API symbols..."
rg -q 'WorldResidency_Init' "$RESIDENCY"
rg -q 'WorldResidency_UpdateView' "$RESIDENCY"
rg -q 'WorldResidency_UpdateServerOrigins' "$RESIDENCY"
rg -q 'WorldResidency_SelectCardinality' "$RESIDENCY"
rg -q 'WorldResidency_SetServerCollisionAllowList' "$RESIDENCY"
rg -q 'WorldResidency_SetDistrictFilter' "$RESIDENCY"
rg -q 'r_openWorldResidency' "$RESIDENCY"
rg -q 'sv_openWorldResidency' "$RESIDENCY"
rg -q 'r_openWorldResidencyMatroid' "$RESIDENCY"
rg -q 'WorldResidency_UpdateView' "$WORLD_OPEN"
rg -q 'WorldResidency_UpdateServerOrigins' "$SV_OW"
rg -q 'WorldResidency_SetServerCollisionAllowList' "$CL_OW"
rg -q 'WorldResidency_SetDistrictFilter' "$DISTRICT"
rg -q 'world_residency.cpp' CMakeLists.txt
rg -q 'SectorGraph_IsReachable' "$RESIDENCY"
rg -q 'sector_graph.cpp' CMakeLists.txt
rg -q 'WR_LayerUsesServerAllow' "$RESIDENCY"
rg -q 'world_residency_status' "$RESIDENCY"
rg -q 'WorldResidency_Status' "$RESIDENCY"
test -f "$ROOT/examples/demo_game/mod/demo_world_residency.cfg"
rg -q 'r_openWorldResidency 1' "$ROOT/examples/demo_game/mod/demo_world_residency.cfg"
rg -q 'modules/world/world_residency' "$ROOT/docs/WORLD_RESIDENCY.md"

if [[ -x "${ROOT}/build-vk-Release/unit_world_residency" ]]; then
	echo "[test_openworld_residency] running unit_world_residency..."
	"${ROOT}/build-vk-Release/unit_world_residency"
elif [[ -x "${ROOT}/build/unit_world_residency" ]]; then
	"${ROOT}/build/unit_world_residency"
else
	echo "[test_openworld_residency] unit_world_residency not built (CMake BUILD_TESTS); symbol checks only"
fi

echo "[test_openworld_residency] ok"
