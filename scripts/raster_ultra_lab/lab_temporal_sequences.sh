#!/usr/bin/env bash
# Raster Ultra 1.11 — temporal stability sequence catalog (static + optional GPU).
# No new rendering techniques. Sequences pin deterministic lab mode.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
fail=0

echo "=== Raster Ultra 1.11 temporal sequences ==="

SEQUENCES=(
  "fast_camera_pan"
  "slow_camera_pan"
  "camera_cut"
  "disocclusion"
  "moving_weapon"
  "moving_reflective_object"
  "moving_shadow_caster"
  "particles"
  "water"
  "foliage"
  "emissive_flicker"
  "weather_transition"
  "exposure_transition"
  "portal_view"
)

DOC="$ROOT/docs/RASTER_ULTRA_1.11.md"
if [[ ! -f "$DOC" ]]; then
  echo "FAIL missing docs/RASTER_ULTRA_1.11.md"
  fail=1
else
  for s in "${SEQUENCES[@]}"; do
    # normalize underscore to space for doc match (or accept underscore)
    spaced="${s//_/ }"
    if grep -qiE "$s|$spaced" "$DOC"; then
      echo "OK  sequence documented: $s"
    else
      echo "FAIL sequence not documented: $s"
      fail=1
    fi
  done
fi

# Thresholds must define temporal tolerances
TH="$ROOT/scripts/raster_ultra_lab/baselines/thresholds.json"
if [[ -f "$TH" ]]; then
  grep -q 'variance_max' "$TH" && grep -q 'ghost_trail_frames_max' "$TH" && \
    grep -q 'disocclusion_recovery_frames_max' "$TH" && \
    echo "OK  temporal thresholds present" || {
      echo "FAIL temporal thresholds incomplete"
      fail=1
    }
else
  echo "FAIL missing thresholds.json"
  fail=1
fi

# Lab must pin temporal jitter for deterministic captures
if grep -q 'freezeTemporalJitter' "$ROOT/renderers/vulkan/vk_reference_lab.c"; then
  echo "OK  lab freezes temporal jitter"
else
  echo "FAIL lab missing freezeTemporalJitter"
  fail=1
fi

if [[ "${RASTER_ULTRA_LAB_GPU:-0}" == "1" ]]; then
  echo "NOTE GPU temporal soak is runner-owned; capture N frames then:"
  echo "  python3 scripts/raster_ultra_lab/metrics/compare_frame.py --ref frame000.ppm --test frameNNN.ppm"
else
  echo "SKIP GPU temporal capture (set RASTER_ULTRA_LAB_GPU=1)"
fi

if [[ "$fail" -ne 0 ]]; then
  echo "lab_temporal_sequences: FAIL"
  exit 1
fi
echo "lab_temporal_sequences: PASS"
exit 0
