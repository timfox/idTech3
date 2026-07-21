#!/usr/bin/env bash
# Surf shipping startup contract + optional live Vulkan startup validation.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CANONICAL_CFG="$ROOT/config/surf.cfg"
CANONICAL_AUTOEXEC="$ROOT/config/surf_autoexec.cfg"
INSTALLED_CFG="$ROOT/release/surf/surf.cfg"
INSTALLED_AUTOEXEC="$ROOT/release/surf/autoexec.cfg"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

[[ -f "$CANONICAL_CFG" ]] || fail "missing canonical config/surf.cfg"
[[ -f "$CANONICAL_AUTOEXEC" ]] || fail "missing config/surf_autoexec.cfg"

# Load the real installed release files when present. A clean source-only CI
# checkout validates the canonical files that compile_engine installs there.
if [[ -f "$INSTALLED_CFG" && -f "$INSTALLED_AUTOEXEC" ]]; then
	SURF_CFG="$INSTALLED_CFG"
	SURF_AUTOEXEC="$INSTALLED_AUTOEXEC"
else
	SURF_CFG="$CANONICAL_CFG"
	SURF_AUTOEXEC="$CANONICAL_AUTOEXEC"
	echo "SKIP: installed release/surf config absent; validating canonical package inputs"
fi

python3 - "$SURF_AUTOEXEC" "$SURF_CFG" <<'PY'
import pathlib
import re
import sys

values = {}
setter = re.compile(r'^\s*seta?\s+(\S+)\s+"?([^"\s]+)"?', re.I)

for filename in sys.argv[1:]:
    for raw in pathlib.Path(filename).read_text(encoding="utf-8").splitlines():
        line = raw.split("//", 1)[0].strip()
        match = setter.match(line)
        if match:
            values[match.group(1)] = match.group(2)

required = {
    "r_fbo": "1",
    "r_aaMode": "4",
    "r_taa": "1",
    "r_taaMotionVectors": "1",
    "r_temporalVarianceClip": "1",
    "r_temporalDisocclusion": "1",
    "r_temporalReactiveMask": "1",
    "r_temporalWeaponAfterTaa": "1",
    "r_weaponTemporalMode": "1",
    "r_weaponSsrIsolation": "1",
    "r_weaponBloomMode": "1",
    "r_bloom": "1",
}

errors = []
for name, expected in required.items():
    actual = values.get(name)
    if actual != expected:
        errors.append(f"{name}: expected {expected}, got {actual!r}")

if errors:
    raise SystemExit("Surf effective temporal config inactive:\n  " + "\n  ".join(errors))

print("PASS: Surf release config enables Temporal Weapon Resolve")
for name in required:
    print(f"  {name}={values[name]}")
PY

grep -q 'surf_validateTemporalConfig' "$ROOT/renderers/vulkan/tr_init.c" ||
	fail "developer validation command not registered"
grep -q 'Surf temporal configuration:' "$ROOT/renderers/vulkan/vk_temporal.c" ||
	fail "Surf startup summary missing"
grep -q 'classification enabled without a valid class texture' "$ROOT/renderers/vulkan/vk_temporal.c" ||
	fail "class texture contradiction warning missing"
grep -q 'reactive masking enabled without a reactive target' "$ROOT/renderers/vulkan/vk_temporal.c" ||
	fail "reactive target contradiction warning missing"
grep -q 'temporal weapon resolve enabled without velocity' "$ROOT/renderers/vulkan/vk_temporal.c" ||
	fail "velocity contradiction warning missing"
grep -qE 'seta[[:space:]]+r_taa[[:space:]]+0' "$ROOT/config/gfx_safe.cfg" ||
	fail "gfx_safe.cfg must preserve the intentional non-temporal recovery path"
if [[ -f "$ROOT/release/surf/ghost_safe.cfg" ]]; then
	grep -qE 'set[[:space:]]+r_taa[[:space:]]+0' "$ROOT/release/surf/ghost_safe.cfg" ||
		fail "Surf ghost_safe.cfg must preserve the intentional TAA-off comparison"
fi

if [[ "${SURF_TEMPORAL_SKIP_LIVE:-0}" == "1" ]]; then
	echo "SKIP: live Surf startup disabled by SURF_TEMPORAL_SKIP_LIVE=1"
	exit 0
fi

CLIENT="$ROOT/release/idtech3"
if [[ ! -x "$CLIENT" || ! -x "$(command -v xvfb-run 2>/dev/null || true)" ||
	! -f "$ROOT/release/surf/maps/surf_aztec.bsp" ]]; then
	echo "SKIP: live Surf startup needs release/idtech3, xvfb-run, and surf_aztec.bsp"
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
	fail "live Surf startup exited with status $status"
fi

grep -q 'Surf temporal configuration:' "$LOG" ||
	{ cat "$LOG" >&2; fail "live startup summary not found"; }
grep -q 'TAA: enabled' "$LOG" ||
	{ cat "$LOG" >&2; fail "live Surf launch did not enable TAA"; }
grep -q 'weapon temporal mode: classified shared history' "$LOG" ||
	{ cat "$LOG" >&2; fail "live Surf launch did not select classified history"; }
grep -q 'weapon class mask: available' "$LOG" ||
	{ cat "$LOG" >&2; fail "live class mask unavailable"; }
grep -q 'weapon reactive mask: available' "$LOG" ||
	{ cat "$LOG" >&2; fail "live reactive mask unavailable"; }
grep -q 'weapon MVP velocity: available' "$LOG" ||
	{ cat "$LOG" >&2; fail "live weapon velocity unavailable"; }
grep -q 'previous depth: available (dual R32F history)' "$LOG" ||
	{ cat "$LOG" >&2; fail "live previous-depth history unavailable"; }
grep -q 'weapon composition stage: pre-bloom combined HDR' "$LOG" ||
	{ cat "$LOG" >&2; fail "weapon is not composited before combined bloom"; }
last_result="$(grep 'RESULT:' "$LOG" | tail -n 1)"
[[ "$last_result" == *"RESULT: PASS"* ]] ||
	{ cat "$LOG" >&2; fail "final surf_validateTemporalConfig did not pass: $last_result"; }

echo "PASS: live Surf startup activates Temporal Weapon Resolve"
