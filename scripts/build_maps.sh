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
PACKAGE_SCRIPT="$ROOT_DIR/scripts/package_atlas_content.sh"
DDC_SCRIPT="$ROOT_DIR/scripts/asset_ddc.sh"

STAGES=(meta vis light bounce aas info)
MAP_TOOL_ARGS=(-game atlas -fs_game atlas -fs_basepath "$RELEASE_ATLAS")

usage() {
  cat <<'EOF'
Usage: ./scripts/build_maps.sh <map-name> [more maps...]

Drop .map sources into content/maps/ and run this script by passing the base name
(without extension). The pipeline runs -meta, -vis, -light, -bounce, -aas, and -info,
copies the products into both content/maps/ and release/atlas/content/maps/, keeps
content/manifest.txt in sync, optionally repacks atlas_content.pk3, and finally
captures cubemaps for each scene (based on the cubemap JSON definitions).

Set ATLAS_PACKAGE=0 to skip repackaging atlas_content.pk3.
Set ATLAS_CAPTURE=0 to skip the cubemap capture step.
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

built_maps=()
for target in "$@"; do
  map_name="${target%.map}"
  map_path="$MAP_DIR/$map_name.map"

  if [ ! -f "$map_path" ]; then
    echo "[map-build] error: map source not found ($map_path)"
    exit 1
  fi

  echo "[map-build] starting pipeline for $map_name"

  skip_pipeline=0
  if [ "${FORCE_MAP_BUILD:-0}" -eq 0 ] && [ -x "$DDC_SCRIPT" ]; then
    if "$DDC_SCRIPT" needs-rebuild "$map_name"; then
      echo "[map-build] DDC reports $map_name needs rebuild"
    else
      rc=$?
      if [ "$rc" -eq 1 ]; then
        echo "[map-build] skipping $map_name (cache up-to-date)"
        skip_pipeline=1
      else
        echo "[map-build] DDC check for $map_name failed (rc=$rc), forcing rebuild"
      fi
    fi
  fi

  if [ "$skip_pipeline" -eq 0 ]; then
    pushd "$MAP_DIR" >/dev/null
    for stage in "${STAGES[@]}"; do
      echo "[map-build]  - stage: $stage"
      "$MAP_TOOL" "${MAP_TOOL_ARGS[@]}" "-$stage" "$map_name.map"
    done
    popd >/dev/null
  else
    echo "[map-build] using cached outputs for $map_name"
  fi

  for ext in bsp aas info; do
    artifact="$MAP_DIR/$map_name.$ext"
    if [ ! -f "$artifact" ]; then
      echo "[map-build] error: expected output $artifact missing"
      exit 1
    fi
    cp -u "$artifact" "$RELEASE_MAPS/"
    add_manifest_entry "maps/$map_name.$ext"
  done

  if [ -x "$DDC_SCRIPT" ] && [ "$skip_pipeline" -eq 0 ]; then
    "$DDC_SCRIPT" update "$map_name" || true
  fi

  if [ "$skip_pipeline" -eq 0 ]; then
    built_maps+=("$map_name")
    echo "[map-build] completed $map_name -> content/maps/{.bsp,.aas,.info}"
  else
    echo "[map-build] cached artifacts preserved for $map_name"
  fi
done

echo "[map-build] manifest updated at $MANIFEST_FILE"

if [ "${ATLAS_PACKAGE:-1}" -ne 0 ]; then
  if [ -x "$PACKAGE_SCRIPT" ]; then
    echo "[map-build] packaging atlas_content.pk3"
    "$PACKAGE_SCRIPT"
  else
    echo "[map-build] warning: package script missing/executable; atlas_content.pk3 not updated"
  fi
else
  echo "[map-build] skipping atlas_content.pk3 packaging (ATLAS_PACKAGE=0)"
fi

if [ "${ATLAS_CAPTURE:-1}" -ne 0 ] && [ "${#built_maps[@]}" -gt 0 ]; then
  if [ ! -x "$CAPTURE_SCRIPT" ]; then
    echo "[map-build] warning: capture script missing/executable; skip cubemap capture"
  else
    for map_name in "${built_maps[@]}"; do
      "$CAPTURE_SCRIPT" "$map_name"
    done
  fi
else
  echo "[map-build] skipping cubemap capture (ATLAS_CAPTURE=0 or no maps built)"
fi
