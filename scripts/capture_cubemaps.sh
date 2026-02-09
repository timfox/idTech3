#!/usr/bin/env bash
#
# Capture parallax-correct cubemaps for a map, based on the cubemap JSON schema.
#
# This helper runs `release/idtech3` with the capture cvars (`r_cubeMapping`, `r_ibl_forceCapture`)
# so Vulkan renders a forced cubemap. The script also mirrors the cubemap JSON into
# `content/cubemaps/<map>/env.json` and the release tree so it gets packaged.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CONTENT_DIR="$ROOT_DIR/content"
RELEASE_ATLAS="$ROOT_DIR/release/atlas"
RELEASE_BIN="$ROOT_DIR/release/idtech3"
PACKAGE_SCRIPT="$ROOT_DIR/scripts/package_content.sh"

usage() {
  echo "Usage: $0 <map-name>"
  echo "Example: $0 demo-full"
}

if [ "$#" -ne 1 ]; then
  usage
  exit 1
fi

MAP_NAME="$1"
MAP_JSON="$CONTENT_DIR/maps/${MAP_NAME}.cubemaps.json"
CONTENT_CUBEMAP_DIR="$CONTENT_DIR/cubemaps/$MAP_NAME"
RELEASE_CUBEMAP_DIR="$RELEASE_ATLAS/cubemaps/$MAP_NAME"

if [ ! -f "$MAP_JSON" ]; then
  echo "[cubemap-capture] No cubemap definition at $MAP_JSON; skipping capture."
  exit 0
fi

if [ ! -x "$RELEASE_BIN" ]; then
  echo "[cubemap-capture] Renderer binary not found at $RELEASE_BIN"
  exit 1
fi

mkdir -p "$CONTENT_CUBEMAP_DIR" "$RELEASE_CUBEMAP_DIR"
cp "$MAP_JSON" "$CONTENT_CUBEMAP_DIR/env.json"
cp "$MAP_JSON" "$RELEASE_CUBEMAP_DIR/env.json"

if [ "${CAPTURE_PACKAGE:-1}" -ne 0 ]; then
  if [ -x "$PACKAGE_SCRIPT" ]; then
    echo "[cubemap-capture] packaging atlas_content.pk3"
    "$PACKAGE_SCRIPT"
  else
    echo "[cubemap-capture] warning: package script missing/executable; atlas_content.pk3 might be stale"
  fi
else
  echo "[cubemap-capture] skipping packaging (CAPTURE_PACKAGE=0)"
fi

echo "[cubemap-capture] Running forced capture for $MAP_NAME (r_cubeMapping=1, r_ibl_forceCapture=1)"
CMD=(
  "$RELEASE_BIN"
  "+set fs_game atlas"
  "+set fs_basegame atlas"
  "+set r_cubeMapping 1"
  "+set r_ibl_forceCapture 1"
  "+set r_cubeMappingSize 512"
  "+set r_cubeMappingFov 90"
  "+map $MAP_NAME"
  "+quit"
)

printf '  %s\n' "${CMD[@]}"
"${CMD[@]}"

echo "[cubemap-capture] Capture command finished for $MAP_NAME"
