#!/usr/bin/env bash
# Fail if first-party trees still use bridge-relative ../qcommon includes.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail=0
for domain in modules/navigation modules/physics modules/audio modules/world modules/botlib \
              runtime/client runtime/game runtime/server \
              engine renderers extensions; do
  if rg -n '#include\s+".*\.\./qcommon/' "$domain" --glob '*.{c,h,cpp,hpp}' --glob '!**/third_party/**' --glob '!**/external/**' 2>/dev/null; then
    echo "FAIL: $domain still has ../qcommon relative includes" >&2
    fail=1
  else
    echo "OK: $domain has no ../qcommon relative includes"
  fi
done

# CMake targets must list IDTECH3_DIR_ENGINE_CORE for these modules.
if ! rg -q 'phys_module.*IDTECH3_DIR_ENGINE_CORE|IDTECH3_DIR_ENGINE_CORE' CMakeLists.txt; then
  echo "FAIL: phys_module / engine-core include wiring missing from CMakeLists.txt" >&2
  fail=1
fi
if ! rg -n 'target_include_directories\(phys_module' -A3 CMakeLists.txt | rg -q 'IDTECH3_DIR_ENGINE_CORE'; then
  echo "FAIL: phys_module PRIVATE includes lack IDTECH3_DIR_ENGINE_CORE" >&2
  fail=1
fi
if ! rg -n 'target_include_directories\(recast_nav PRIVATE' -A8 CMakeLists.txt | rg -q 'IDTECH3_DIR_ENGINE_CORE'; then
  echo "FAIL: recast_nav (enabled) PRIVATE includes lack IDTECH3_DIR_ENGINE_CORE" >&2
  fail=1
fi
if ! rg -n 'target_include_directories\(botlib PRIVATE' -A2 CMakeLists.txt | rg -q 'IDTECH3_DIR_ENGINE_CORE'; then
  echo "FAIL: botlib PRIVATE includes lack IDTECH3_DIR_ENGINE_CORE" >&2
  fail=1
fi
# Audio compiles into client; client must see engine/core for flat qcommon headers.
if ! rg -n 'target_include_directories\(client PRIVATE' -A2 CMakeLists.txt | rg -q 'IDTECH3_DIR_ENGINE_CORE'; then
  echo "FAIL: client PRIVATE includes lack IDTECH3_DIR_ENGINE_CORE (needed for modules/audio flat includes)" >&2
  fail=1
fi
# World unit tests compile modules/world sources; unit include dirs need engine/core.
if ! rg -n 'set\(IDTECH3_UNIT_INCLUDE_DIRS' -A8 CMakeLists.txt | rg -q 'IDTECH3_DIR_ENGINE_CORE'; then
  echo "FAIL: IDTECH3_UNIT_INCLUDE_DIRS lack IDTECH3_DIR_ENGINE_CORE (needed for modules/world flat includes)" >&2
  fail=1
fi
# Vulkan renderer needs engine/core for flat qcommon headers after include rewrite.
if ! rg -n 'target_include_directories\(\$\{RENDERER_PREFIX\}_vulkan' -A3 CMakeLists.txt | rg -q 'IDTECH3_DIR_ENGINE_CORE'; then
  echo "FAIL: vulkan renderer PRIVATE includes lack IDTECH3_DIR_ENGINE_CORE" >&2
  fail=1
fi

if [[ "$fail" -ne 0 ]]; then
  exit 1
fi
echo "test_module_include_canonical.sh: ok"
