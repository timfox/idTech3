#!/usr/bin/env bash
# Steam Deck control path: binds, dual-stick look, START/BACK menu, Surf JS pad nav.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SURF_ROOT="$(cd "$ROOT/../Surf" 2>/dev/null && pwd || true)"
CFG="$ROOT/config/steamdeck.cfg"
CL_INPUT="$ROOT/runtime/client/core/cl_input.c"
CL_KEYS="$ROOT/runtime/client/core/cl_keys.c"
SDL="$ROOT/engine/platform/sdl/sdl_input.c"

fail() { echo "test_steamdeck_controls: $*" >&2; exit 1; }

[[ -f "$CFG" ]] || fail "missing steamdeck.cfg"
grep -Fq 'in_joystickUseAnalog "1"' "$CFG" || fail "analog sticks not enabled"
grep -Fq 'bind PAD0_A "+moveup"' "$CFG" || fail "missing jump bind"
grep -Fq 'bind PAD0_LEFTSTICK_UP "+forward"' "$CFG" || fail "missing left-stick move binds"
grep -Fq 'bind PAD0_RIGHTSTICK_LEFT "+left"' "$CFG" || fail "missing right-stick look binds"
grep -Fq 'j_yaw_axis "2"' "$CFG" || fail "missing yaw axis mapping"
grep -Fq 'j_pitch_axis "3"' "$CFG" || fail "missing pitch axis mapping"

grep -Fq 'AXIS_YAW' "$CL_INPUT" || fail "CL_JoystickMove missing AXIS_YAW look"
grep -Fq 'AXIS_PITCH' "$CL_INPUT" || fail "CL_JoystickMove missing AXIS_PITCH look"
grep -Fq 'value * 127 ) / 32767' "$CL_INPUT" || fail "joystick axis not normalized to ±127"

grep -Fq 'K_PAD0_START' "$CL_KEYS" || fail "START not treated as menu key"
grep -Fq 'K_PAD0_BACK' "$CL_KEYS" || fail "BACK not treated as menu key"
grep -Fq 'ui_surfMapSelect' "$CL_KEYS" || fail "escape does not pop map-select overlay"
grep -Fq 'CL_RemapGamepadMenuKey' "$CL_KEYS" || fail "native UI pad remap missing"

grep -Fq '127 * ((axis < 0)' "$SDL" || fail "gamepad axes not scaled to ±127"

if [[ -n "${SURF_ROOT:-}" && -d "$SURF_ROOT/ui/surf" ]]; then
	[[ -f "$SURF_ROOT/ui/surf/pad.js" ]] || fail "missing Surf ui/surf/pad.js"
	grep -Fq 'PAD0_DPAD_UP' "$SURF_ROOT/ui/surf/pad.js" || fail "pad.js missing dpad helpers"
	grep -Fq 'ui/surf/pad' "$SURF_ROOT/ui/surf/menu.js" || fail "menu.js does not require pad.js"
	grep -Fq 'ui/surf/pad' "$SURF_ROOT/ui/surf/mapselect.js" || fail "mapselect.js does not require pad.js"
	grep -Fq 'ui/surf/pad' "$SURF_ROOT/ui/surf/leaderboard.js" || fail "leaderboard.js does not require pad.js"
fi

grep -Fq 'Steam Deck controls' "$ROOT/docs/STEAM.md" || fail "STEAM.md missing Deck controls section"

echo "test_steamdeck_controls: PASS"
