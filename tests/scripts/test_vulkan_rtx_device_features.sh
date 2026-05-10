#!/usr/bin/env bash
# Regression checks for Vulkan RTX device feature gating.
# The RTX path is hardware-dependent, so pin the device-creation invariants
# that prevent enabling KHR ray tracing extensions without verified features.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${1:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
VK_INSTANCE="$PROJECT_ROOT/src/renderers/vulkan/vk_instance.c"

if [ ! -f "$VK_INSTANCE" ]; then
	echo "FAIL: missing file: $VK_INSTANCE" >&2
	exit 1
fi

python3 - "$VK_INSTANCE" <<'PY'
import re
import sys
from pathlib import Path

path = Path(sys.argv[1])
text = path.read_text()


def fail(message):
    print(f"FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def require_literal(literal, context):
    if literal not in text:
        fail(f"{context}: expected literal {literal!r}")


def require_regex(pattern, context, flags=0):
    if not re.search(pattern, text, flags):
        fail(f"{context}: expected pattern {pattern!r}")


def require_order(context, *needles):
    pos = -1
    for needle in needles:
        next_pos = text.find(needle, pos + 1)
        if next_pos < 0:
            fail(f"{context}: missing {needle!r}")
        if next_pos <= pos:
            fail(f"{context}: {needle!r} is out of order")
        pos = next_pos


def require_block(start, end, context):
    start_pos = text.find(start)
    if start_pos < 0:
        fail(f"{context}: missing start {start!r}")
    end_pos = text.find(end, start_pos)
    if end_pos < 0:
        fail(f"{context}: missing end {end!r}")
    return text[start_pos:end_pos]


def brace_block_from(start, context):
    start_pos = text.find(start)
    if start_pos < 0:
        fail(f"{context}: missing start {start!r}")
    brace_pos = text.find("{", start_pos)
    if brace_pos < 0:
        fail(f"{context}: missing opening brace")
    depth = 0
    for index in range(brace_pos, len(text)):
        char = text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return text[start_pos:index + 1]
    fail(f"{context}: missing closing brace")


require_literal("#ifdef USE_VULKAN_RTX", "RTX path must remain build gated")
require_literal("vk.rtxAvailable = qfalse;", "RTX availability reset")
if text.count("vk.rtxAvailable = qtrue;") != 1:
    fail("RTX availability should be set true at exactly one verified feature gate")

# Extension discovery must track all prerequisites independently. Checking
# extension names alone is not sufficient for Vulkan RT enablement.
for flag, extension in (
    ("rtxAccelStruct", "VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME"),
    ("rtxPipeline", "VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME"),
    ("rtxDeferredHostOps", "VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME"),
    ("rtxBufferDeviceAddress", "VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME"),
):
    require_literal(f"qboolean {flag} = qfalse;", f"{flag} declaration")
    require_literal(f"{flag} = qtrue;", f"{flag} extension probe")
    require_literal(extension, f"{flag} extension name")

require_regex(
    r"if \( rtxAccelStruct && rtxPipeline && rtxDeferredHostOps && "
    r"rtxBufferDeviceAddress && memoryRequirements2 \) \{",
    "RTX extension prerequisite gate",
)
require_literal('ri.Cvar_Get( "r_rtx", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );', "RTX cvar remains opt-in and latched")
require_literal("if ( qvkGetPhysicalDeviceFeatures2 == NULL ) {", "feature2 loader guard")
require_literal("device_extension_count + 4 > ARRAY_LEN( device_extension_list )", "extension-list capacity guard")

query_block = require_block(
    "vk.rtxAvailable = qfalse;",
    "\n#endif\n\t\tqvkGetPhysicalDeviceFeatures( physical_device, &device_features );",
    "RTX feature query block",
)
for literal in (
    "Com_Memset( &rtx_features2, 0, sizeof( rtx_features2 ) );",
    "Com_Memset( &rtx_vulkan12_features, 0, sizeof( rtx_vulkan12_features ) );",
    "Com_Memset( &rtx_accel_features, 0, sizeof( rtx_accel_features ) );",
    "Com_Memset( &rtx_pipeline_features, 0, sizeof( rtx_pipeline_features ) );",
    "rtx_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;",
    "rtx_vulkan12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;",
    "rtx_accel_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;",
    "rtx_pipeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;",
    "qvkGetPhysicalDeviceFeatures2( physical_device, &rtx_features2 );",
):
    if literal not in query_block:
        fail(f"RTX feature query block: missing {literal!r}")

for needle in (
    "rtx_features2.pNext = &rtx_vulkan12_features;",
    "rtx_vulkan12_features.pNext = &rtx_accel_features;",
    "rtx_accel_features.pNext = &rtx_pipeline_features;",
    "rtx_pipeline_features.pNext = NULL;",
    "qvkGetPhysicalDeviceFeatures2( physical_device, &rtx_features2 );",
):
    if needle not in query_block:
        fail(f"RTX feature query order: missing {needle!r}")
require_order(
    "RTX feature query order",
    "rtx_features2.pNext = &rtx_vulkan12_features;",
    "rtx_vulkan12_features.pNext = &rtx_accel_features;",
    "rtx_accel_features.pNext = &rtx_pipeline_features;",
    "rtx_pipeline_features.pNext = NULL;",
    "qvkGetPhysicalDeviceFeatures2( physical_device, &rtx_features2 );",
)

feature_gate = brace_block_from(
    "if ( rtx_vulkan12_features.bufferDeviceAddress && rtx_accel_features.accelerationStructure",
    "RTX required-feature gate",
)
for literal in (
    "&& rtx_pipeline_features.rayTracingPipeline",
    "device_extension_list[ device_extension_count++ ] = VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME;",
    "device_extension_list[ device_extension_count++ ] = VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME;",
    "device_extension_list[ device_extension_count++ ] = VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME;",
    "device_extension_list[ device_extension_count++ ] = VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME;",
    "vk.rtxAvailable = qtrue;",
    "rtx_vulkan12_features.bufferDeviceAddress = VK_TRUE;",
    "rtx_accel_features.accelerationStructure = VK_TRUE;",
    "rtx_pipeline_features.rayTracingPipeline = VK_TRUE;",
    "rtx_vulkan12_features.bufferDeviceAddressCaptureReplay = VK_FALSE;",
    "rtx_accel_features.accelerationStructureCaptureReplay = VK_FALSE;",
    "rtx_pipeline_features.rayTracingPipelineTraceRaysIndirect = VK_FALSE;",
):
    if literal not in feature_gate:
        fail(f"RTX required-feature gate: missing {literal!r}")
require_order(
    "RTX extension append order",
    "device_extension_list[ device_extension_count++ ] = VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME;",
    "device_extension_list[ device_extension_count++ ] = VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME;",
    "device_extension_list[ device_extension_count++ ] = VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME;",
    "device_extension_list[ device_extension_count++ ] = VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME;",
    "vk.rtxAvailable = qtrue;",
)

require_regex(
    r"if \( devAddrFeat\s*#ifdef USE_VULKAN_RTX\s*&& !vk\.rtxAvailable\s*#endif\s*\) \{",
    "debug buffer-device-address chain must be skipped when RTX owns it",
    re.MULTILINE,
)
require_order(
    "RTX create-device pNext wrapping",
    "if ( vk.rtxAvailable ) {",
    "Com_Memcpy( &rtx_features2.features, &features, sizeof( features ) );",
    "rtx_features2.pNext = &rtx_vulkan12_features;",
    "rtx_vulkan12_features.pNext = &rtx_accel_features;",
    "rtx_accel_features.pNext = &rtx_pipeline_features;",
    "rtx_pipeline_features.pNext = (void *)(uintptr_t)device_desc.pNext;",
    "device_desc.pNext = &rtx_features2;",
    "device_desc.pEnabledFeatures = NULL;",
    "qvkCreateDevice( physical_device, &device_desc, NULL, &vk.device );",
)

for function_name in (
    "vkGetBufferDeviceAddress",
    "vkCreateAccelerationStructureKHR",
    "vkDestroyAccelerationStructureKHR",
    "vkGetAccelerationStructureBuildSizesKHR",
    "vkGetAccelerationStructureDeviceAddressKHR",
    "vkCmdBuildAccelerationStructuresKHR",
    "vkCreateRayTracingPipelinesKHR",
    "vkGetRayTracingShaderGroupHandlesKHR",
    "vkCmdTraceRaysKHR",
):
    require_literal(f"INIT_DEVICE_FUNCTION_EXT( {function_name} );", f"{function_name} loader")

print("PASS: test_vulkan_rtx_device_features")
PY
