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
VK_RESOURCE_DESTROY="$PROJECT_ROOT/src/renderers/vulkan/vk_resource_destroy.c"

assert_file_exists "$VK_INSTANCE"
assert_file_exists "$TR_SHADE"
assert_file_exists "$VK_FRAME_SUBMIT"
assert_file_exists "$VK_RESOURCE_DESTROY"

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

# Vegetation wind compute must happen after SURF_VEGETATION batching in RB_EndSurface.
assert_contains_literal "$TR_SHADE" "if ( PostFX_VegWind_IsEnabled() && tess.shader && ( tess.shader->surfaceFlags & SURF_VEGETATION ) ) {" "vegetation dispatch gate in RB_EndSurface"
assert_contains_literal "$TR_SHADE" "vk_vegetation_wind_dispatch();" "vegetation dispatch call in RB_EndSurface"
assert_contains_literal "$TR_SHADE" "vk_vegetation_clear_staging();" "vegetation staging clear after dispatch"

# Frame begin path must not dispatch vegetation compute directly (would run before tess upload).
assert_not_matches_regex "$VK_FRAME_SUBMIT" "^[[:space:]]*vk_vegetation_wind_dispatch[[:space:]]*\\(" "no early vegetation dispatch in vk_begin_frame"
assert_contains_literal "$VK_FRAME_SUBMIT" "before tessellation and see vertexCount==0." "frame-submit comment documents ordering risk"

python3 - "$VK_RESOURCE_DESTROY" "$VK_FRAME_SUBMIT" <<'PY'
import re
import sys


def fail(message):
    print(f"FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def strip_comments(source):
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)


def extract_function(source, name):
    pattern = re.compile(r"(^|\n)\s*(?:static\s+)?[A-Za-z_][\w\s\*]*\b" + re.escape(name) + r"\s*\([^;]*\)\s*\{")
    match = pattern.search(source)
    if not match:
        fail(f"missing function {name}")

    brace = source.find("{", match.start())
    depth = 0
    for index in range(brace, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():index + 1]

    fail(f"unterminated function {name}")


def require_contains(text, needle, context):
    if needle not in text:
        fail(f"{context}: expected {needle!r}")


def require_regex(text, pattern, context):
    if not re.search(pattern, text, flags=re.S):
        fail(f"{context}: expected pattern {pattern!r}")


def forbid_regex(text, pattern, context):
    if re.search(pattern, text, flags=re.S):
        fail(f"{context}: unexpected pattern {pattern!r}")


resource_destroy_path, frame_submit_path = sys.argv[1:3]
with open(resource_destroy_path, "r", encoding="utf-8") as handle:
    resource_destroy = handle.read()
with open(frame_submit_path, "r", encoding="utf-8") as handle:
    frame_submit = handle.read()

world_destroy = extract_function(resource_destroy, "vk_destroy_world_graphics_pipelines")
world_destroy_code = strip_comments(world_destroy)
require_contains(
    world_destroy,
    "Keep vk.pipelines[i].def and do not shrink pipelines_count",
    "world pipeline invalidation documents cached pipeline-index invariant",
)
require_contains(
    world_destroy,
    "shader_t::vk_pipeline",
    "world pipeline invalidation documents shader cache risk",
)
require_regex(
    world_destroy_code,
    r"for\s*\([^;]*vk\.pipelines_world_base[^;]*;\s*i\s*<\s*vk\.pipelines_count\s*;",
    "world pipeline invalidation iterates existing world pipeline rows",
)
require_contains(
    world_destroy_code,
    "qvkDestroyPipeline( vk.device, vk.pipelines[i].handle[j], NULL );",
    "world pipeline invalidation destroys GPU handles",
)
require_contains(
    world_destroy_code,
    "vk.pipelines[i].handle[j] = VK_NULL_HANDLE;",
    "world pipeline invalidation clears destroyed handles",
)
forbid_regex(
    world_destroy_code,
    r"vk\.pipelines_count\s*=",
    "world pipeline invalidation must preserve cached shader pipeline indices",
)
forbid_regex(
    world_destroy_code,
    r"Com_Memset\s*\(\s*&?vk\.pipelines\b",
    "world pipeline invalidation must preserve pipeline definitions",
)

begin_frame_code = strip_comments(extract_function(frame_submit, "vk_begin_frame"))
require_contains(
    begin_frame_code,
    "r_forwardPlusShade && r_forwardPlusShade->modified",
    "Forward+ shade cvar modification is handled during frame begin",
)
require_contains(
    begin_frame_code,
    "vk_destroy_world_graphics_pipelines();",
    "Forward+ shade changes invalidate only world graphics pipelines",
)
forbid_regex(
    begin_frame_code,
    r"vk_destroy_pipelines\s*\(",
    "Forward+ shade changes must not destroy unrelated post-process pipelines",
)
PY

echo "PASS: test_vulkan_regression_source_guards"
