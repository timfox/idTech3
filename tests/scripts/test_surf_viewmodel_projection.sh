#!/usr/bin/env bash
# Surf viewmodel projection: packaged config and live effective matrix contract.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CANONICAL="$ROOT/config/surf.cfg"
INSTALLED="$ROOT/release/surf/surf.cfg"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

[[ -f "$CANONICAL" ]] || fail "missing config/surf.cfg"
CFG="$CANONICAL"
if [[ -f "$INSTALLED" ]]; then
	CFG="$INSTALLED"
fi

python3 - "$CFG" <<'PY'
import pathlib
import re
import sys

values = {}
setter = re.compile(r'^\s*seta?\s+(\S+)\s+"?([^"\s]+)"?', re.I)
for raw in pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").splitlines():
    match = setter.match(raw.split("//", 1)[0].strip())
    if match:
        values[match.group(1)] = match.group(2)

required = {
    "cg_fov": "90",
    "r_firstPersonFovEnabled": "1",
    "r_firstPersonFov": "65",
    "r_firstPersonScaleEnabled": "1",
    "r_firstPersonScale": "1.0",
    "r_firstPersonZNear": "4",
}
errors = [
    f"{name}: expected {expected}, got {values.get(name)!r}"
    for name, expected in required.items()
    if values.get(name) != expected
]
if "FovEnabled" in values:
    errors.append("obsolete unscoped FovEnabled alias is present")
if errors:
    raise SystemExit("Surf projection config mismatch:\n  " + "\n  ".join(errors))
print("PASS: Surf projection config", " ".join(f"{k}={v}" for k, v in required.items()))
PY

grep -q 'r_printViewmodelProjection' "$ROOT/renderers/vulkan/tr_init.c" ||
	fail "r_printViewmodelProjection is not registered"
grep -q 'projection = backEnd.useFirstPersonProjection' "$ROOT/renderers/vulkan/vk_view_state.c" ||
	fail "weapon velocity does not select the effective first-person projection"
grep -q 'migrated stale viewmodel projection' "$ROOT/renderers/vulkan/tr_init.c" ||
	fail "stale Surf projection migration warning missing"

for cfg in "$ROOT/release/surf/rt_gun_on.cfg" "$ROOT/release/surf/rt_gun_off.cfg"; do
	if [[ -f "$cfg" ]] && grep -q 'r_firstPerson' "$cfg"; then
		fail "$(basename "$cfg") still overrides the authoritative projection"
	fi
done

if [[ "${SURF_PROJECTION_SKIP_LIVE:-0}" == "1" ]]; then
	echo "SKIP: live projection test disabled"
	exit 0
fi

CLIENT="$ROOT/release/idtech3"
if [[ ! -x "$CLIENT" || ! -x "$(command -v xvfb-run 2>/dev/null || true)" ||
	! -f "$ROOT/release/surf/maps/surf_aztec.bsp" ]]; then
	echo "SKIP: live projection test needs release/idtech3, xvfb-run, and surf_aztec.bsp"
	exit 0
fi

HOME_DIR="$(mktemp -d)"
LOG="$(mktemp)"
trap 'rm -rf "$HOME_DIR" "$LOG"' EXIT
mkdir -p "$HOME_DIR/surf"
cat >"$HOME_DIR/surf/autoexec.cfg" <<'CFG'
// Legacy archive fixture: renderer startup must migrate and warn.
seta FovEnabled 0
seta r_firstPersonFovEnabled 0
seta r_firstPersonFov 90
seta r_firstPersonZNear 0.125
seta com_nativeLibraryExtractPk3 0
CFG

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
	+r_printViewmodelProjection \
	+quit >"$LOG" 2>&1
status=$?
set -e

if [[ "$status" -ne 0 && "$status" -ne 124 ]]; then
	cat "$LOG" >&2
	fail "live projection startup exited with status $status"
fi

grep -q 'effective weapon FOV  : 65.000 deg horizontal' "$LOG" ||
	{ cat "$LOG" >&2; fail "effective weapon FOV is not 65 degrees"; }
grep -q 'effective world FOV   : 90.000 x 58.733 deg' "$LOG" ||
	{ cat "$LOG" >&2; fail "effective Surf world FOV mismatch"; }
grep -q 'aspect-adjusted FOV   : 39.444 deg vertical' "$LOG" ||
	{ cat "$LOG" >&2; fail "viewmodel aspect compensation mismatch"; }
grep -q 'z-near / z-far        : 4.000 /' "$LOG" ||
	{ cat "$LOG" >&2; fail "effective weapon z-near is not 4"; }
grep -q 'projection mode       : custom horizontal weapon FOV' "$LOG" ||
	{ cat "$LOG" >&2; fail "custom viewmodel projection is inactive"; }
grep -q 'reversed-Z state      : enabled' "$LOG" ||
	{ cat "$LOG" >&2; fail "Vulkan reverse-Z state missing"; }
grep -q 'depth-range remap     : DEPTH_RANGE_WEAPON \[0.600, 1.000\]' "$LOG" ||
	{ cat "$LOG" >&2; fail "weapon depth remap mismatch"; }
grep -q 'matrix provenance     : current and velocity history use the same effective weapon projection' "$LOG" ||
	{ cat "$LOG" >&2; fail "weapon velocity projection provenance missing"; }
grep -q '\[Surf\] migrated stale viewmodel projection' "$LOG" ||
	{ cat "$LOG" >&2; fail "stale projection was not migrated with a warning"; }
grep -q '\[Surf\] obsolete FovEnabled="0" is ignored' "$LOG" ||
	{ cat "$LOG" >&2; fail "obsolete FovEnabled alias was not reported"; }

echo "PASS: live Surf viewmodel projection matches shipping config"
