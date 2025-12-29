#!/usr/bin/env bash
set -euo pipefail

# CI script to verify that a map auto-loads at startup using AUTO_MAP_NAME or
# a known OA map present in pak0.pk3. Logs are produced for triage.

MAP_DEFAULT="oa_dm1"
RELEASE_BASE="${RELEASE_BASE:-/home/tim/Desktop/idtech3/release/base}"
PK3_PATH="$RELEASE_BASE/pak0.pk3"

MAP_LIST=""
if command -v unzip >/dev/null && [ -f "$PK3_PATH" ]; then
  MAP_LIST=$(unzip -l "$PK3_PATH" 2>/dev/null | awk '/maps\/.*\.bsp/ {gsub(/\\r/,"",$0); sub(/.*maps\\//,"",$0); sub(/\.bsp$/,"",$0); print $0}' | sort -u)
fi

SELECTED_MAP="$MAP_DEFAULT"
if [ -n "$MAP_LIST" ]; then
  for m in oa_dm1 oa_dm2 oa_dm3; do
    if echo "$MAP_LIST" | grep -qi "^${m}$"; then
      SELECTED_MAP="$m"
      break
    fi
  done
fi

echo "CI: auto-map startup test using map: ${SELECTED_MAP}"

export AUTO_MAP_NAME="${SELECTED_MAP}"
VK_METRICS_ENABLED=1 VK_VERBOSE_PIPELINE_LOGS=1 AUTO_MAP_NAME="${SELECTED_MAP}" \
  ./release/idtech3.x86_64 +set fs_game mymod +set cl_renderer vulkan \
  > ci_map_autoload.log 2>&1

grep -E "AUTO_MAP_STARTUP|Auto-loading|devmap|VK_CreateSurface|VK_METRICS" ci_map_autoload.log || true

