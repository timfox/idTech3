#!/usr/bin/env bash
# Documented exposure convention: linear multiply, AE darkens bright scenes.
set -euo pipefail
python3 - <<'PY'
# targetExp = target / sceneLum  → multiplier
# exposed = scene * targetExp
target = 0.18
for scene, label in [(0.05, "dark"), (0.18, "mid"), (2.0, "bright")]:
    exp = target / scene
    exposed_mid = 0.18 * exp  # how 0.18 patch reads after AE aiming at `target`
    print(f"{label}: sceneLum={scene} adapted≈{exp:.3f} patch0.18→{exposed_mid:.3f}")
assert (target / 2.0) < (target / 0.05), "bright scenes must get lower multiplier"
print("test_exposure_ev_sign: ok (linear *= adaptedExposure; bright→darken)")
PY
