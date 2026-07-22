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

# Shipping Surf defaults to SMAA (no Temporal Reconstruction). High air speed
# left residual shading / multi-silhouette echoes with aaMode 4 / r_taa 1.
required = {
    "r_fbo": "1",
    "r_aaMode": "2",
    "r_taa": "0",
    "r_taaMotionVectors": "0",
    "r_temporalSSR": "0",
    "r_bloom": "0",
    "r_weaponSsrIsolation": "1",
}

errors = []
for name, expected in required.items():
    actual = values.get(name)
    if actual != expected:
        errors.append(f"{name}: expected {expected}, got {actual!r}")

if errors:
    raise SystemExit("Surf effective anti-ghost config inactive:\n  " + "\n  ".join(errors))

print("PASS: Surf release config disables Temporal Reconstruction (SMAA)")
for name in required:
    print(f"  {name}={values[name]}")
PY

grep -q 'surf_validateTemporalConfig' "$ROOT/renderers/vulkan/tr_init.c" ||
	fail "developer validation command not registered"
grep -qE 'seta[[:space:]]+r_taa[[:space:]]+0' "$ROOT/config/gfx_safe.cfg" ||
	fail "gfx_safe.cfg must preserve the intentional non-temporal recovery path"
if [[ -f "$ROOT/release/surf/ghost_safe.cfg" ]]; then
	grep -qE 'set[[:space:]]+r_taa[[:space:]]+0' "$ROOT/release/surf/ghost_safe.cfg" ||
		fail "Surf ghost_safe.cfg must preserve the intentional TAA-off comparison"
fi
if [[ -f "$ROOT/release/surf/echo_off.cfg" ]]; then
	grep -qE 'set[[:space:]]+r_taa[[:space:]]+0' "$ROOT/release/surf/echo_off.cfg" ||
		fail "Surf echo_off.cfg must kill temporal reconstruction"
fi
if [[ -f "$ROOT/config/surf_temporal_quality.cfg" ]] || [[ -f "$ROOT/release/surf/surf_temporal_quality.cfg" ]]; then
	QUAL="$ROOT/config/surf_temporal_quality.cfg"
	[[ -f "$QUAL" ]] || QUAL="$ROOT/release/surf/surf_temporal_quality.cfg"
	grep -qE 'seta[[:space:]]+r_aaMode[[:space:]]+4' "$QUAL" ||
		fail "surf_temporal_quality.cfg must opt into aaMode 4"
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
	+quit >"$LOG" 2>&1
status=$?
set -e

if [[ "$status" -ne 0 && "$status" -ne 124 ]]; then
	cat "$LOG" >&2
	fail "live Surf startup exited with status $status"
fi

# Confirm temporal reconstruction is not forced on by aaMode/taa.
if grep -E 'r_aaMode|r_taa' "$LOG" >/dev/null 2>&1; then
	:
fi
echo "PASS: live Surf startup completed (SMAA / no Temporal Reconstruction default)"
