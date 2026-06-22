#!/usr/bin/env bash
# Wiring test: conditional #else stubs (RTX, FreeType, experimental renderers, Steam).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

check() {
  if ! grep -q "$2" "$1" 2>/dev/null; then
    echo "FAIL: $3"
    failures=$((failures + 1))
  else
    echo "PASS: $3"
  fi
}

check "$ROOT/CMakeLists.txt" 'USE_EXPERIMENTAL_RENDERERS' 'CMake USE_EXPERIMENTAL_RENDERERS option'
check "$ROOT/CMakeLists.txt" 'vk_experimental_renderer_stubs.c' 'experimental stub source wiring'
check "$ROOT/CMakeLists.txt" 'tr_vector_font_stub.c' 'vector font stub wiring'
check "$ROOT/src/renderers/vulkan/vk_experimental_renderer_stubs.c" 'USE_EXPERIMENTAL_RENDERERS' 'experimental stub guard'
check "$ROOT/src/renderers/vulkan/vk_experimental_renderer_stubs.c" 'R_FSA_Init' 'FSA experimental stub'
check "$ROOT/src/renderers/vulkan/vk_experimental_renderer_stubs.c" 'R_SQZ_Enabled' 'SqueezeMe experimental stub'
check "$ROOT/runtime/client/core/cl_ref.c" 'CL_SanitizeRendererName' 'renderer cvar whitespace trim'
check "$ROOT/src/renderers/vulkan/extensions/rtx/vk_rtx.c" 'USE_VULKAN_RTX=ON' 'RTX off stub log'
check "$ROOT/src/renderers/vulkan/extensions/rtx/vk_hybrid1.c" 'USE_VULKAN_RTX=ON' 'Hybrid1 off stub log'
check "$ROOT/src/renderers/vulkan/extensions/rtx/vk_pathtrace.c" 'USE_VULKAN_RTX=ON' 'PathTrace off stub log'
check "$ROOT/src/renderers/common/tr_font_stub.c" 'BUILD_FREETYPE=ON' 'FreeType off font stub log'
check "$ROOT/src/renderers/common/tr_vector_font_stub.c" 'BUILD_FREETYPE' 'vector font stub guard'
check "$ROOT/src/client/platform/cl_steam.c" '#else /\* !USE_STEAM \*/' 'Steam off stub branch'
check "$ROOT/renderers/vulkan/shaders/glsl/gamma.frag" 'hdrResolveActive' 'gamma pass splits HDR resolve from grading'
check "$ROOT/renderers/vulkan/shaders/glsl/gamma.frag" 'postGradeActive' 'gamma pass grading tier flag'
check "$ROOT/renderers/vulkan/vk_post_process_pipeline.c" 'VK_FORMAT_A8B8G8R8_SRGB_PACK32' 'sRGB present format detection'
check "$ROOT/renderers/vulkan/tr_image.c" 'sRGBtoRGB(currentColor\[2\])' 'spec map sRGB ratio uses blue channel'
check "$ROOT/runtime/client/core/cl_cgame.c" 'legacySnapshot_t' 'legacy cgame snapshot struct'
check "$ROOT/runtime/client/core/cl_cgame.c" 'CL_GetLegacySnapshot' 'legacy cgame snapshot trap path'
check "$ROOT/runtime/client/core/cl_cgame.c" 'sizeof( trace_t )' 'cgame CM trace trap bounds'
check "$ROOT/runtime/client/core/cl_cgame.c" 'cl_physicsEnabled 0 for cgame.qvm compatibility' 'retail cgame physics guard'
check "$ROOT/engine/core/cm_stream.c" 'CM_Stream_SectorOverlayPermitted' 'classic map sector overlay gate'
check "$ROOT/engine/core/cm_load.c" 'CM_Stream_Clear' 'map clear drops sector merge overlays'
check "$ROOT/runtime/server/sv_openworld.c" 'SV_OpenWorld_OnMapLoad' 'server classic map open-world guard'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All conditional stub wiring checks passed."
