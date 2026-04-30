#!/usr/bin/env bash
# Guard optional VK_NV_mesh_shader setup so it remains an explicit, safe opt-in.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

python3 - "$PROJECT_ROOT" <<'PY'
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def read(relpath: str) -> str:
    path = root / relpath
    if not path.is_file():
        fail(f"missing file: {relpath}")
    return path.read_text(encoding="utf-8")


def require(text: str, needle: str, context: str) -> None:
    if needle not in text:
        fail(f"{context}: missing {needle!r}")


def require_regex(text: str, pattern: str, context: str) -> None:
    if not re.search(pattern, text, re.S):
        fail(f"{context}: pattern not found: {pattern}")


vk_instance = read("src/renderers/vulkan/vk_instance.c")
tr_init = read("src/renderers/vulkan/tr_init.c")
tr_local = read("src/renderers/vulkan/tr_local.h")
vk_h = read("src/renderers/vulkan/vk.h")

# Public renderer configuration must stay explicit, restart-bound, and default-off.
require(
    tr_init,
    'r_vk_meshShaderNV = ri.Cvar_Get( "r_vk_meshShaderNV", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );',
    "mesh shader cvar registration",
)
require(
    tr_init,
    'ri.Cvar_CheckRange( r_vk_meshShaderNV, "0", "1", CV_INTEGER );',
    "mesh shader cvar range",
)
require(
    tr_init,
    "Default off: extension is enabled for future mesh pipelines only; no draw path uses mesh shaders yet. Requires vid_restart.",
    "mesh shader cvar description",
)
require(tr_local, "extern cvar_t\t*r_vk_meshShaderNV;", "mesh shader cvar declaration")
require(vk_h, "qboolean meshShaderNV;", "mesh shader device state")

# The extension can be detected from the driver, but it must only be appended
# when the driver supports it, the user opted in, and there is room in the list.
require_regex(
    vk_instance,
    r'else if \( strcmp\( ext, VK_NV_MESH_SHADER_EXTENSION_NAME \) == 0 \) \{\s*nvMeshShader = qtrue;\s*\}',
    "mesh shader extension detection",
)
require_regex(
    vk_instance,
    r'vk\.meshShaderNV = qfalse;.*?if \( nvMeshShader && r_vk_meshShaderNV && r_vk_meshShaderNV->integer &&\s*'
    r'device_extension_count < ARRAY_LEN\( device_extension_list \) \) \{\s*'
    r'device_extension_list\[ device_extension_count\+\+ \] = VK_NV_MESH_SHADER_EXTENSION_NAME;\s*'
    r'vk\.meshShaderNV = qtrue;\s*'
    r'ri\.Printf\( PRINT_ALL, "\[VK\] VK_NV_mesh_shader enabled \(experimental; no mesh draw path yet\)\\n" \);\s*'
    r'\}',
    "mesh shader extension opt-in gate",
)

# Device features must be chained only when the extension was actually enabled.
# Keep task shaders off until a draw path exists; enable only meshShader.
require_regex(
    vk_instance,
    r'if \( vk\.meshShaderNV \) \{\s*'
    r'Com_Memset\( &mesh_shader_features_nv, 0, sizeof\( mesh_shader_features_nv \) \);\s*'
    r'mesh_shader_features_nv\.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;\s*'
    r'mesh_shader_features_nv\.pNext = \(void \*\)\(uintptr_t\)device_desc\.pNext;\s*'
    r'mesh_shader_features_nv\.taskShader = VK_FALSE;\s*'
    r'mesh_shader_features_nv\.meshShader = VK_TRUE;\s*'
    r'device_desc\.pNext = &mesh_shader_features_nv;\s*'
    r'\}',
    "mesh shader feature pNext chain",
)

if vk_instance.count("VK_NV_MESH_SHADER_EXTENSION_NAME") < 2:
    fail("mesh shader extension should be both detected and conditionally enabled")

print("PASS: test_vulkan_mesh_shader_opt_in")
PY
