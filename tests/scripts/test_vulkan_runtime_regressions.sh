#!/usr/bin/env bash
# Regression checks for high-risk Vulkan runtime wiring.
# Verifies mesh-shader opt-in remains gated and vegetation wind dispatch
# stays after tessellation upload (not at frame start).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${1:-$(cd "$SCRIPT_DIR/../.." && pwd)}"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

assert_file_exists() {
	local path="$1"
	[ -f "$path" ] || fail "missing file: $path"
}

assert_contains() {
	local path="$1"
	local needle="$2"
	local context="$3"
	if ! grep -Fq -- "$needle" "$path"; then
		fail "$context: expected '$needle' in $path"
	fi
}

assert_not_contains() {
	local path="$1"
	local needle="$2"
	local context="$3"
	if grep -Fq -- "$needle" "$path"; then
		fail "$context: unexpected '$needle' in $path"
	fi
}

TR_INIT="$PROJECT_ROOT/src/renderers/vulkan/tr_init.c"
TR_LOCAL="$PROJECT_ROOT/src/renderers/vulkan/tr_local.h"
TR_SHADE="$PROJECT_ROOT/src/renderers/vulkan/tr_shade.c"
VK_H="$PROJECT_ROOT/src/renderers/vulkan/vk.h"
VK_INSTANCE="$PROJECT_ROOT/src/renderers/vulkan/vk_instance.c"
VK_FRAME_SUBMIT="$PROJECT_ROOT/src/renderers/vulkan/vk_frame_submit.c"

assert_file_exists "$TR_INIT"
assert_file_exists "$TR_LOCAL"
assert_file_exists "$TR_SHADE"
assert_file_exists "$VK_H"
assert_file_exists "$VK_INSTANCE"
assert_file_exists "$VK_FRAME_SUBMIT"

# Mesh shader opt-in: cvar remains explicit and default-off.
assert_contains "$TR_INIT" 'r_vk_meshShaderNV = ri.Cvar_Get( "r_vk_meshShaderNV", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );' \
	"mesh shader cvar registration"
assert_contains "$TR_LOCAL" 'r_vk_meshShaderNV;' \
	"mesh shader cvar declaration"
assert_contains "$VK_H" 'qboolean meshShaderNV;' \
	"mesh shader runtime flag declaration"

# Mesh shader device wiring remains opt-in + extension-gated.
assert_contains "$VK_INSTANCE" 'qboolean nvMeshShader = qfalse;' \
	"mesh shader extension probe flag"
assert_contains "$VK_INSTANCE" 'if ( nvMeshShader && r_vk_meshShaderNV && r_vk_meshShaderNV->integer' \
	"mesh shader enable guard"
assert_contains "$VK_INSTANCE" 'device_extension_list[ device_extension_count++ ] = VK_NV_MESH_SHADER_EXTENSION_NAME;' \
	"mesh shader extension enable"
assert_contains "$VK_INSTANCE" 'vk.meshShaderNV = qtrue;' \
	"mesh shader runtime flag enable"
assert_contains "$VK_INSTANCE" 'if ( vk.meshShaderNV ) {' \
	"mesh shader feature chain guard"
assert_contains "$VK_INSTANCE" 'mesh_shader_features_nv.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;' \
	"mesh shader feature struct type"
assert_contains "$VK_INSTANCE" 'mesh_shader_features_nv.meshShader = VK_TRUE;' \
	"mesh shader feature bit enable"
assert_contains "$VK_INSTANCE" 'device_desc.pNext = &mesh_shader_features_nv;' \
	"mesh shader feature chain insertion"

# Vegetation wind compute remains dispatched after staging upload (RB_EndSurface).
assert_contains "$TR_SHADE" '#include "vk_postfx.h"' \
	"vegetation dispatch include"
assert_contains "$TR_SHADE" 'if ( PostFX_VegWind_IsEnabled() && tess.shader && ( tess.shader->surfaceFlags & SURF_VEGETATION ) ) {' \
	"vegetation dispatch guard"
assert_contains "$TR_SHADE" 'vk_vegetation_wind_dispatch();' \
	"vegetation dispatch call"
assert_contains "$TR_SHADE" 'vk_vegetation_clear_staging();' \
	"vegetation staging clear"
assert_not_contains "$VK_FRAME_SUBMIT" 'vk_vegetation_wind_dispatch();' \
	"frame-start dispatch regression"

echo "PASS: test_vulkan_runtime_regressions"
