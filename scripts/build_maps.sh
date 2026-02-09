#!/usr/bin/env bash
#
# Compile id Tech 3 maps through every stage that the engine expects. The script
# keeps the `map-tool` compiler in the tree, runs the stages sequentially, copies
# the final `.bsp/.aas/.info` artifacts into `content/maps/` (and into the release
# atlas content tree), and keeps `content/manifest.txt` in sync so packaging can
# pick up the new assets.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CONTENT_DIR="$ROOT_DIR/content"
MAP_DIR="$CONTENT_DIR/maps"
MANIFEST_FILE="$CONTENT_DIR/manifest.txt"
RELEASE_ATLAS="$ROOT_DIR/release/atlas"
RELEASE_MAPS="$RELEASE_ATLAS/content/maps"
MAP_TOOL="$ROOT_DIR/src/tools/id3map/build/map-tool"
MAP_TOOL_BUILD_SCRIPT="$ROOT_DIR/src/tools/id3map/build-map-tool.sh"
CAPTURE_SCRIPT="$ROOT_DIR/scripts/capture_cubemaps.sh"

STAGES=(meta vis light bounce aas info)
MAP_TOOL_ARGS=(-game atlas -fs_game atlas -fs_basepath "$RELEASE_ATLAS")

usage() {
  cat <<'EOF'
Usage: ./scripts/build_maps.sh <map-name> [more maps...]

Drop .map sources into content/maps/ and run this script by passing the base name
(without extension). The pipeline runs -meta, -vis, -light, -bounce, -aas, and -info,
copies the products into both content/maps/ and release/atlas/content/maps/, and
updates content/manifest.txt so atlas_content.pk3 can be repackaged with the new
assets.
EOF
}

ensure_map_tool() {
  if [ ! -x "$MAP_TOOL" ]; then
    echo "[map-build] map-tool not found, running ${MAP_TOOL_BUILD_SCRIPT}"
    "$MAP_TOOL_BUILD_SCRIPT"
  fi
}

add_manifest_entry() {
  local entry="$1"
  mkdir -p "$(dirname "$MANIFEST_FILE")"
  touch "$MANIFEST_FILE"
  if grep -Fxq "$entry" "$MANIFEST_FILE"; then
    return
  fi
  echo "$entry" >>"$MANIFEST_FILE"
}

if [ "$#" -lt 1 ]; then
  usage
  exit 1
fi

ensure_map_tool
mkdir -p "$MAP_DIR" "$RELEASE_MAPS"
touch "$MANIFEST_FILE"

for target in "$@"; do
  map_name="${target%.map}"
  map_path="$MAP_DIR/$map_name.map"

  if [ ! -f "$map_path" ]; then
    echo "[map-build] error: map source not found ($map_path)"
    exit 1
  fi

  echo "[map-build] starting pipeline for $map_name"

  pushd "$MAP_DIR" >/dev/null
  for stage in "${STAGES[@]}"; do
    echo "[map-build]  - stage: $stage"
    "$MAP_TOOL" "${MAP_TOOL_ARGS[@]}" "-$stage" "$map_name.map"
  done
  popd >/dev/null

  for ext in bsp aas info; do
    artifact="$MAP_DIR/$map_name.$ext"
    if [ ! -f "$artifact" ]; then
      echo "[map-build] error: expected output $artifact missing"
      exit 1
    fi
    cp -u "$artifact" "$RELEASE_MAPS/"
    add_manifest_entry "maps/$map_name.$ext"
  done

  if [ -x "$CAPTURE_SCRIPT" ]; then
    "$CAPTURE_SCRIPT" "$map_name"
  fi

  echo "[map-build] completed $map_name -> content/maps/{.bsp,.aas,.info}"
done

echo "[map-build] manifest updated at $MANIFEST_FILE"
