#!/usr/bin/env bash
# Raster Ultra 1.11 — Markdown report from JSON metric/artifact results.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-$ROOT/scripts/raster_ultra_lab/baselines/last_report.md}"
JSON_DIR="${2:-$ROOT/scripts/raster_ultra_lab/baselines}"

mkdir -p "$(dirname "$OUT")"
REV="$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
DATE="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

{
  echo "# Raster Ultra 1.11 Reference Lab Report"
  echo
  echo "- generated: \`$DATE\`"
  echo "- git: \`$REV\`"
  echo "- host: \`$(uname -srm)\`"
  echo "- GPU/driver: \`${GPU_NAME:-unknown}\`"
  echo
  echo "## Reproduction"
  echo
  echo '```'
  echo "exec modern_raster_reference.cfg"
  echo "exec vulkan_overlay_raster_ultra_1_11_reference_lab.cfg"
  echo "vid_restart"
  echo "reference_lab_status"
  echo "reference_lab_scenes"
  echo "./scripts/raster_ultra_1_11_check.sh"
  echo "./scripts/raster_ultra_lab/run_all.sh"
  echo '```'
  echo
  echo "## Results"
  echo
  shopt -s nullglob
  found=0
  for j in "$JSON_DIR"/*.json; do
    [[ "$(basename "$j")" == "thresholds.json" ]] && continue
    found=1
    echo "### $(basename "$j")"
    echo
    echo '```json'
    cat "$j"
    echo '```'
    echo
  done
  if [[ "$found" -eq 0 ]]; then
    echo "_No metric JSON yet. Run metrics/compare_frame.py or detect_artifacts.py._"
    echo
  fi
  echo "## CI tiers"
  echo
  echo "| Tier | Scope |"
  echo "|------|-------|"
  echo "| Per-commit | \`raster_ultra_1_11_check.sh\` + lab static matrices |"
  echo "| Pre-merge | material/shadow/GI/reflection/water/AA + lifecycle |"
  echo "| Nightly | pairwise Ultra overlays + soak |"
  echo "| Release | multi-GPU SDR/HDR long soak |"
  echo
  echo "## Blind spots"
  echo
  echo "- External \`rtest_*.bsp\` pack may be required for Tier B GPU captures"
  echo "- PQ OETF / full EXR master capture remain iterative"
  echo "- Temporal metrics need multi-frame capture sequences"
} > "$OUT"

echo "Wrote $OUT"
