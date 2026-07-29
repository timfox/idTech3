#!/usr/bin/env bash
# Arc Blanc ocean module wiring checks (Algis et al. 2025).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
cd "$ROOT"

echo "[test_arc_blanc] checking sources..."
AB="$(idtech3_require_file modules/world/arc_blanc/arc_blanc.c src/world/arc_blanc/arc_blanc.c)"
idtech3_require_file modules/world/arc_blanc/arc_blanc.h src/world/arc_blanc/arc_blanc.h >/dev/null
idtech3_require_file modules/world/arc_blanc/arc_blanc_fft.c src/world/arc_blanc/arc_blanc_fft.c >/dev/null
VK_AB="$(idtech3_require_file renderers/vulkan/extensions/scaffold/vk_arc_blanc.c src/renderers/vulkan/extensions/scaffold/vk_arc_blanc.c)"
VK_GPU="$(idtech3_require_file renderers/vulkan/extensions/scaffold/vk_arc_blanc_gpu.c src/renderers/vulkan/extensions/scaffold/vk_arc_blanc_gpu.c)"
idtech3_require_file renderers/vulkan/shaders/glsl/arc_blanc/arc_blanc_htilde.comp src/renderers/vulkan/shaders/glsl/arc_blanc/arc_blanc_htilde.comp >/dev/null
CL_GF="$(idtech3_require_file runtime/client/core/cl_gameframe.c src/client/core/cl_gameframe.c)"
SPEC="$(idtech3_file modules/world/arc_blanc/arc_blanc_spectrum.c src/world/arc_blanc/arc_blanc_spectrum.c)"
OCEAN="$(idtech3_file modules/world/arc_blanc/arc_blanc_ocean.c src/world/arc_blanc/arc_blanc_ocean.c)"
COUP="$(idtech3_file modules/world/arc_blanc/arc_blanc_coupling.c src/world/arc_blanc/arc_blanc_coupling.c)"
TR_PUB="$(idtech3_file renderers/common/tr_public.h src/renderers/common/tr_public.h)"
CL_REF="$(idtech3_file runtime/client/core/cl_ref.c src/client/core/cl_ref.c)"
LUA_B="$(idtech3_file runtime/game/scripting/g_lua_bindings.c src/game/g_lua_bindings.c)"
LUA_REG="$(idtech3_file runtime/game/scripting/g_lua_registration.inc src/game/g_lua_registration.inc)"

rg -q 'USE_ARC_BLANC' CMakeLists.txt
rg -q 'arc_blanc/arc_blanc.c' cmake/IdTech3QcommonExtensions.cmake

echo "[test_arc_blanc] grep API symbols..."
rg -q 'ArcBlanc_Init' "$AB"
rg -q 'ArcBlanc_Frame' "$AB"
rg -q 'r_arcBlanc' "$AB"
rg -q 'AB_Spectrum_JONSWAP' "$SPEC"
rg -q 'AB_Ocean_UpdateTime' "$OCEAN"
rg -q 'AB_Coupling_ComputeForces' "$COUP"
rg -q 'AB_Spectrum_NegKIndex' "$SPEC"
rg -q 'AB_Coupling_ApplyWakesToHeightGrid' "$COUP"
rg -q 'AB_Spectrum_WaterDensity' "$SPEC"
rg -q 'ArcBlanc_Frame' "$CL_GF"
rg -q 'ArcBlancUploadHeightMap' "$TR_PUB"
rg -q 'ArcBlancGpuOceanStep' "$TR_PUB"
rg -q 'r_arcBlancGpu' "$AB"
rg -q 'arc_blanc_fft_1d_cs' scripts/compile_shaders.sh
rg -q 'arc_blanc_velocity_cs' scripts/compile_shaders.sh
rg -q 'ABGpu_UpdateVelocitySlices' "$VK_GPU"
rg -q 'RE_ArcBlancGpuOceanStep' "$VK_GPU"
rg -q 'ArcBlancSampleHeight' "$TR_PUB"
rg -q 'R_ArcBlanc_AddSurfaces' "$VK_AB"
rg -q 'r_arcBlancDraw' "$VK_AB"
rg -q 'ArcBlancSampleHeight' "$CL_REF"
rg -q 'registerTable\(L, "ArcBlanc"' "$LUA_B" "$LUA_REG"
rg -q 'arc_blanc_status' "$AB"
test -f examples/demo_game/mod/demo_arc_blanc.cfg || { echo "missing demo_arc_blanc.cfg"; exit 1; }

if [[ -x "${ROOT}/build-vk-Release/unit_arc_blanc" ]]; then
	echo "[test_arc_blanc] running unit_arc_blanc..."
	"${ROOT}/build-vk-Release/unit_arc_blanc"
elif [[ -x "${ROOT}/build/unit_arc_blanc" ]]; then
	"${ROOT}/build/unit_arc_blanc"
else
	echo "[test_arc_blanc] unit_arc_blanc not built; symbol checks only"
fi

echo "[test_arc_blanc] ok"
