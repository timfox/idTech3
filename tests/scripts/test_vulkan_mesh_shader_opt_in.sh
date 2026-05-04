#!/usr/bin/env bash
# Regression coverage for optional VK_NV_mesh_shader device setup.
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

line_of() {
	local file="$1"
	local needle="$2"
	awk -v needle="$needle" 'index($0, needle) { print NR; found=1; exit } END { if (!found) exit 1 }' "$file"
}

line_count() {
	local file="$1"
	local needle="$2"
	awk -v needle="$needle" 'index($0, needle) { count++ } END { print count + 0 }' "$file"
}

VK_INSTANCE="$PROJECT_ROOT/src/renderers/vulkan/vk_instance.c"
TR_INIT="$PROJECT_ROOT/src/renderers/vulkan/tr_init.c"
TR_LOCAL="$PROJECT_ROOT/src/renderers/vulkan/tr_local.h"
VK_HEADER="$PROJECT_ROOT/src/renderers/vulkan/vk.h"

for required in "$VK_INSTANCE" "$TR_INIT" "$TR_LOCAL" "$VK_HEADER"; do
	[[ -f "$required" ]] || fail "required source file missing: $required"
done

vk_instance="$(<"$VK_INSTANCE")"
tr_init="$(<"$TR_INIT")"
tr_local="$(<"$TR_LOCAL")"
vk_header="$(<"$VK_HEADER")"

assert_contains "$tr_init" 'r_vk_meshShaderNV = ri.Cvar_Get( "r_vk_meshShaderNV", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );' "mesh shader cvar default and flags"
assert_contains "$tr_init" 'ri.Cvar_CheckRange( r_vk_meshShaderNV, "0", "1", CV_INTEGER );' "mesh shader cvar range"
assert_contains "$tr_init" 'Requires vid_restart.' "mesh shader cvar restart documentation"
assert_contains "$tr_init" 'ri.Cvar_SetGroup( r_vk_meshShaderNV, CVG_RENDERER );' "mesh shader cvar renderer group"
assert_contains "$tr_local" 'extern cvar_t	*r_vk_meshShaderNV;' "mesh shader cvar declaration"
assert_contains "$vk_header" 'qboolean meshShaderNV;' "mesh shader device state"

assert_contains "$vk_instance" 'qboolean nvMeshShader = qfalse;' "mesh shader extension detection default"
assert_contains "$vk_instance" 'strcmp( ext, VK_NV_MESH_SHADER_EXTENSION_NAME ) == 0' "mesh shader extension detection"
assert_contains "$vk_instance" 'vk.meshShaderNV = qfalse;' "mesh shader state reset"
assert_contains "$vk_instance" 'if ( nvMeshShader && r_vk_meshShaderNV && r_vk_meshShaderNV->integer &&' "mesh shader opt-in gate"
assert_contains "$vk_instance" 'device_extension_count < ARRAY_LEN( device_extension_list )' "mesh shader extension capacity guard"
assert_contains "$vk_instance" 'device_extension_list[ device_extension_count++ ] = VK_NV_MESH_SHADER_EXTENSION_NAME;' "mesh shader extension append"
assert_contains "$vk_instance" 'vk.meshShaderNV = qtrue;' "mesh shader enabled state"
assert_contains "$vk_instance" '[VK] VK_NV_mesh_shader enabled (experimental; no mesh draw path yet)' "mesh shader startup log"

assert_contains "$vk_instance" '/* Chain last so _DEBUG / host_query pNext lists stay valid. */' "mesh shader pNext chain placement comment"
assert_contains "$vk_instance" 'if ( vk.meshShaderNV ) {' "mesh shader feature chain gate"
assert_contains "$vk_instance" 'mesh_shader_features_nv.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;' "mesh shader feature sType"
assert_contains "$vk_instance" 'mesh_shader_features_nv.pNext = (void *)(uintptr_t)device_desc.pNext;' "mesh shader preserves previous pNext"
assert_contains "$vk_instance" 'mesh_shader_features_nv.taskShader = VK_FALSE;' "mesh shader keeps task shaders disabled"
assert_contains "$vk_instance" 'mesh_shader_features_nv.meshShader = VK_TRUE;' "mesh shader enables only mesh shader feature"
assert_contains "$vk_instance" 'device_desc.pNext = &mesh_shader_features_nv;' "mesh shader pNext head"

extension_append_count="$(line_count "$VK_INSTANCE" 'VK_NV_MESH_SHADER_EXTENSION_NAME')"
[[ "$extension_append_count" -eq 2 ]] || fail "expected one detection and one append of VK_NV_mesh_shader; saw $extension_append_count occurrences"

gate_line="$(line_of "$VK_INSTANCE" 'if ( nvMeshShader && r_vk_meshShaderNV && r_vk_meshShaderNV->integer &&')"
append_line="$(line_of "$VK_INSTANCE" 'device_extension_list[ device_extension_count++ ] = VK_NV_MESH_SHADER_EXTENSION_NAME;')"
state_line="$(line_of "$VK_INSTANCE" 'vk.meshShaderNV = qtrue;')"
feature_line="$(line_of "$VK_INSTANCE" 'if ( vk.meshShaderNV ) {')"
create_line="$(line_of "$VK_INSTANCE" 'qvkCreateDevice( physical_device, &device_desc, NULL, &vk.device )')"

(( gate_line < append_line )) || fail "mesh shader extension append must remain inside the opt-in gate"
(( append_line < state_line )) || fail "mesh shader state should be set after appending the extension"
(( state_line < feature_line )) || fail "mesh shader feature chaining must happen after state is set"
(( feature_line < create_line )) || fail "mesh shader features must be chained before vkCreateDevice"

echo "PASS: test_vulkan_mesh_shader_opt_in"
