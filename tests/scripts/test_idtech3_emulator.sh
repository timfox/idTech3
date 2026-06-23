#!/usr/bin/env bash
# Contract: idTech3-Emulator optional submodule + in-engine bridge wiring.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

grep -q 'third_party/idtech3-emulator' .gitmodules || fail '.gitmodules missing idtech3-emulator path'
grep -q 'idTech3-Emulator' .gitmodules || fail '.gitmodules missing idTech3-Emulator url'

grep -q 'IdTech3Emulator.cmake' CMakeLists.txt || fail 'CMakeLists must include IdTech3Emulator.cmake'
grep -q 'USE_IDTECH3_EMULATOR' CMakeLists.txt || fail 'USE_IDTECH3_EMULATOR option missing'
grep -q 'emulator_process.c' cmake/client/ClientExtensionSources.cmake || fail 'client extension manifest missing emulator_process'
grep -q 'vk_emulator_screen.c' cmake/renderers/VulkanExtensionSources.cmake || fail 'vk_emulator_screen missing from renderer manifest'

grep -q '\-\-emulator' scripts/init_optional_submodules.sh || fail 'init script missing --emulator'
grep -q 'cl_emulator' runtime/client/cl_emulator.c || fail 'cl_emulator.c missing'
grep -q 'emulator_input.c' cmake/client/ClientExtensionSources.cmake || fail 'client extension manifest missing emulator_input'
grep -q 'emulator_capture' extensions/emulator/emulator_console.c || fail 'emulator_capture command missing'
grep -q 'EMULATOR_INPUT_SHM_NAME' extensions/emulator/emulator_types.h || fail 'input shm contract missing'
grep -q 'CL_Emulator_KeyEvent' runtime/client/cl_emulator.c || fail 'keyboard capture hook missing'
test -f examples/demo_game/mod/demo_emulator.cfg || fail 'demo_emulator.cfg missing'
grep -q 'EmulatorUploadFrame' renderers/common/tr_public.h || fail 'tr_public missing EmulatorUploadFrame'

grep -q 'IDTECH3_EMULATOR' docs/IDTECH3_EMULATOR.md || fail 'docs/IDTECH3_EMULATOR.md missing'

pass 'idTech3-Emulator submodule + bridge contract'
