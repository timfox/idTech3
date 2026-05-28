#!/usr/bin/env bash
# Regression guardrails for high-risk Vulkan source paths.
# These checks pin critical control-flow invariants that are hard to execute in headless CI.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

assert_file_exists() {
	local file="$1"
	[ -f "$file" ] || fail "missing file: $file"
}

assert_contains_literal() {
	local file="$1"
	local literal="$2"
	local context="$3"
	if ! grep -Fq "$literal" "$file"; then
		fail "$context: expected literal '$literal' in $file"
	fi
}

assert_not_matches_regex() {
	local file="$1"
	local regex="$2"
	local context="$3"
	if grep -Eq "$regex" "$file"; then
		fail "$context: unexpected pattern '$regex' in $file"
	fi
}

VK_INSTANCE="$PROJECT_ROOT/src/renderers/vulkan/vk_instance.c"
TR_SHADE="$PROJECT_ROOT/src/renderers/vulkan/tr_shade.c"
VK_FRAME_SUBMIT="$PROJECT_ROOT/src/renderers/vulkan/vk_frame_submit.c"
VK_SWAPCHAIN="$PROJECT_ROOT/src/renderers/vulkan/vk_swapchain.c"

assert_file_exists "$VK_INSTANCE"
assert_file_exists "$TR_SHADE"
assert_file_exists "$VK_FRAME_SUBMIT"
assert_file_exists "$VK_SWAPCHAIN"

# Mesh shader extension must stay explicitly gated (support + cvar + extension list capacity).
assert_contains_literal "$VK_INSTANCE" "const char *device_extension_list[40];" "mesh shader extension-list headroom"
assert_contains_literal "$VK_INSTANCE" "vk.meshShaderNV = qfalse;" "mesh shader default disabled"
assert_contains_literal "$VK_INSTANCE" "if ( nvMeshShader && r_vk_meshShaderNV && r_vk_meshShaderNV->integer &&" "mesh shader cvar/support gate"
assert_contains_literal "$VK_INSTANCE" "device_extension_count < ARRAY_LEN( device_extension_list ) ) {" "mesh shader extension-list bounds gate"
assert_contains_literal "$VK_INSTANCE" "device_extension_list[ device_extension_count++ ] = VK_NV_MESH_SHADER_EXTENSION_NAME;" "mesh shader extension append"
assert_contains_literal "$VK_INSTANCE" "vk.meshShaderNV = qtrue;" "mesh shader enable flag"
assert_contains_literal "$VK_INSTANCE" "if ( vk.meshShaderNV ) {" "mesh shader features chained only when enabled"
assert_contains_literal "$VK_INSTANCE" "mesh_shader_features_nv.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;" "mesh shader feature struct type"
assert_contains_literal "$VK_INSTANCE" "mesh_shader_features_nv.taskShader = VK_FALSE;" "mesh shader task stage intentionally disabled"
assert_contains_literal "$VK_INSTANCE" "mesh_shader_features_nv.meshShader = VK_TRUE;" "mesh shader stage explicitly enabled"
assert_contains_literal "$VK_INSTANCE" "device_desc.pNext = &mesh_shader_features_nv;" "mesh shader features added to pNext chain"

# Vegetation wind compute must run before draw for SURF_VEGETATION batches in RB_EndSurface.
assert_contains_literal "$TR_SHADE" "if ( PostFX_VegWind_IsEnabled() && tess.shader && ( tess.shader->surfaceFlags & SURF_VEGETATION ) ) {" "vegetation prepare gate in RB_EndSurface"
assert_contains_literal "$TR_SHADE" "vk_vegetation_wind_prepare_draw();" "vegetation prepare call in RB_EndSurface"

# Frame begin path must not prepare vegetation compute directly (would run before tess upload).
assert_not_matches_regex "$VK_FRAME_SUBMIT" "^[[:space:]]*vk_vegetation_wind_prepare_draw[[:space:]]*\\(" "no early vegetation prepare in vk_begin_frame"

# Swapchain without TRANSFER_SRC must warn (not fatal) so Q3/OA can run on constrained surfaces.
assert_contains_literal "$VK_SWAPCHAIN" "vk.swapchainTransferSrc = qfalse;" "swapchain transfer-src fallback flag"
assert_not_matches_regex "$VK_SWAPCHAIN" "TRANSFER_SRC_BIT.*ERR_FATAL" "no fatal on missing swapchain transfer-src"

# Classic-mod lightmap decode must rebuild world pipelines when toggled at runtime.
assert_contains_literal "$VK_FRAME_SUBMIT" "r_lightmap_srgb_decode && r_lightmap_srgb_decode->modified" "lightmap srgb decode pipeline invalidation"

echo "PASS: test_vulkan_regression_source_guards"
