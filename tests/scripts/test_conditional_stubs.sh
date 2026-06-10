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
check "$ROOT/src/renderers/vulkan/vk_rtx.c" 'USE_VULKAN_RTX=ON' 'RTX off stub log'
check "$ROOT/src/renderers/vulkan/vk_hybrid1.c" 'USE_VULKAN_RTX=ON' 'Hybrid1 off stub log'
check "$ROOT/src/renderers/vulkan/vk_pathtrace.c" 'USE_VULKAN_RTX=ON' 'PathTrace off stub log'
check "$ROOT/src/renderers/common/tr_font_stub.c" 'BUILD_FREETYPE=ON' 'FreeType off font stub log'
check "$ROOT/src/renderers/common/tr_vector_font_stub.c" 'BUILD_FREETYPE' 'vector font stub guard'
check "$ROOT/src/client/cl_steam.c" '#else /\* !USE_STEAM \*/' 'Steam off stub branch'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All conditional stub wiring checks passed."
