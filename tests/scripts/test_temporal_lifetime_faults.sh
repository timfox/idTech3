#!/usr/bin/env bash
# Temporal lifetime / fault-injection gate for Architecture B.
# Static wiring checks always run; the live lifecycle exercise (TAA toggles,
# weapon-temporal-mode toggles, vid_restart) runs only when
# TEMPORAL_LIFETIME_LIVE=1 and the Surf binaries exist.
# See docs/TEMPORAL_RESOURCE_OWNERSHIP.md and docs/TEMPORAL_WEAPON_VALIDATION.md.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail=0

pass() { echo "PASS: $*"; }
failmsg() { echo "FAIL: $*"; fail=1; }

VK_H="$ROOT/renderers/vulkan/vk.h"
VK_TEMPORAL_H="$ROOT/renderers/vulkan/vk_temporal.h"
VK_TEMPORAL_C="$ROOT/renderers/vulkan/vk_temporal.c"
TR_INIT="$ROOT/renderers/vulkan/tr_init.c"
MANIFEST="$ROOT/tests/data/temporal_weapon_validation.json"

for f in "$VK_H" "$VK_TEMPORAL_H" "$VK_TEMPORAL_C" "$TR_INIT" "$MANIFEST"; do
	[[ -f "$f" ]] || failmsg "missing required file: $f"
done
[[ "$fail" -eq 0 ]] || { echo "test_temporal_lifetime_faults: FAIL"; exit 1; }

# --- Diagnostic command ---
if grep -q '"r_dumpTemporalState"' "$TR_INIT"; then
	pass "r_dumpTemporalState command registered"
else
	failmsg "r_dumpTemporalState command registration missing from tr_init.c"
fi

# --- Per-history frame ID fields ---
for field in taaHistoryFrameId prevDepthFrameId classFrameId weaponHistoryFrameId weaponDepthFrameId; do
	if grep -q "$field\[2\]" "$VK_H"; then
		pass "frame ID field $field[2] present"
	else
		failmsg "frame ID field $field[2] missing from vk.h"
	fi
done

# --- Independent weapon invalidation ---
if grep -q 'vk_reset_weapon_history' "$VK_TEMPORAL_H"; then
	pass "vk_reset_weapon_history declared"
else
	failmsg "vk_reset_weapon_history declaration missing from vk_temporal.h"
fi
if grep -rq 'vk_reset_weapon_history()' "$ROOT/renderers/vulkan/tr_backend.c" \
	"$ROOT/renderers/vulkan/vk_frame_end.c" "$ROOT/renderers/vulkan/vk_frame_submit.c"; then
	pass "vk_reset_weapon_history invoked on weapon lifecycle transitions"
else
	failmsg "vk_reset_weapon_history has no lifecycle callers"
fi

# --- Shared reset reasons: resize / world / swapchain ---
for reason in VK_TEMPORAL_RESET_RENDER_SIZE_CHANGE VK_TEMPORAL_RESET_WORLD_CHANGE VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE; do
	if grep -q "$reason" "$VK_TEMPORAL_H" && grep -q "$reason" "$VK_TEMPORAL_C"; then
		pass "reset reason $reason declared and applied"
	else
		failmsg "reset reason $reason missing from vk_temporal.h / vk_temporal.c"
	fi
done

# --- Manifest lifecycle coverage ---
for event in resize weapon_switch teleport toggle_taa; do
	if grep -q "$event" "$MANIFEST"; then
		pass "validation manifest lists lifecycle event: $event"
	else
		failmsg "temporal_weapon_validation.json must list lifecycle event: $event"
	fi
done

if [[ "$fail" -ne 0 ]]; then
	echo "test_temporal_lifetime_faults: FAIL"
	exit 1
fi

# --- Live lifecycle exercise (opt-in) ---
# Alt-tab / minimize / focus-loss faults need an interactive window manager and
# real input; a headless xvfb run cannot synthesize them. The live path below
# covers what a console launch can honestly drive: TAA toggles, weapon temporal
# mode toggles, one vid_restart, and the r_dumpTemporalState ownership dump.
if [[ "${TEMPORAL_LIFETIME_LIVE:-0}" != "1" ]]; then
	echo "SKIP: live lifetime checks (alt-tab/minimize need an interactive host;"
	echo "      set TEMPORAL_LIFETIME_LIVE=1 to run the scripted toggle/restart pass)"
	echo "test_temporal_lifetime_faults: PASS (static)"
	exit 0
fi

CLIENT="$ROOT/release/idtech3"
if [[ ! -x "$CLIENT" || ! -x "$(command -v xvfb-run 2>/dev/null || true)" ||
	! -f "$ROOT/release/surf/maps/surf_aztec.bsp" ]]; then
	echo "SKIP: TEMPORAL_LIFETIME_LIVE=1 but release/idtech3, xvfb-run, or surf_aztec.bsp missing"
	echo "test_temporal_lifetime_faults: PASS (static)"
	exit 0
fi

HOME_DIR="$(mktemp -d)"
LOG="$(mktemp)"
trap 'rm -rf "$HOME_DIR" "$LOG"' EXIT

set +e
timeout 90s xvfb-run -a env LIBGL_ALWAYS_SOFTWARE=1 \
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
	+r_dumpTemporalState \
	+set r_taa 0 +wait 5 +set r_taa 1 +wait 5 \
	+set r_weaponTemporalMode 0 +wait 5 \
	+set r_weaponTemporalMode 1 +wait 5 \
	+set r_weaponTemporalMode 2 +wait 5 \
	+vid_restart +wait 20 \
	+r_dumpTemporalState \
	+quit >"$LOG" 2>&1
status=$?
set -e

if [[ "$status" -ne 0 && "$status" -ne 124 ]]; then
	cat "$LOG" >&2
	echo "FAIL: live lifecycle launch exited with status $status"
	echo "test_temporal_lifetime_faults: FAIL"
	exit 1
fi

live_fail=0
grep -q 'Temporal Ownership Status' "$LOG" || {
	echo "FAIL: r_dumpTemporalState ownership dump not found"; live_fail=1; }
# Independent world vs weapon validity fields in the same dump line.
grep -qE 'validity  : color=[01] depth=[01] class=[01] velocity=[01] weapon=[01]' "$LOG" || {
	echo "FAIL: dump must show independent world (color/depth/class) and weapon validity bits"; live_fail=1; }
grep -qE 'frame IDs : color=\{[0-9]+,[0-9]+\}.*weapon=\{[0-9]+,[0-9]+\}' "$LOG" || {
	echo "FAIL: dump must show per-history frame IDs including weapon history"; live_fail=1; }
if [[ "$live_fail" -ne 0 ]]; then
	cat "$LOG" >&2
	echo "test_temporal_lifetime_faults: FAIL"
	exit 1
fi

echo "test_temporal_lifetime_faults: PASS (static + live)"
exit 0
