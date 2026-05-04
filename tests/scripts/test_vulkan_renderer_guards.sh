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

# Vegetation wind dispatch ordering guard:
# It must be dispatched only after vegetation staging upload in RB_EndSurface.
assert_not_contains "$VK_FRAME" "\\bvk_vegetation_wind_dispatch[[:space:]]*\\(" "vk_begin_frame dispatch removal"
assert_contains "$TR_SHADE" "PostFX_VegWind_IsEnabled\\(\\) && tess\\.shader && \\( tess\\.shader->surfaceFlags & SURF_VEGETATION \\)" "RB_EndSurface vegetation gate"
assert_contains "$TR_SHADE" "\\bvk_vegetation_wind_dispatch[[:space:]]*\\(\\);" "RB_EndSurface dispatch call"
assert_contains "$TR_SHADE" "\\bvk_vegetation_clear_staging[[:space:]]*\\(\\);" "RB_EndSurface staging clear call"

dispatch_line="$(awk '/vk_vegetation_wind_dispatch[[:space:]]*\(/ {print NR; exit}' "$TR_SHADE")"
clear_line="$(awk '/vk_vegetation_clear_staging[[:space:]]*\(/ {print NR; exit}' "$TR_SHADE")"
guard_line="$(awk '/PostFX_VegWind_IsEnabled\(\).*SURF_VEGETATION/ {print NR; exit}' "$TR_SHADE")"

if [ -z "$dispatch_line" ] || [ -z "$clear_line" ] || [ -z "$guard_line" ]; then
	fail "could not locate vegetation wind guard/dispatch/clear lines"
fi
if [ "$dispatch_line" -le "$guard_line" ] || [ $((dispatch_line - guard_line)) -gt 4 ]; then
	fail "vegetation dispatch is no longer directly guarded by SURF_VEGETATION branch"
fi
if [ "$clear_line" -le "$dispatch_line" ] || [ $((clear_line - dispatch_line)) -gt 4 ]; then
	fail "staging clear is no longer immediately after vegetation dispatch"
fi

dispatch_count="$(grep -Eho '\bvk_vegetation_wind_dispatch[[:space:]]*\(' "$TR_SHADE" "$VK_FRAME" | wc -l | tr -d '[:space:]')"
if [ "$dispatch_count" -ne 1 ]; then
	fail "expected exactly one vegetation dispatch call across tr_shade/vk_frame_submit, got $dispatch_count"
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
