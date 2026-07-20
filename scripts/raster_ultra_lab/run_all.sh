#!/usr/bin/env bash
# Raster Ultra 1.11 — aggregate static lab validation.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LAB="$ROOT/scripts/raster_ultra_lab"
fail=0

echo "=== Raster Ultra Lab run_all ==="

bash "$ROOT/scripts/raster_ultra_1_11_check.sh" || fail=1
bash "$LAB/lab_combination_matrix.sh" || fail=1
bash "$LAB/lab_lifecycle_matrix.sh" || fail=1
bash "$LAB/lab_temporal_sequences.sh" || fail=1

# Self-test metrics on a synthetic pair if python3 available
if command -v python3 >/dev/null 2>&1; then
  TMP=$(mktemp -d)
  python3 - <<PY
from pathlib import Path
p = Path("$TMP") / "a.ppm"
w = h = 8
body = bytearray()
for y in range(h):
    for x in range(w):
        body += bytes((x * 28, y * 28, 128))
header = f"P6\n{w} {h}\n255\n".encode("ascii")
p.write_bytes(header + body)
(p.parent / "b.ppm").write_bytes(p.read_bytes())
PY
  if python3 "$LAB/metrics/compare_frame.py" --ref "$TMP/a.ppm" --test "$TMP/b.ppm" \
      --json "$TMP/selftest_compare.json"; then
    echo "OK  metrics self-test (identical frames)"
    cp "$TMP/selftest_compare.json" "$LAB/baselines/selftest_compare.json"
  else
    echo "FAIL metrics self-test"
    fail=1
  fi
  if python3 "$LAB/metrics/detect_artifacts.py" "$TMP/a.ppm" \
      --json "$TMP/selftest_artifacts.json"; then
    echo "OK  artifact self-test"
    cp "$TMP/selftest_artifacts.json" "$LAB/baselines/selftest_artifacts.json"
  else
    echo "FAIL artifact self-test"
    fail=1
  fi
  rm -rf "$TMP"
else
  echo "SKIP python metrics (no python3)"
fi

bash "$LAB/lab_report.sh" "$LAB/baselines/last_report.md" "$LAB/baselines" || true

# Aggregate prior Ultra static gates (evidence pack)
for n in 7 8 9 10; do
  if [[ -x "$ROOT/scripts/raster_ultra_1_${n}_check.sh" ]]; then
    if bash "$ROOT/scripts/raster_ultra_1_${n}_check.sh" >/dev/null; then
      echo "OK  raster_ultra_1_${n}_check"
    else
      echo "FAIL raster_ultra_1_${n}_check"
      fail=1
    fi
  fi
done

if [[ "$fail" -ne 0 ]]; then
  echo "raster_ultra_lab run_all: FAIL"
  exit 1
fi
echo "raster_ultra_lab run_all: PASS"
exit 0
