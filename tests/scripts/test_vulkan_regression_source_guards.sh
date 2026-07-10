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

# shellcheck source=idtech3_test_paths.sh
source "$SCRIPT_DIR/idtech3_test_paths.sh"
idtech3_test_paths_init "$PROJECT_ROOT"

VK_INSTANCE="$(idtech3_file renderers/vulkan/vk_instance.c src/renderers/vulkan/vk_instance.c)"
VK_INIT_DEVICE="$(idtech3_file renderers/vulkan/vk_init_device.c src/renderers/vulkan/vk_init_device.c)"
VK_PROCS="$(idtech3_file renderers/vulkan/vk_procs.h src/renderers/vulkan/vk_procs.h)"
VK_SHUTDOWN="$(idtech3_file renderers/vulkan/vk_shutdown.c src/renderers/vulkan/vk_shutdown.c)"
VK_FRAME_END="$(idtech3_file renderers/vulkan/vk_frame_end.c src/renderers/vulkan/vk_frame_end.c)"
VK_HEADER="$(idtech3_file renderers/vulkan/vk.h src/renderers/vulkan/vk.h)"
TR_INIT="$(idtech3_file renderers/vulkan/tr_init.c src/renderers/vulkan/tr_init.c)"
TR_SHADE="$(idtech3_file renderers/vulkan/tr_shade.c src/renderers/vulkan/tr_shade.c)"
VK_FRAME_SUBMIT="$(idtech3_file renderers/vulkan/vk_frame_submit.c src/renderers/vulkan/vk_frame_submit.c)"
COMPILE_SHADERS="$PROJECT_ROOT/scripts/compile_shaders.sh"

assert_file_exists "$VK_INSTANCE"
assert_file_exists "$VK_INIT_DEVICE"
assert_file_exists "$VK_PROCS"
assert_file_exists "$VK_SHUTDOWN"
assert_file_exists "$VK_FRAME_END"
assert_file_exists "$VK_HEADER"
assert_file_exists "$TR_INIT"
assert_file_exists "$TR_SHADE"
assert_file_exists "$VK_FRAME_SUBMIT"
assert_file_exists "$COMPILE_SHADERS"

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

# Runtime cubemap convolution currently causes device loss on Vulkan. Keep fallback IBL active.
assert_contains_literal "$VK_INIT_DEVICE" "PBR IBL: runtime cubemap convolution is disabled on Vulkan; using fallback IBL." "runtime cubemap fallback warning"
assert_not_matches_regex "$VK_INIT_DEVICE" "vk\\.cubemapActive[[:space:]]*=[[:space:]]*qtrue" "runtime cubemap convolution remains disabled"

# Validation builds need debug-report procs, and pool resets must invalidate cached Forward+ sets.
assert_contains_literal "$VK_PROCS" "#ifdef USE_VK_VALIDATION" "validation proc declarations use feature flag"
assert_contains_literal "$VK_SHUTDOWN" "vk_forward_plus_on_descriptor_pool_destroyed();" "descriptor-pool reset invalidates Forward+ cache"
if ! awk '
	/qvkResetDescriptorPool/ { reset_pool = NR }
	/vk_forward_plus_on_descriptor_pool_destroyed/ && reset_pool && NR > reset_pool { found = 1 }
	END { exit found ? 0 : 1 }
' "$VK_SHUTDOWN"; then
	fail "descriptor-pool reset must invalidate Forward+ cache afterward"
fi

# Swapchain maintenance extension dependency and overlay descriptor ownership must remain explicit.
assert_contains_literal "$VK_INSTANCE" "if ( swapchainMaintenance1 && surfaceMaintenance1 ) {" "swapchain maintenance dependency gate"
assert_contains_literal "$VK_INSTANCE" 'device_extension_list[ device_extension_count++ ] = "VK_KHR_surface_maintenance1";' "surface maintenance extension append"
assert_contains_literal "$VK_HEADER" "VkDescriptorSet overlay_color_descriptor[NUM_COMMAND_BUFFERS];" "immutable overlay descriptor sets"
assert_contains_literal "$VK_FRAME_END" "&vk.overlay_color_descriptor[vk.cmd_index]" "overlay compose binds immutable descriptor"
if ! awk '
	/VkImgui_Shutdown/ { imgui_shutdown = NR }
	/vk_release_resources/ && imgui_shutdown && NR > imgui_shutdown { found = 1 }
	END { exit found ? 0 : 1 }
' "$TR_INIT"; then
	fail "ImGui Vulkan backend must shut down before renderer resources"
fi

# Vegetation wind compute must run before draw for SURF_VEGETATION batches in RB_EndSurface.
assert_contains_literal "$TR_SHADE" "if ( PostFX_VegWind_IsEnabled() && tess.shader && ( tess.shader->surfaceFlags & SURF_VEGETATION ) ) {" "vegetation prepare gate in RB_EndSurface"
assert_contains_literal "$TR_SHADE" "vk_vegetation_wind_prepare_draw();" "vegetation prepare call in RB_EndSurface"

# Frame begin path must not prepare vegetation compute directly (would run before tess upload).
assert_not_matches_regex "$VK_FRAME_SUBMIT" "^[[:space:]]*vk_vegetation_wind_prepare_draw[[:space:]]*\\(" "no early vegetation prepare in vk_begin_frame"

# Command substitution strips the validator version newline; converting it to a space dirties metadata.
assert_contains_literal "$COMPILE_SHADERS" 'VALIDATOR_VERSION="$("$GLSLANG_VALIDATOR" --version 2>&1 | head -n1)"' "shader metadata validator version normalization"
assert_not_matches_regex "$COMPILE_SHADERS" "VALIDATOR_VERSION=.*tr.*\\\\n.* " "shader metadata validator version has no trailing-space conversion"
if ! awk '
	/cmp -s "\$src" "\$dst"/ { compare = NR }
	/mkdir -p "\$backup_base"/ && compare && NR > compare { found = 1 }
	END { exit found ? 0 : 1 }
' "$COMPILE_SHADERS"; then
	fail "shader apply must skip unchanged blobs before creating backups"
fi

echo "PASS: test_vulkan_regression_source_guards"
