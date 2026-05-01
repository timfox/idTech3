#!/usr/bin/env bash
# Regression coverage for optional VK_NV_mesh_shader device-extension opt-in.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

python3 - "$PROJECT_ROOT" <<'PY'
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def read_rel(path: str) -> str:
    full = root / path
    try:
        return full.read_text(encoding="utf-8")
    except OSError as exc:
        fail(f"could not read {path}: {exc}")


def assert_contains(text: str, needle: str, context: str) -> None:
    if needle not in text:
        fail(f"{context}: expected to find {needle!r}")


def assert_regex(text: str, pattern: str, context: str) -> None:
    if not re.search(pattern, text, re.DOTALL):
        fail(f"{context}: expected pattern {pattern}")


tr_init = read_rel("src/renderers/vulkan/tr_init.c")
tr_local = read_rel("src/renderers/vulkan/tr_local.h")
vk_h = read_rel("src/renderers/vulkan/vk.h")
vk_instance = read_rel("src/renderers/vulkan/vk_instance.c")

# The cvar is the user-facing safety valve: default off, latched, range-limited,
# documented as restart-required, and exported like the other Vulkan renderer cvars.
assert_contains(tr_init, 'cvar_t\t*r_vk_meshShaderNV;', "tr_init cvar definition")
assert_regex(
    tr_init,
    r'r_vk_meshShaderNV\s*=\s*ri\.Cvar_Get\(\s*"r_vk_meshShaderNV"\s*,\s*"0"\s*,\s*CVAR_ARCHIVE_ND\s*\|\s*CVAR_LATCH\s*\)',
    "mesh shader cvar remains default-off and latched",
)
assert_regex(
    tr_init,
    r'ri\.Cvar_CheckRange\(\s*r_vk_meshShaderNV\s*,\s*"0"\s*,\s*"1"\s*,\s*CV_INTEGER\s*\)',
    "mesh shader cvar keeps integer 0/1 range",
)
assert_regex(
    tr_init,
    r'ri\.Cvar_SetDescription\(\s*r_vk_meshShaderNV\s*,.*?Requires vid_restart\.',
    "mesh shader cvar documents restart requirement",
)
assert_contains(tr_local, 'extern cvar_t\t*r_vk_meshShaderNV;', "tr_local cvar export")
assert_contains(vk_h, 'qboolean meshShaderNV;', "vk state records mesh shader enablement")

# Device extension discovery must only mark driver support. Enabling the
# extension requires both driver support and explicit user opt-in below.
assert_regex(
    vk_instance,
    r'else\s+if\s*\(\s*strcmp\(\s*ext\s*,\s*VK_NV_MESH_SHADER_EXTENSION_NAME\s*\)\s*==\s*0\s*\)\s*\{\s*nvMeshShader\s*=\s*qtrue;\s*\}',
    "mesh shader discovery records driver support only",
)
assert_contains(vk_instance, "vk.meshShaderNV = qfalse;", "mesh shader state resets before extension selection")

extension_append = 'device_extension_list[ device_extension_count++ ] = VK_NV_MESH_SHADER_EXTENSION_NAME;'
if vk_instance.count(extension_append) != 1:
    fail("mesh shader extension should be appended at exactly one call site")

assert_regex(
    vk_instance,
    r'if\s*\(\s*nvMeshShader\s*&&\s*r_vk_meshShaderNV\s*&&\s*r_vk_meshShaderNV->integer\s*&&\s*device_extension_count\s*<\s*ARRAY_LEN\(\s*device_extension_list\s*\)\s*\)\s*\{'
    r'\s*device_extension_list\[\s*device_extension_count\+\+\s*\]\s*=\s*VK_NV_MESH_SHADER_EXTENSION_NAME;'
    r'\s*vk\.meshShaderNV\s*=\s*qtrue;'
    r'\s*ri\.Printf\(\s*PRINT_ALL\s*,\s*"\[VK\] VK_NV_mesh_shader enabled',
    "mesh shader extension remains gated by support, cvar opt-in, and list capacity",
)

# When enabled, request only the mesh-shader feature and preserve any existing
# pNext chain. Task shaders stay disabled because there is no task draw path.
assert_regex(
    vk_instance,
    r'if\s*\(\s*vk\.meshShaderNV\s*\)\s*\{'
    r'\s*Com_Memset\(\s*&mesh_shader_features_nv\s*,\s*0\s*,\s*sizeof\(\s*mesh_shader_features_nv\s*\)\s*\);'
    r'\s*mesh_shader_features_nv\.sType\s*=\s*VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;'
    r'\s*mesh_shader_features_nv\.pNext\s*=\s*\(void \*\)\(uintptr_t\)device_desc\.pNext;'
    r'\s*mesh_shader_features_nv\.taskShader\s*=\s*VK_FALSE;'
    r'\s*mesh_shader_features_nv\.meshShader\s*=\s*VK_TRUE;'
    r'\s*device_desc\.pNext\s*=\s*&mesh_shader_features_nv;',
    "mesh shader feature chain preserves pNext and disables task shaders",
)

print("PASS: test_vulkan_mesh_shader_opt_in")
PY
