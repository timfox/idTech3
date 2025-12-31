#!/usr/bin/env bash
set -euo pipefail

# Build a demo_content pak layout from ./mods/demo (relative to repo root)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
REPO_ROOT="$SCRIPT_DIR/.."
MODS_DIR="$REPO_ROOT/mods/demo"
DEMO_BASE="$MODS_DIR"
MAPS_OUT_DIR="$DEMO_BASE/maps"
TEXTURES_OUT_DIR="$DEMO_BASE/textures"

mkdir -p "$MAPS_OUT_DIR" "$TEXTURES_OUT_DIR"

# Collect OA maps
MAP_FILES=(oa_dm1.bsp oa_dm2.bsp oa_dm3.bsp)
for f in "${MAP_FILES[@]}"; do
  if [ -f "$MODS_DIR/maps/$f" ]; then
    cp -f "$MODS_DIR/maps/$f" "$MAPS_OUT_DIR/$f"
  fi
done

# Collect textures if present
if [ -d "$MODS_DIR/textures/demo" ]; then
  mkdir -p "$TEXTURES_OUT_DIR/demo"
  cp -f "$MODS_DIR/textures/demo/"* "$TEXTURES_OUT_DIR/demo/" 2>/dev/null || true
fi

# Create a simple pak1-maps pk3 with maps and a pak4-textures pk3 with textures
PK3_MAPS="$DEMO_BASE/pak1-maps.pk3"
PK3_TEXTURES="$DEMO_BASE/pak4-textures.pk3"
rm -f "$PK3_MAPS" "$PK3_TEXTURES"
# Always use zip for .pk3 as it's the standard format for Q3/D3
(cd "$DEMO_BASE" && zip -r -q "$(basename "$PK3_MAPS")" maps/*.bsp) 2>/dev/null || true
(cd "$DEMO_BASE" && zip -r -q "$(basename "$PK3_TEXTURES")" textures) 2>/dev/null || true

echo "Wrote: $PK3_MAPS"
echo "Wrote: $PK3_TEXTURES"
