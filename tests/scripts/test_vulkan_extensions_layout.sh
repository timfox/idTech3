#!/usr/bin/env bash
# Wiring test: Vulkan extensions/ physical layout + CMake manifest.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fail() { echo "FAIL: $*" >&2; exit 1; }

VE="${ROOT}/cmake/renderers/VulkanExtensionSources.cmake"
[ -f "$VE" ] || fail "missing VulkanExtensionSources.cmake"

for d in neural splats rtx scaffold; do
  [ -d "${ROOT}/src/renderers/vulkan/extensions/$d" ] || fail "missing extensions/$d"
done

rg -q 'neural/vk_niv.c' "$VE" || fail "neural manifest path"
rg -q 'rtx/vk_rtx.c' "$VE" || fail "rtx core manifest path"
rg -q 'idtech3_vulkan_extension_include_dirs' "$VE" || fail "extension include macro missing"

# RTX core always appended (self-stubs); experimental pack gated
rg -q 'VK_RTX_CORE_SRCS' "$VE" || fail "VK_RTX_CORE_SRCS missing"

# Root must not retain moved extension TU
for f in vk_niv.c vk_hybrid1.c vk_mgs.c vk_vuda.c; do
  [ ! -f "${ROOT}/src/renderers/vulkan/$f" ] || fail "stale root file $f (should be under extensions/)"
done

[ -f "${ROOT}/src/renderers/vulkan/vk_experimental_renderer_stubs.c" ] || fail "stubs must stay at vulkan root"

echo "test_vulkan_extensions_layout: passed"
