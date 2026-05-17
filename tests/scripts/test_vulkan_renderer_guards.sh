#!/usr/bin/env bash
# Regression checks for Vulkan renderer guardrails added in recent fixes:
# - Vegetation wind compute dispatch must happen in RB_EndSurface, not frame-begin.
# - VK_NV_mesh_shader enablement must remain explicitly gated by support + cvar.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

TR_SHADE="$PROJECT_ROOT/src/renderers/vulkan/tr_shade.c"
VK_FRAME="$PROJECT_ROOT/src/renderers/vulkan/vk_frame_submit.c"
VK_INSTANCE="$PROJECT_ROOT/src/renderers/vulkan/vk_instance.c"
TR_INIT="$PROJECT_ROOT/src/renderers/vulkan/tr_init.c"
TR_LOCAL="$PROJECT_ROOT/src/renderers/vulkan/tr_local.h"
VK_H="$PROJECT_ROOT/src/renderers/vulkan/vk.h"

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
	local regex="$2"
	local context="$3"
	if ! grep -Eq "$regex" "$path"; then
		fail "$context: expected pattern '$regex' in $path"
	fi
}

assert_not_contains() {
	local path="$1"
	local regex="$2"
	local context="$3"
	if grep -Eq "$regex" "$path"; then
		fail "$context: unexpected pattern '$regex' in $path"
	fi
}

for f in "$TR_SHADE" "$VK_FRAME" "$VK_INSTANCE" "$TR_INIT" "$TR_LOCAL" "$VK_H"; do
	assert_file_exists "$f"
done

# Vegetation wind prepare ordering guard: compute before draw in RB_EndSurface.
assert_not_contains "$VK_FRAME" "\\bvk_vegetation_wind_prepare_draw[[:space:]]*\\(" "vk_begin_frame prepare removal"
assert_contains "$TR_SHADE" "PostFX_VegWind_IsEnabled\\(\\) && tess\\.shader && \\( tess\\.shader->surfaceFlags & SURF_VEGETATION \\)" "RB_EndSurface vegetation gate"
assert_contains "$TR_SHADE" "\\bvk_vegetation_wind_prepare_draw[[:space:]]*\\(\\);" "RB_EndSurface prepare call"

prepare_line="$(awk '/vk_vegetation_wind_prepare_draw[[:space:]]*\(/ {print NR; exit}' "$TR_SHADE")"
iterator_line="$(awk '/optimalStageIteratorFunc[[:space:]]*\(/ {print NR; exit}' "$TR_SHADE")"
guard_line="$(awk '/PostFX_VegWind_IsEnabled\(\).*SURF_VEGETATION/ {print NR; exit}' "$TR_SHADE")"

if [ -z "$prepare_line" ] || [ -z "$iterator_line" ] || [ -z "$guard_line" ]; then
	fail "could not locate vegetation wind guard/prepare/iterator lines"
fi
if [ "$prepare_line" -le "$guard_line" ] || [ "$prepare_line" -ge "$iterator_line" ]; then
	fail "vegetation prepare must run after SURF_VEGETATION guard and before stage iterator"
fi

prepare_count="$(grep -Eho '\bvk_vegetation_wind_prepare_draw[[:space:]]*\(' "$TR_SHADE" "$VK_FRAME" | wc -l | tr -d '[:space:]')"
if [ "$prepare_count" -ne 1 ]; then
	fail "expected exactly one vegetation prepare call across tr_shade/vk_frame_submit, got $prepare_count"
fi

# VK_NV_mesh_shader gating guard:
# Should stay opt-in, default off, and only enabled when support + cvar are true.
assert_contains "$VK_INSTANCE" "vk\\.meshShaderNV = qfalse;" "meshShaderNV reset"
assert_contains "$VK_INSTANCE" "if \\( nvMeshShader && r_vk_meshShaderNV && r_vk_meshShaderNV->integer" "meshShaderNV support+cvar gate"
assert_contains "$VK_INSTANCE" "device_extension_list\\[ device_extension_count\\+\\+ \\] = VK_NV_MESH_SHADER_EXTENSION_NAME;" "mesh shader extension append"
assert_contains "$VK_INSTANCE" "if \\( vk\\.meshShaderNV \\)" "mesh shader feature chain condition"
assert_contains "$VK_INSTANCE" "mesh_shader_features_nv\\.meshShader = VK_TRUE;" "mesh shader feature enable"
assert_contains "$TR_INIT" 'r_vk_meshShaderNV = ri\.Cvar_Get\( "r_vk_meshShaderNV", "0", CVAR_ARCHIVE_ND \| CVAR_LATCH \);' "mesh shader cvar registration"
assert_contains "$TR_LOCAL" 'extern cvar_t[[:space:]]*\*r_vk_meshShaderNV;' "mesh shader cvar extern"
assert_contains "$VK_H" 'qboolean meshShaderNV;' "mesh shader vk state member"

echo "PASS: test_vulkan_renderer_guards"
