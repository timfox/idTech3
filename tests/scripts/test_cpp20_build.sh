#!/usr/bin/env bash
# Verify mixed C/C++20 build options and converted leaves are wired.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
check() {
  if ! grep -qE "$2" "$1" 2>/dev/null; then
    echo "FAIL: $3"; failures=$((failures+1))
  else
    echo "PASS: $3"
  fi
}
check "$ROOT/cmake/IdTech3Cpp20.cmake" 'option\(USE_CPP20' 'USE_CPP20 option'
check "$ROOT/cmake/IdTech3Cpp20.cmake" 'CPP20_EXCEPTIONS' 'CPP20_EXCEPTIONS option'
check "$ROOT/cmake/IdTech3Cpp20.cmake" 'CPP20_RTTI' 'CPP20_RTTI option'
check "$ROOT/cmake/IdTech3Cpp20.cmake" 'CPP20_STRICT' 'CPP20_STRICT option'
check "$ROOT/cmake/IdTech3Cpp20.cmake" 'fno-exceptions|/EHs' 'exceptions disabled path'
check "$ROOT/CMakeLists.txt" 'IdTech3Cpp20.cmake' 'CMake includes IdTech3Cpp20'
check "$ROOT/engine/core/md4.cpp" 'extern "C"' 'md4.cpp has C linkage wrap'
check "$ROOT/engine/core/md5.cpp" 'extern "C"' 'md5.cpp has C linkage wrap'
check "$ROOT/engine/core/huffman_static.cpp" 'extern "C"' 'huffman_static.cpp wrapped'
check "$ROOT/engine/core/q_utf8.cpp" 'extern "C"' 'q_utf8.cpp wrapped'
check "$ROOT/renderers/vulkan/vk_cluster_math.cpp" 'extern "C"' 'vk_cluster_math.cpp wrapped'
check "$ROOT/engine/core/cpp20_compat.h" 'IDTECH3_EXTERN_C_BEGIN' 'cpp20_compat.h present'
check "$ROOT/engine/core/cpp20_abi_guards.cpp" 'sizeof\( trace_t \)' 'ABI guards for trace_t'
check "$ROOT/engine/core/cpp20_status.cpp" 'cpp20_status' 'cpp20_status command TU'
check "$ROOT/docs/CPP20_MIGRATION.md" 'USE_CPP20' 'migration doc'
[[ ! -f "$ROOT/engine/core/md4.c" ]] || { echo "FAIL: md4.c still present"; failures=$((failures+1)); }
[[ ! -f "$ROOT/engine/core/md5.c" ]] || { echo "FAIL: md5.c still present"; failures=$((failures+1)); }
if [[ $failures -ne 0 ]]; then echo "$failures failed"; exit 1; fi
echo "All cpp20 build wiring checks passed."
