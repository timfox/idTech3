#!/usr/bin/env bash
# Static gates for OpenHMD VR wiring (no HMD required).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail() { echo "test_openhmd: $*" >&2; exit 1; }

[[ -f "$ROOT/runtime/client/platform/cl_openhmd.c" ]] || fail "missing cl_openhmd.c"
[[ -f "$ROOT/runtime/client/platform/cl_openhmd.h" ]] || fail "missing cl_openhmd.h"
[[ -f "$ROOT/third_party/openhmd/openhmd.h" ]] || fail "missing vendored openhmd.h"
[[ -f "$ROOT/docs/OPENHMD.md" ]] || fail "missing docs/OPENHMD.md"
[[ -f "$ROOT/config/openhmd.cfg" ]] || fail "missing config/openhmd.cfg"

grep -q 'OPTION(USE_OPENHMD' "$ROOT/CMakeLists.txt" || fail "missing USE_OPENHMD option"
grep -q 'USE_OPENHMD=1' "$ROOT/CMakeLists.txt" || fail "client missing USE_OPENHMD define"
grep -q 'cl_openhmd.c' "$ROOT/cmake/client/ClientSources.cmake" || fail "cl_openhmd.c not in client sources"

grep -q 'OHMD_Init' "$ROOT/runtime/client/core/cl_main.c" || fail "CL_Init missing OHMD_Init"
grep -q 'OHMD_Shutdown' "$ROOT/runtime/client/core/cl_lifecycle.c" || fail "CL_Shutdown missing OHMD_Shutdown"
grep -q 'OHMD_Frame' "$ROOT/runtime/client/core/cl_frame.c" || fail "CL_Frame missing OHMD_Frame"
grep -q 'OHMD_ApplyViewAngles' "$ROOT/runtime/client/core/cl_input.c" || fail "CL_CreateCmd missing ApplyViewAngles"
grep -q 'OHMD_WantStereo' "$ROOT/runtime/client/shell/cl_scrn.c" || fail "SCR_UpdateScreen missing WantStereo"
grep -q 'Software stereo' "$ROOT/renderers/vulkan/tr_cmds.c" || fail "RE_BeginFrame still fatals on software stereo"

grep -q 'dlopen' "$ROOT/runtime/client/platform/cl_openhmd.c" || fail "expected runtime dlopen"
grep -q 'ohmd_status' "$ROOT/runtime/client/platform/cl_openhmd.c" || fail "missing ohmd_status"
grep -q 'OHMD_ROTATION_QUAT' "$ROOT/runtime/client/platform/cl_openhmd.c" || fail "missing quat tracking"
grep -q 'vr_openhmd' "$ROOT/config/openhmd.cfg" || fail "openhmd.cfg missing vr_openhmd"

echo "test_openhmd: PASS"
