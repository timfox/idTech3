#!/usr/bin/env bash
# Regression checks for optional VK_NV_mesh_shader device setup.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

assert_contains() {
	local haystack="$1"
	local needle="$2"
	local context="$3"
	if [[ "$haystack" != *"$needle"* ]]; then
		fail "$context: expected '$needle'"
	fi
}

assert_regex() {
	local haystack="$1"
	local pattern="$2"
	local context="$3"
	if [[ ! "$haystack" =~ $pattern ]]; then
		fail "$context: pattern not found: $pattern"
	fi
}

VK_INSTANCE="$PROJECT_ROOT/src/renderers/vulkan/vk_instance.c"
TR_INIT="$PROJECT_ROOT/src/renderers/vulkan/tr_init.c"
TR_LOCAL="$PROJECT_ROOT/src/renderers/vulkan/tr_local.h"
VK_H="$PROJECT_ROOT/src/renderers/vulkan/vk.h"

for file in "$VK_INSTANCE" "$TR_INIT" "$TR_LOCAL" "$VK_H"; do
	if [ ! -f "$file" ]; then
		fail "missing expected source file: $file"
	fi
done

vk_instance="$(<"$VK_INSTANCE")"
tr_init="$(<"$TR_INIT")"
tr_local="$(<"$TR_LOCAL")"
vk_h="$(<"$VK_H")"

# Cvar must remain explicitly opt-in and latched because this enables an experimental
# device extension at vkCreateDevice time.
assert_contains "$tr_init" 'cvar_t	*r_vk_meshShaderNV;' "mesh shader cvar definition"
assert_contains "$tr_init" 'ri.Cvar_Get( "r_vk_meshShaderNV", "0", CVAR_ARCHIVE_ND | CVAR_LATCH )' "mesh shader cvar default/flags"
assert_contains "$tr_init" 'ri.Cvar_CheckRange( r_vk_meshShaderNV, "0", "1", CV_INTEGER )' "mesh shader cvar range"
assert_contains "$tr_init" 'Default off' "mesh shader cvar description"
assert_contains "$tr_init" 'Requires vid_restart' "mesh shader cvar restart warning"
assert_contains "$tr_local" 'extern cvar_t	*r_vk_meshShaderNV;' "mesh shader cvar declaration"
assert_contains "$vk_h" 'qboolean meshShaderNV;' "mesh shader device state"

# Device extension enumeration must detect VK_NV_mesh_shader but only add it when
# the driver supports it, the cvar opts in, and the fixed extension list has room.
assert_contains "$vk_instance" 'strcmp( ext, VK_NV_MESH_SHADER_EXTENSION_NAME ) == 0' "mesh shader extension detection"
assert_regex "$vk_instance" 'if \( nvMeshShader && r_vk_meshShaderNV && r_vk_meshShaderNV->integer &&[[:space:]]*device_extension_count < ARRAY_LEN\( device_extension_list \) \)' "mesh shader opt-in gate"
assert_contains "$vk_instance" 'device_extension_list[ device_extension_count++ ] = VK_NV_MESH_SHADER_EXTENSION_NAME;' "mesh shader extension append"
assert_contains "$vk_instance" 'vk.meshShaderNV = qtrue;' "mesh shader state enabled"
assert_contains "$vk_instance" 'vk.meshShaderNV = qfalse;' "mesh shader state reset"
assert_contains "$vk_instance" 'VK_NV_mesh_shader enabled (experimental; no mesh draw path yet)' "mesh shader startup log"

# Enabling the extension without appending VkPhysicalDeviceMeshShaderFeaturesNV to
# pNext would make device creation fail on conformant drivers.
assert_contains "$vk_instance" 'if ( vk.meshShaderNV ) {' "mesh shader feature gate"
assert_contains "$vk_instance" 'mesh_shader_features_nv.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;' "mesh shader feature sType"
assert_contains "$vk_instance" 'mesh_shader_features_nv.pNext = (void *)(uintptr_t)device_desc.pNext;' "mesh shader feature pNext chain"
assert_contains "$vk_instance" 'mesh_shader_features_nv.taskShader = VK_FALSE;' "mesh shader taskShader disabled"
assert_contains "$vk_instance" 'mesh_shader_features_nv.meshShader = VK_TRUE;' "mesh shader feature enabled"
assert_contains "$vk_instance" 'device_desc.pNext = &mesh_shader_features_nv;' "mesh shader feature chain head"

echo "PASS: test_vulkan_mesh_shader_opt_in"
