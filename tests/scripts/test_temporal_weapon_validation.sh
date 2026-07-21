#!/usr/bin/env bash
# Validation-manifest contract; --capture optionally launches deterministic static cases.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
MANIFEST="$ROOT/tests/data/temporal_weapon_validation.json"
CAPTURE=0
[[ "${1:-}" == "--capture" ]] && CAPTURE=1

python3 - "$ROOT" "$MANIFEST" <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
manifest = json.loads(pathlib.Path(sys.argv[2]).read_text(encoding="utf-8"))
required_buffers = {
    "final_hdr", "final_ldr", "class_current", "class_previous",
    "weapon_mvp_velocity", "velocity_merged", "depth_current", "depth_previous",
    "depth_rejection", "class_rejection", "reactive_mask", "history_weight",
    "weapon_history_validity",
}
assert set(manifest["requiredBuffers"]) == required_buffers
cases = manifest["cases"]
assert {case["set"].get("r_weaponTemporalMode") for case in cases} >= {0, 1, 2}
assert {case["set"].get("r_renderScale") for case in cases if "r_renderScale" in case["set"]} >= {0.5, 0.75, 1.0}
assert {case["set"].get("r_firstPersonFov") for case in cases if "r_firstPersonFov" in case["set"]} >= {55, 65, 80}
assert any(case.get("event", "").startswith("resize_") for case in cases)
assert any(case.get("event") == "weapon_switch_during_recoil" for case in cases)
assert any(case.get("event") == "teleport" for case in cases)
assert any(case["set"].get("r_temporalDropClassDescriptor") == 1 for case in cases)
assert any(case["set"].get("r_bloom") == 0 for case in cases)
assert any(case["set"].get("r_bloom") == 1 for case in cases)

taa = (root / "renderers/vulkan/shaders/glsl/taa.frag").read_text(encoding="utf-8")
weapon = (root / "renderers/vulkan/shaders/glsl/weapon_taa.frag").read_text(encoding="utf-8")
frame_end = (root / "renderers/vulkan/vk_frame_end.c").read_text(encoding="utf-8")
backend = (root / "renderers/vulkan/tr_backend.c").read_text(encoding="utf-8")
required_source = [
    (taa, "previousDepthTex"),
    (taa, "linearizeReversedDepth"),
    (weapon, "previousWeaponHistory"),
    (weapon, "currentClassTex"),
    (weapon, "temporalDebugParams"),
    (frame_end, "TemporalUnclassifiedR8"),
    (frame_end, "forcedHistoryRejectFrames"),
    (backend, "RB_ResolveIndependentWeaponHistory"),
    (backend, "weaponHistoryFrameId"),
]
for text, token in required_source:
    assert token in text, f"missing validation token: {token}"

surf = (root / "config/surf.cfg").read_text(encoding="utf-8")
for line in ("seta r_taa 1", "seta r_weaponTemporalMode 1", "seta r_weaponBloomMode 1"):
    assert line in surf, f"missing Surf default: {line}"
print(f"PASS: temporal weapon validation manifest ({len(cases)} cases, {len(required_buffers)} buffers)")
PY

if [[ "$CAPTURE" -eq 0 ]]; then
	echo "SKIP: live captures require --capture"
	exit 0
fi

CLIENT="$ROOT/release/idtech3"
if [[ ! -x "$CLIENT" || ! -x "$(command -v xvfb-run 2>/dev/null || true)" ||
	! -f "$ROOT/release/surf/maps/surf_aztec.bsp" ]]; then
	echo "SKIP: capture requires release/idtech3, xvfb-run, and surf_aztec.bsp"
	exit 0
fi

OUT="${TEMPORAL_WEAPON_CAPTURE_DIR:-$ROOT/artifacts/temporal_weapon}"
HOME_DIR="$(mktemp -d)"
trap 'rm -rf "$HOME_DIR"' EXIT
mkdir -p "$OUT" "$HOME_DIR/surf"

python3 - "$MANIFEST" "$HOME_DIR/surf/temporal_validation.cfg" <<'PY'
import json
import pathlib
import sys

manifest = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
lines = ["seta com_nativeLibraryExtractPk3 0", "exec surf.cfg"]
for case in manifest["cases"]:
    if case.get("event"):
        continue
    lines.append(f'echo "TEMPORAL_CASE_BEGIN {case["id"]}"')
    for name, value in case["set"].items():
        lines.append(f"set {name} {value}")
    lines += [
        "wait 5",
        "r_dumpTemporalState",
        "r_printViewmodelProjection",
    ]
    for mode in range(16, 34):
        lines += [f"set r_temporalDebug {mode}", "wait 2", f"screenshot {case['id']}_debug_{mode:02d}"]
    lines += ["set r_temporalDebug 0", f"screenshot {case['id']}_final_ldr", f'echo "TEMPORAL_CASE_END {case["id"]}"']
lines += ["quit"]
pathlib.Path(sys.argv[2]).write_text("\n".join(lines) + "\n", encoding="utf-8")
PY

xvfb-run -a env LIBGL_ALWAYS_SOFTWARE=1 \
	"$CLIENT" \
	+set fs_basepath "$ROOT/release" \
	+set fs_homepath "$HOME_DIR" \
	+set fs_basegame openarena \
	+set fs_game surf \
	+set r_fullscreen 0 \
	+set developer 1 \
	+map surf_aztec \
	+wait 20 \
	+exec temporal_validation.cfg \
	>"$OUT/runtime.log" 2>&1

cp -a "$HOME_DIR/surf/screenshots/." "$OUT/" 2>/dev/null || true
grep -q 'TEMPORAL_CASE_END' "$OUT/runtime.log"
echo "PASS: temporal weapon static captures written to $OUT"
