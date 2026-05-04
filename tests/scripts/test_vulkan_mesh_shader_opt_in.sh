#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <project-root>" >&2
  exit 2
fi

PROJECT_ROOT="$1"
VK_INSTANCE="$PROJECT_ROOT/src/renderers/vulkan/vk_instance.c"
VK_INIT="$PROJECT_ROOT/src/renderers/vulkan/tr_init.c"
VK_H="$PROJECT_ROOT/src/renderers/vulkan/vk.h"
VK_LOCAL="$PROJECT_ROOT/src/renderers/vulkan/tr_local.h"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

require_file() {
  [ -f "$1" ] || fail "missing file: $1"
}

require_pattern() {
  local file="$1"
  local pattern="$2"
  local message="$3"

  perl -0ne "exit(!(/$pattern/s))" "$file" || fail "$message"
}

require_file "$VK_INSTANCE"
require_file "$VK_INIT"
require_file "$VK_H"
require_file "$VK_LOCAL"

require_pattern "$VK_INIT" \
  'r_vk_meshShaderNV\s*=\s*ri\.Cvar_Get\s*\(\s*"r_vk_meshShaderNV"\s*,\s*"0"\s*,\s*CVAR_ARCHIVE_ND\s*\|\s*CVAR_LATCH\s*\)' \
  "r_vk_meshShaderNV must default off and remain latched"

require_pattern "$VK_INIT" \
  'ri\.Cvar_CheckRange\s*\(\s*r_vk_meshShaderNV\s*,\s*"0"\s*,\s*"1"\s*,\s*CV_INTEGER\s*\)' \
  "r_vk_meshShaderNV must stay constrained to a boolean integer"

require_pattern "$VK_LOCAL" \
  'extern\s+cvar_t\s*\*\s*r_vk_meshShaderNV\s*;' \
  "r_vk_meshShaderNV must remain visible to Vulkan device creation"

require_pattern "$VK_H" \
  'qboolean\s+meshShaderNV\s*;' \
  "vk_t must keep meshShaderNV state for feature-chain gating"

require_pattern "$VK_INSTANCE" \
  'qboolean\s+nvMeshShader\s*=\s*qfalse\s*;' \
  "VK_NV_mesh_shader availability must be tracked from enumerated extensions"

require_pattern "$VK_INSTANCE" \
  'strcmp\s*\(\s*ext\s*,\s*VK_NV_MESH_SHADER_EXTENSION_NAME\s*\)\s*==\s*0\s*\)\s*\{[^}]*nvMeshShader\s*=\s*qtrue\s*;' \
  "VK_NV_mesh_shader must only be marked available when reported by the driver"

require_pattern "$VK_INSTANCE" \
  'vk\.meshShaderNV\s*=\s*qfalse\s*;.*if\s*\(\s*nvMeshShader\s*&&\s*r_vk_meshShaderNV\s*&&\s*r_vk_meshShaderNV->integer\s*&&\s*device_extension_count\s*<\s*ARRAY_LEN\s*\(\s*device_extension_list\s*\)\s*\)\s*\{[^}]*device_extension_list\s*\[\s*device_extension_count\+\+\s*\]\s*=\s*VK_NV_MESH_SHADER_EXTENSION_NAME\s*;[^}]*vk\.meshShaderNV\s*=\s*qtrue\s*;' \
  "VK_NV_mesh_shader must require driver support, explicit cvar opt-in, and list capacity before enabling"

require_pattern "$VK_INSTANCE" \
  'if\s*\(\s*vk\.meshShaderNV\s*\)\s*\{[^}]*Com_Memset\s*\(\s*&mesh_shader_features_nv\s*,\s*0\s*,\s*sizeof\s*\(\s*mesh_shader_features_nv\s*\)\s*\)\s*;[^}]*mesh_shader_features_nv\.sType\s*=\s*VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV\s*;[^}]*mesh_shader_features_nv\.pNext\s*=\s*\(void\s*\*\)\s*\(uintptr_t\)device_desc\.pNext\s*;[^}]*mesh_shader_features_nv\.taskShader\s*=\s*VK_FALSE\s*;[^}]*mesh_shader_features_nv\.meshShader\s*=\s*VK_TRUE\s*;[^}]*device_desc\.pNext\s*=\s*&mesh_shader_features_nv\s*;' \
  "VK_NV_mesh_shader must append meshShader feature enablement to the VkDeviceCreateInfo pNext chain"

echo "PASS: Vulkan mesh shader extension remains explicit opt-in with feature-chain wiring"
