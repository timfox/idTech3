#!/usr/bin/env bash
# Arc Blanc ocean module wiring checks (Algis et al. 2025).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "[test_arc_blanc] checking sources..."
for f in \
	src/world/arc_blanc/arc_blanc.c \
	src/world/arc_blanc/arc_blanc.h \
	src/world/arc_blanc/arc_blanc_fft.c \
	src/renderers/vulkan/extensions/scaffold/vk_arc_blanc.c \
	src/renderers/vulkan/extensions/scaffold/vk_arc_blanc_gpu.c \
	src/renderers/vulkan/shaders/glsl/arc_blanc/arc_blanc_htilde.comp \
	src/client/core/cl_gameframe.c
do
	test -f "$f" || { echo "missing $f"; exit 1; }
done

rg -q 'USE_ARC_BLANC' CMakeLists.txt
rg -q 'arc_blanc/arc_blanc.c' cmake/IdTech3QcommonExtensions.cmake

echo "[test_arc_blanc] grep API symbols..."
rg -q 'ArcBlanc_Init' src/world/arc_blanc/arc_blanc.c
rg -q 'ArcBlanc_Frame' src/world/arc_blanc/arc_blanc.c
rg -q 'r_arcBlanc' src/world/arc_blanc/arc_blanc.c
rg -q 'AB_Spectrum_JONSWAP' src/world/arc_blanc/arc_blanc_spectrum.c
rg -q 'AB_Ocean_UpdateTime' src/world/arc_blanc/arc_blanc_ocean.c
rg -q 'AB_Coupling_ComputeForces' src/world/arc_blanc/arc_blanc_coupling.c
rg -q 'AB_Spectrum_NegKIndex' src/world/arc_blanc/arc_blanc_spectrum.c
rg -q 'AB_Coupling_ApplyWakesToHeightGrid' src/world/arc_blanc/arc_blanc_coupling.c
rg -q 'AB_Spectrum_WaterDensity' src/world/arc_blanc/arc_blanc_spectrum.c
rg -q 'ArcBlanc_Frame' src/client/core/cl_gameframe.c
rg -q 'ArcBlancUploadHeightMap' src/renderers/common/tr_public.h
rg -q 'ArcBlancGpuOceanStep' src/renderers/common/tr_public.h
rg -q 'r_arcBlancGpu' src/world/arc_blanc/arc_blanc.c
rg -q 'arc_blanc_fft_1d_cs' scripts/compile_shaders.sh
rg -q 'arc_blanc_velocity_cs' scripts/compile_shaders.sh
rg -q 'ABGpu_UpdateVelocitySlices' src/renderers/vulkan/extensions/scaffold/vk_arc_blanc_gpu.c
rg -q 'RE_ArcBlancGpuOceanStep' src/renderers/vulkan/extensions/scaffold/vk_arc_blanc_gpu.c
rg -q 'ArcBlancSampleHeight' src/renderers/common/tr_public.h
rg -q 'R_ArcBlanc_AddSurfaces' src/renderers/vulkan/extensions/scaffold/vk_arc_blanc.c
rg -q 'r_arcBlancDraw' src/renderers/vulkan/extensions/scaffold/vk_arc_blanc.c
rg -q 'ArcBlancSampleHeight' src/client/core/cl_ref.c
rg -q 'registerTable\(L, "ArcBlanc"' src/game/g_lua_bindings.c
rg -q 'arc_blanc_status' src/world/arc_blanc/arc_blanc.c
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
