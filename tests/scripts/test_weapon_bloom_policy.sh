#!/usr/bin/env bash
# Static gate (+ optional live launch): weapon bloom policy for Architecture B
# Temporal Weapon Resolve. Guards the single combined-HDR bloom contract:
# weapon composited before one global bloom, no silent class descriptor
# substitution. See docs/TEMPORAL_WEAPON_VALIDATION.md and
# docs/TEMPORAL_RESOURCE_OWNERSHIP.md.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail=0

pass() { echo "PASS: $*"; }
failmsg() { echo "FAIL: $*"; fail=1; }

SURF_CFG="$ROOT/config/surf.cfg"
VK_TEMPORAL_C="$ROOT/renderers/vulkan/vk_temporal.c"
VK_TEMPORAL_H="$ROOT/renderers/vulkan/vk_temporal.h"
VK_FRAME_SUBMIT="$ROOT/renderers/vulkan/vk_frame_submit.c"
VK_FRAME_END="$ROOT/renderers/vulkan/vk_frame_end.c"

for f in "$SURF_CFG" "$VK_TEMPORAL_C" "$VK_TEMPORAL_H" "$VK_FRAME_SUBMIT" "$VK_FRAME_END"; do
	[[ -f "$f" ]] || { failmsg "missing required file: $f"; }
done
[[ "$fail" -eq 0 ]] || { echo "test_weapon_bloom_policy: FAIL"; exit 1; }

# --- 1. Surf ships combined-HDR weapon bloom (mode 1) ---
if grep -qE '^\s*seta?\s+r_weaponBloomMode\s+"?1"?' "$SURF_CFG"; then
	pass "config/surf.cfg sets r_weaponBloomMode 1"
else
	failmsg "config/surf.cfg must set r_weaponBloomMode 1"
fi

# --- 2. Defer helper exists and only defers when weapon bloom mode > 0 ---
if grep -q 'vk_temporal_defer_bloom_for_weapon' "$VK_TEMPORAL_H"; then
	pass "vk_temporal_defer_bloom_for_weapon declared in vk_temporal.h"
else
	failmsg "vk_temporal_defer_bloom_for_weapon declaration missing from vk_temporal.h"
fi

defer_body="$(awk '/qboolean vk_temporal_defer_bloom_for_weapon/,/^}/' "$VK_TEMPORAL_C")"
if [[ -z "$defer_body" ]]; then
	failmsg "vk_temporal_defer_bloom_for_weapon definition missing from vk_temporal.c"
elif echo "$defer_body" | grep -qE 'r_weaponBloomMode->integer\s*(>\s*0|==\s*1)'; then
	pass "defer helper gates on r_weaponBloomMode (mode>0 defers, mode 0 keeps no-weapon-bloom comparison)"
else
	failmsg "vk_temporal_defer_bloom_for_weapon must check r_weaponBloomMode->integer > 0 (or == 1)"
fi
if echo "$defer_body" | grep -q 'r_bloom' &&
	echo "$defer_body" | grep -q 'vk_temporal_want_weapon_after_world_post'; then
	pass "defer helper also requires r_bloom and weapon-after-world-post"
else
	failmsg "defer helper must require r_bloom and vk_temporal_want_weapon_after_world_post"
fi

# --- 3. Pre-weapon bloom is skipped and re-run after weapon composite ---
if grep -q '!vk_temporal_defer_bloom_for_weapon()' "$VK_FRAME_SUBMIT"; then
	pass "vk_frame_submit.c skips pre-weapon bloom when deferring"
else
	failmsg "vk_frame_submit.c must skip the pre-weapon bloom pass when deferring for the weapon"
fi
if grep -A4 'vk_temporal_defer_bloom_for_weapon() && !backEnd.doneBloom' "$VK_FRAME_SUBMIT" | grep -q 'vk_bloom()'; then
	pass "vk_frame_submit.c re-runs bloom after weapon composite (post-TAA, once)"
else
	failmsg "vk_frame_submit.c must re-run vk_bloom() after the weapon composite when bloom was deferred"
fi

# --- 3b. Mode 2 dedicated weapon bloom path ---
if grep -q 'vk_weapon_bloom' "$VK_TEMPORAL_H" &&
	grep -q 'vk_temporal_want_dedicated_weapon_bloom' "$VK_TEMPORAL_H"; then
	pass "dedicated weapon bloom helpers declared"
else
	failmsg "vk_weapon_bloom / vk_temporal_want_dedicated_weapon_bloom must be declared"
fi
if grep -q 'vk_weapon_bloom()' "$VK_FRAME_SUBMIT" &&
	grep -q 'vk_temporal_want_dedicated_weapon_bloom' "$VK_FRAME_SUBMIT"; then
	pass "vk_frame_submit.c calls dedicated weapon bloom for mode 2"
else
	failmsg "vk_frame_submit.c must call vk_weapon_bloom() when want_dedicated_weapon_bloom"
fi
if [[ -f "$ROOT/renderers/vulkan/shaders/glsl/weapon_bloom_extract.frag" ]] &&
	grep -q 'vk_weapon_bloom' "$ROOT/renderers/vulkan/vk_postfx_passes.c"; then
	pass "weapon bloom extract shader + vk_weapon_bloom implementation present"
else
	failmsg "weapon_bloom_extract.frag and vk_weapon_bloom() implementation required for mode 2"
fi

# --- 4. No silent class=reactive descriptor fallback ---
if grep -qE 'class_set\s*=\s*reactive_set' "$VK_FRAME_END"; then
	failmsg "vk_frame_end.c silently substitutes the reactive descriptor as class data"
else
	pass "no silent class_set = reactive_set fallback in vk_frame_end.c"
fi
if grep -q 'TemporalUnclassifiedR8' "$VK_FRAME_END" &&
	grep -q 'temporal_class_fallback_descriptor' "$VK_FRAME_END"; then
	pass "class descriptor fault binds TemporalUnclassifiedR8 with an explicit warning"
else
	failmsg "class descriptor fault must bind TemporalUnclassifiedR8 (dedicated fallback) and warn"
fi

if [[ "$fail" -ne 0 ]]; then
	echo "test_weapon_bloom_policy: FAIL"
	exit 1
fi

# --- 5. Optional live Surf launch ---
CLIENT="$ROOT/release/idtech3"
if [[ ! -x "$CLIENT" || ! -x "$(command -v xvfb-run 2>/dev/null || true)" ||
	! -f "$ROOT/release/surf/maps/surf_aztec.bsp" ]]; then
	echo "SKIP: live weapon bloom check needs release/idtech3, xvfb-run, and surf_aztec.bsp"
	echo "test_weapon_bloom_policy: PASS (static)"
	exit 0
fi

HOME_DIR="$(mktemp -d)"
LOG="$(mktemp)"
trap 'rm -rf "$HOME_DIR" "$LOG"' EXIT

set +e
timeout 45s xvfb-run -a env LIBGL_ALWAYS_SOFTWARE=1 \
	"$CLIENT" \
	+set fs_basepath "$ROOT/release" \
	+set fs_homepath "$HOME_DIR" \
	+set fs_basegame openarena \
	+set fs_game surf \
	+set com_introplayed 1 \
	+set developer 1 \
	+set r_fullscreen 0 \
	+map surf_aztec \
	+wait 20 \
	+surf_validateTemporalConfig \
	+quit >"$LOG" 2>&1
status=$?
set -e

if [[ "$status" -ne 0 && "$status" -ne 124 ]]; then
	cat "$LOG" >&2
	echo "FAIL: live Surf launch exited with status $status"
	echo "test_weapon_bloom_policy: FAIL"
	exit 1
fi

if grep -q 'weapon composition stage: pre-bloom combined HDR' "$LOG"; then
	pass "live: weapon composited into combined HDR before the single bloom pass"
else
	cat "$LOG" >&2
	echo "FAIL: live launch did not report pre-bloom combined HDR weapon composition"
	echo "test_weapon_bloom_policy: FAIL"
	exit 1
fi
grep -q 'Surf temporal configuration:' "$LOG" || {
	cat "$LOG" >&2
	echo "FAIL: live Surf temporal configuration summary missing"
	echo "test_weapon_bloom_policy: FAIL"
	exit 1
}
echo "test_weapon_bloom_policy: PASS (static + live)"
exit 0
