#!/usr/bin/env bash
# SDL3 input/camera source contract plus runtime SDL3 subsystem smoke.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

mode="${1:-all}"

source_checks() {
	local src="engine/platform/sdl/sdl_input.c"
	[[ -f "$src" ]] || fail "missing SDL input source"
	rg -q 'SDL_INIT_CAMERA' "$src" || fail "missing SDL3 camera subsystem guard"
	rg -q 'SDL_GetCameras' "$src" || fail "missing SDL3 camera enumeration"
	rg -q 'SDL_OpenCamera' "$src" || fail "missing SDL3 camera open path"
	rg -q 'SDL_AcquireCameraFrame' "$src" || fail "missing SDL3 camera frame polling"
	rg -q 'webcam_list' "$src" || fail "missing webcam_list command"
	rg -q 'webcam_status' "$src" || fail "missing webcam_status command"
	rg -q 'webcam_start' "$src" || fail "missing webcam_start command"
	rg -q 'webcam_stop' "$src" || fail "missing webcam_stop command"
	rg -q 'cl_webcamEnable' "$src" || fail "missing webcam opt-in cvar"
	rg -q 'SDL_AddGamepadMappingsFromFile' "$src" || fail "missing gamepad mapping loader"
	rg -q 'SDL_SetGamepadEventsEnabled' "$src" || fail "missing gamepad event control"
	rg -q 'SDL_RumbleGamepad' "$src" || fail "missing gamepad rumble command"
	rg -q 'SDL_RumbleGamepadTriggers' "$src" || fail "missing gamepad trigger rumble command"
	rg -q 'gamepad_status' "$src" || fail "missing gamepad_status command"
	rg -q 'gamepad_load_mappings' "$src" || fail "missing gamepad_load_mappings command"
	rg -q 'unit_sdl3_features' CMakeLists.txt || fail "missing SDL3 runtime unit registration"
	pass "SDL3 input/camera source contract"
}

runtime_checks() {
	local bin="${BUILD_DIR:-$ROOT/build-vk-Release}/unit_sdl3_features"
	if [[ ! -x "$bin" ]]; then
		if [[ "${IDTECH3_RUNTIME_REQUIRED:-0}" == "1" ]]; then
			fail "unit_sdl3_features is not built"
		fi
		echo "SKIP: SDL3 runtime smoke (unit_sdl3_features not built)"
		return 0
	fi
	SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}" SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}" "$bin"
	pass "SDL3 runtime subsystem smoke"
}

case "$mode" in
	source) source_checks ;;
	runtime) runtime_checks ;;
	all) source_checks; runtime_checks ;;
	*) echo "usage: $0 [source|runtime|all]" >&2; exit 2 ;;
esac
