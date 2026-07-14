#!/usr/bin/env bash
# Wiring test: conditional #else stubs (RTX, FreeType, experimental renderers, Steam).
# Chocolate paths (MGS/WSP/SqueezeMe, Hybrid1/Raygun, Arc Blanc) are always linked —
# see cmake/renderers/VulkanExtensionSources.cmake.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
failures=0

VK_STUBS="$(idtech3_file renderers/vulkan/vk_experimental_renderer_stubs.c src/renderers/vulkan/vk_experimental_renderer_stubs.c)"
VK_EXT_CMAKE="$ROOT/cmake/renderers/VulkanExtensionSources.cmake"
VK_RTX="$(idtech3_file renderers/vulkan/extensions/rtx/vk_rtx.c src/renderers/vulkan/extensions/rtx/vk_rtx.c)"
VK_HYBRID1="$(idtech3_file renderers/vulkan/extensions/rtx/vk_hybrid1.c src/renderers/vulkan/extensions/rtx/vk_hybrid1.c)"
VK_PATHTRACE="$(idtech3_file renderers/vulkan/extensions/rtx/vk_pathtrace.c src/renderers/vulkan/extensions/rtx/vk_pathtrace.c)"
VK_ARC="$(idtech3_file renderers/vulkan/extensions/scaffold/vk_arc_blanc.c src/renderers/vulkan/extensions/scaffold/vk_arc_blanc.c)"
VK_MGS="$(idtech3_file renderers/vulkan/extensions/splats/vk_mgs.c src/renderers/vulkan/extensions/splats/vk_mgs.c)"
VK_SQZ="$(idtech3_file renderers/vulkan/extensions/splats/vk_squeezeme.c src/renderers/vulkan/extensions/splats/vk_squeezeme.c)"
TR_FONT_STUB="$(idtech3_file renderers/common/tr_font_stub.c src/renderers/common/tr_font_stub.c)"
TR_VECTOR_FONT_STUB="$(idtech3_file renderers/common/tr_vector_font_stub.c src/renderers/common/tr_vector_font_stub.c)"
CL_STEAM="$(idtech3_file runtime/client/platform/cl_steam.c src/client/platform/cl_steam.c)"
CL_REF="$(idtech3_file runtime/client/core/cl_ref.c src/client/core/cl_ref.c)"
GAMMA_FRAG="$(idtech3_file renderers/vulkan/shaders/glsl/gamma.frag src/renderers/vulkan/shaders/glsl/gamma.frag)"
VK_POST="$(idtech3_file renderers/vulkan/vk_post_process_pipeline.c src/renderers/vulkan/vk_post_process_pipeline.c)"
TR_IMAGE="$(idtech3_file renderers/vulkan/tr_image.c src/renderers/vulkan/tr_image.c)"
CL_CGAME="$(idtech3_file runtime/client/core/cl_cgame.c src/client/core/cl_cgame.c)"
CM_STREAM="$(idtech3_file engine/core/cm_stream.c src/qcommon/cm_stream.c)"
CM_LOAD="$(idtech3_file engine/core/cm_load.c src/qcommon/cm_load.c)"
SV_OPENWORLD="$(idtech3_file runtime/server/sv_openworld.c src/server/sv_openworld.c)"

check() {
  if ! grep -q "$2" "$1" 2>/dev/null; then
    echo "FAIL: $3"
    failures=$((failures + 1))
  else
    echo "PASS: $3"
  fi
}

check_absent() {
  if grep -q "$2" "$1" 2>/dev/null; then
    echo "FAIL: $3 (should be absent)"
    failures=$((failures + 1))
  else
    echo "PASS: $3"
  fi
}

check "$ROOT/CMakeLists.txt" 'USE_EXPERIMENTAL_RENDERERS' 'CMake USE_EXPERIMENTAL_RENDERERS option'
check "$ROOT/CMakeLists.txt" 'vk_experimental_renderer_stubs.c' 'experimental stub source wiring'
check "$ROOT/CMakeLists.txt" 'tr_vector_font_stub.c' 'vector font stub wiring'
check "$VK_EXT_CMAKE" 'VK_CHOCOLATE_SPLAT_SRCS' 'chocolate splat CMake list'
check "$VK_EXT_CMAKE" 'VK_CHOCOLATE_RTX_SRCS' 'chocolate RTX CMake list'
check "$VK_EXT_CMAKE" 'VK_ARC_BLANC_VK_SRCS' 'Arc Blanc chocolate CMake list'
check "$VK_STUBS" 'USE_EXPERIMENTAL_RENDERERS' 'experimental stub guard'
check "$VK_STUBS" 'R_FSA_Init' 'FSA experimental stub'
check_absent "$VK_STUBS" 'R_SQZ_Enabled' 'SqueezeMe not in experimental stubs (chocolate)'
check_absent "$VK_STUBS" 'R_MGS_Init' 'MGS not in experimental stubs (chocolate)'
check_absent "$VK_STUBS" 'vk_hybrid1_init' 'Hybrid1 not in experimental stubs (chocolate)'
check_absent "$VK_STUBS" 'R_ArcBlanc_Init' 'Arc Blanc not in experimental stubs (chocolate)'
check "$VK_MGS" 'chocolate path ready' 'MGS chocolate startup log'
check "$VK_SQZ" 'R_SQZ_Init' 'SqueezeMe chocolate source'
check "$VK_ARC" 'USE_ARC_BLANC=ON' 'Arc Blanc off stub log'
check "$CL_REF" 'CL_SanitizeRendererName' 'renderer cvar whitespace trim'
check "$VK_RTX" 'USE_VULKAN_RTX=ON' 'RTX off stub log'
check "$VK_HYBRID1" 'USE_VULKAN_RTX=ON' 'Hybrid1 off stub log'
check "$VK_PATHTRACE" 'USE_VULKAN_RTX=ON' 'PathTrace off stub log'
check "$TR_FONT_STUB" 'BUILD_FREETYPE=ON' 'FreeType off font stub log'
check "$TR_VECTOR_FONT_STUB" 'BUILD_FREETYPE' 'vector font stub guard'
check "$CL_STEAM" '#else /\* !USE_STEAM \*/' 'Steam off stub branch'
check "$GAMMA_FRAG" 'hdrResolveActive' 'gamma pass splits HDR resolve from grading'
check "$GAMMA_FRAG" 'postGradeActive' 'gamma pass grading tier flag'
check "$GAMMA_FRAG" 'applyHueShift' 'gamma pass exposes hue grading'
check "$GAMMA_FRAG" 'postHueDegrees' 'gamma pass reads hue grading param'
check "$VK_POST" 'VK_FORMAT_A8B8G8R8_SRGB_PACK32' 'sRGB present format detection'
check "$TR_IMAGE" 'sRGBtoRGB(currentColor\[2\])' 'spec map sRGB ratio uses blue channel'
check "$CL_CGAME" 'legacySnapshot_t' 'legacy cgame snapshot struct'
check "$CL_CGAME" 'CL_GetLegacySnapshot' 'legacy cgame snapshot trap path'
check "$CL_CGAME" 'sizeof( trace_t )' 'cgame CM trace trap bounds'
check "$CL_CGAME" 'cl_physicsEnabled 0 for cgame.qvm compatibility' 'retail cgame physics guard'
check "$CM_STREAM" 'CM_Stream_SectorOverlayPermitted' 'classic map sector overlay gate'
check "$CM_LOAD" 'CM_Stream_Clear' 'map clear drops sector merge overlays'
check "$SV_OPENWORLD" 'SV_OpenWorld_OnMapLoad' 'server classic map open-world guard'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All conditional stub wiring checks passed."
