#!/usr/bin/env bash
# Regression coverage for optional VK_NV_mesh_shader device setup.
# The test is source-invariant based so it runs without Vulkan hardware.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

python3 - "$PROJECT_ROOT" <<'PY'
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])


def fail(message):
	print(f"FAIL: {message}", file=sys.stderr)
	sys.exit(1)


def read(relpath):
	path = root / relpath
	if not path.is_file():
		fail(f"missing required source file: {relpath}")
	return path.read_text(encoding="utf-8")


def require(text, needle, context):
	if needle not in text:
		fail(f"{context}: missing {needle!r}")


def require_regex(text, pattern, context):
	if not re.search(pattern, text, re.S):
		fail(f"{context}: pattern not found: {pattern}")


def count(text, needle):
	return text.count(needle)


vk_instance = read("src/renderers/vulkan/vk_instance.c")
tr_init = read("src/renderers/vulkan/tr_init.c")
tr_local = read("src/renderers/vulkan/tr_local.h")
vk_h = read("src/renderers/vulkan/vk.h")

# Public cvar contract: experimental extension stays opt-in and latched.
require(
	tr_init,
	'r_vk_meshShaderNV = ri.Cvar_Get( "r_vk_meshShaderNV", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );',
	"mesh shader cvar default/flags",
)
require(
	tr_init,
	'ri.Cvar_CheckRange( r_vk_meshShaderNV, "0", "1", CV_INTEGER );',
	"mesh shader cvar range",
)
require(tr_init, "Default off", "mesh shader cvar description")
require(tr_init, "Requires vid_restart", "mesh shader cvar description")
require(tr_init, "ri.Cvar_SetGroup( r_vk_meshShaderNV, CVG_RENDERER );", "mesh shader cvar group")
require(tr_local, "extern cvar_t\t*r_vk_meshShaderNV;", "mesh shader cvar declaration")
require(vk_h, "qboolean meshShaderNV;", "mesh shader state bit")
require(vk_h, "no mesh pipelines yet", "mesh shader state documentation")

# Device extension detection must be separate from extension enablement.
require(vk_instance, "qboolean nvMeshShader = qfalse;", "mesh shader extension probe state")
require_regex(
	vk_instance,
	r'else if \( strcmp\( ext, VK_NV_MESH_SHADER_EXTENSION_NAME \) == 0 \) \{\s+nvMeshShader = qtrue;\s+\}',
	"mesh shader extension detection",
)
require(vk_instance, "vk.meshShaderNV = qfalse;", "mesh shader state reset before device creation")

append_line = "device_extension_list[ device_extension_count++ ] = VK_NV_MESH_SHADER_EXTENSION_NAME;"
if count(vk_instance, append_line) != 1:
	fail(f"mesh shader extension should be appended exactly once, saw {count(vk_instance, append_line)}")

require_regex(
	vk_instance,
	r'if \( nvMeshShader && r_vk_meshShaderNV && r_vk_meshShaderNV->integer &&\s+device_extension_count < ARRAY_LEN\( device_extension_list \) \) \{\s+'
	r'device_extension_list\[ device_extension_count\+\+ \] = VK_NV_MESH_SHADER_EXTENSION_NAME;\s+'
	r'vk\.meshShaderNV = qtrue;\s+'
	r'ri\.Printf\( PRINT_ALL, "\[VK\] VK_NV_mesh_shader enabled \(experimental; no mesh draw path yet\)\\n" \);\s+'
	r'\}',
	"mesh shader extension opt-in gate",
)

# Feature chaining must only happen after the extension is selected, and must
# keep task shaders disabled because there is no task/mesh draw path yet.
require_regex(
	vk_instance,
	r'if \( vk\.meshShaderNV \) \{\s+'
	r'Com_Memset\( &mesh_shader_features_nv, 0, sizeof\( mesh_shader_features_nv \) \);\s+'
	r'mesh_shader_features_nv\.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;\s+'
	r'mesh_shader_features_nv\.pNext = \(void \*\)\(uintptr_t\)device_desc\.pNext;\s+'
	r'mesh_shader_features_nv\.taskShader = VK_FALSE;\s+'
	r'mesh_shader_features_nv\.meshShader = VK_TRUE;\s+'
	r'device_desc\.pNext = &mesh_shader_features_nv;\s+'
	r'\}',
	"mesh shader feature pNext chain",
)

print("PASS: test_vulkan_mesh_shader_opt_in")
PY
