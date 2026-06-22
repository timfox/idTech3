#!/usr/bin/env bash
# Wiring test: Metal / DXR roadmap renderer scaffolds + WebGPU manifest.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
source "$ROOT/tests/scripts/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
failures=0

check() {
  if ! grep -q "$2" "$1" 2>/dev/null; then
    echo "FAIL: $3"
    failures=$((failures + 1))
  else
    echo "PASS: $3"
  fi
}

check "$ROOT/CMakeLists.txt" 'tr_platform_renderer_stub.c' 'platform renderer stub source'
check "$ROOT/CMakeLists.txt" 'idtech3_add_platform_renderer_stub' 'CMake platform stub helper'
check "$ROOT/CMakeLists.txt" 'RENDERER_PLATFORM_STUB_METAL' 'Metal stub define wiring'
check "$ROOT/CMakeLists.txt" 'RENDERER_PLATFORM_STUB_DXR' 'DXR stub define wiring'
check "$ROOT/src/renderers/common/tr_platform_renderer_stub.c" 'GetRefAPI' 'platform stub GetRefAPI'
check "$ROOT/src/renderers/common/renderer_backend.h" 'RENDERER_BACKEND_METAL' 'renderer backend header'
check "$IDTECH3_CLIENT/core/cl_ref.c" 'renderer_backend.h' 'client backend header include'
check "$IDTECH3_CLIENT/core/cl_ref.c" 'WEBGPU_ROADMAP.md' 'WebGPU fallback message'
check "$ROOT/docs/DXR_RENDERER.md" 'idtech3_dxr' 'DXR renderer doc'
check "$ROOT/docs/WEBGPU_ROADMAP.md" 'check_webgpu_shader_portability' 'WebGPU roadmap doc'
check "$ROOT/scripts/check_webgpu_shader_portability.sh" 'webgpu_shader_manifest' 'WebGPU manifest script'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All platform renderer stub wiring checks passed."
