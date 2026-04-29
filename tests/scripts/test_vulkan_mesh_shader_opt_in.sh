#!/usr/bin/env bash
# Regression checks for optional VK_NV_mesh_shader device setup.
# This keeps the experimental extension explicitly opt-in until a draw path exists.
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

assert_not_contains() {
	local haystack="$1"
	local needle="$2"
	local context="$3"
	if [[ "$haystack" == *"$needle"* ]]; then
		fail "$context: unexpected '$needle'"
	fi
}

assert_single_occurrence() {
	local file="$1"
	local needle="$2"
	local context="$3"
	local count
	count="$(grep -F -c "$needle" "$file" || true)"
	if [ "$count" -ne 1 ]; then
		fail "$context: expected one '$needle' occurrence in $file, found $count"
	fi
}

extract_function() {
	local file="$1"
	local function_name="$2"
	python3 - "$file" "$function_name" <<'PY'
import re
import sys

path, name = sys.argv[1], sys.argv[2]
text = open(path, encoding="utf-8").read()
match = re.search(r'\b%s\s*\([^)]*\)\s*\{' % re.escape(name), text)
if not match:
    sys.exit(1)

start = match.start()
brace = text.find("{", match.end() - 1)
depth = 0
for index in range(brace, len(text)):
    char = text[index]
    if char == "{":
        depth += 1
    elif char == "}":
        depth -= 1
        if depth == 0:
            print(text[start:index + 1])
            sys.exit(0)

sys.exit(1)
PY
}

TR_INIT="$PROJECT_ROOT/src/renderers/vulkan/tr_init.c"
TR_LOCAL="$PROJECT_ROOT/src/renderers/vulkan/tr_local.h"
VK_H="$PROJECT_ROOT/src/renderers/vulkan/vk.h"
VK_INSTANCE="$PROJECT_ROOT/src/renderers/vulkan/vk_instance.c"

for required in "$TR_INIT" "$TR_LOCAL" "$VK_H" "$VK_INSTANCE"; do
	[ -f "$required" ] || fail "required source file missing: $required"
done

assert_single_occurrence "$TR_INIT" 'cvar_t	*r_vk_meshShaderNV;' "mesh-shader cvar definition"
assert_contains "$(<"$TR_INIT")" 'r_vk_meshShaderNV = ri.Cvar_Get( "r_vk_meshShaderNV", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );' "mesh-shader cvar defaults off and is latched"
assert_contains "$(<"$TR_INIT")" 'ri.Cvar_CheckRange( r_vk_meshShaderNV, "0", "1", CV_INTEGER );' "mesh-shader cvar integer range"
assert_contains "$(<"$TR_INIT")" 'Requires vid_restart.' "mesh-shader cvar restart warning"
assert_contains "$(<"$TR_LOCAL")" 'extern cvar_t	*r_vk_meshShaderNV;' "mesh-shader cvar declaration"
assert_contains "$(<"$VK_H")" 'qboolean meshShaderNV;' "mesh-shader runtime state"

vk_create_device="$(extract_function "$VK_INSTANCE" "vk_create_device")" || fail "could not extract vk_create_device"

assert_contains "$vk_create_device" 'VkPhysicalDeviceMeshShaderFeaturesNV mesh_shader_features_nv;' "mesh-shader feature struct"
assert_contains "$vk_create_device" 'qboolean nvMeshShader = qfalse;' "mesh-shader driver-support flag"
assert_contains "$vk_create_device" 'strcmp( ext, VK_NV_MESH_SHADER_EXTENSION_NAME ) == 0' "mesh-shader extension detection"
assert_contains "$vk_create_device" 'nvMeshShader = qtrue;' "mesh-shader driver-support assignment"
assert_contains "$vk_create_device" 'vk.meshShaderNV = qfalse;' "mesh-shader state reset before selection"

opt_in_block="$(python3 - <<'PY' "$vk_create_device"
import re
import sys

text = sys.argv[1]
match = re.search(
    r'if\s*\(\s*nvMeshShader\s*&&\s*r_vk_meshShaderNV\s*&&\s*r_vk_meshShaderNV->integer\s*&&\s*'
    r'device_extension_count\s*<\s*ARRAY_LEN\(\s*device_extension_list\s*\)\s*\)\s*\{(?P<body>.*?)\n\t\t\}',
    text,
    re.S,
)
if not match:
    sys.exit(1)
print(match.group(0))
PY
)" || fail "could not find gated VK_NV_mesh_shader opt-in block"

assert_contains "$opt_in_block" 'device_extension_list[ device_extension_count++ ] = VK_NV_MESH_SHADER_EXTENSION_NAME;' "mesh-shader extension append"
assert_contains "$opt_in_block" 'vk.meshShaderNV = qtrue;' "mesh-shader state enabled only inside opt-in block"
assert_contains "$opt_in_block" 'VK_NV_mesh_shader enabled' "mesh-shader startup log"
assert_not_contains "${vk_create_device%%"$opt_in_block"*}" 'VK_NV_MESH_SHADER_EXTENSION_NAME;' "mesh-shader extension must not be appended before opt-in gate"

feature_chain_block="$(python3 - <<'PY' "$vk_create_device"
import re
import sys

text = sys.argv[1]
match = re.search(r'if\s*\(\s*vk\.meshShaderNV\s*\)\s*\{(?P<body>.*?)\n\t\t\}', text, re.S)
if not match:
    sys.exit(1)
print(match.group(0))
PY
)" || fail "could not find mesh-shader feature-chain block"

assert_contains "$feature_chain_block" 'mesh_shader_features_nv.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;' "mesh-shader feature sType"
assert_contains "$feature_chain_block" 'mesh_shader_features_nv.pNext = (void *)(uintptr_t)device_desc.pNext;' "mesh-shader feature preserves existing pNext chain"
assert_contains "$feature_chain_block" 'mesh_shader_features_nv.taskShader = VK_FALSE;' "mesh-shader task shader disabled"
assert_contains "$feature_chain_block" 'mesh_shader_features_nv.meshShader = VK_TRUE;' "mesh-shader feature enabled"
assert_contains "$feature_chain_block" 'device_desc.pNext = &mesh_shader_features_nv;' "mesh-shader feature becomes pNext head"

echo "PASS: test_vulkan_mesh_shader_opt_in"
