#!/usr/bin/env bash
# Static Vulkan architecture gates (no GPU runtime).
# Protects feature-gating patterns from regressing.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "OK: $*"; }

# 1) RTX must be gated by vkGetPhysicalDeviceFeatures2 + feature bits (not extension names alone)
if ! rg -n "vkGetPhysicalDeviceFeatures2" src/renderers/vulkan/vk_instance.c >/dev/null; then
  fail "vk_instance.c does not reference vkGetPhysicalDeviceFeatures2 for feature gating"
fi
if ! rg -n "bufferDeviceAddress" src/renderers/vulkan/vk_instance.c >/dev/null; then
  fail "vk_instance.c missing bufferDeviceAddress feature check"
fi
if ! rg -n "accelerationStructure" src/renderers/vulkan/vk_instance.c >/dev/null; then
  fail "vk_instance.c missing accelerationStructure feature check"
fi
if ! rg -n "rayTracingPipeline" src/renderers/vulkan/vk_instance.c >/dev/null; then
  fail "vk_instance.c missing rayTracingPipeline feature check"
fi
ok "RTX enabling is feature-gated (Features2 + required bits)"

# 2) When RTX is enabled, device creation must switch to VkPhysicalDeviceFeatures2 chain
if ! rg -n "device_desc\.pEnabledFeatures = NULL" src/renderers/vulkan/vk_instance.c >/dev/null; then
  fail "vk_instance.c does not null device_desc.pEnabledFeatures when using Features2 chain"
fi
ok "Device creation uses Features2 chain for RTX (pEnabledFeatures NULL)"

# 3) Ensure we keep the 'extensions present but required features unsupported' warning
if ! rg -n "extensions present but required features unsupported" src/renderers/vulkan/vk_instance.c >/dev/null; then
  fail "Missing warning for extension-only RT (feature unsupported)"
fi
ok "RT warning for extension-only devices present"

# 4) Mesh-shader extension list capacity guard must remain
if ! rg -n "device_extension_count < ARRAY_LEN\( device_extension_list \)" src/renderers/vulkan/vk_instance.c >/dev/null; then
  fail "Missing extension-list capacity guard for VK_NV_mesh_shader"
fi
ok "Extension-list capacity guard present"

echo "vulkan_state_of_art_check: all checks passed"
