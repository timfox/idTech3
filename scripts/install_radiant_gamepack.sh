#!/usr/bin/env bash
# Install idTech3Radiant gamepack fragment into a mod directory.
# Usage: ./scripts/install_radiant_gamepack.sh <mod_dir> [engine_release_dir]
set -euo pipefail

MOD="${1:?mod directory required}"
ENGINE="${2:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/release}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${ROOT}/examples/radiant"
MOD_ABS="$(cd "$MOD" && pwd)"

if [[ ! -d "$SRC" ]]; then
  echo "Missing gamepack source: $SRC" >&2
  exit 1
fi

mkdir -p "${MOD_ABS}/scripts" "${MOD_ABS}/maps" "${MOD_ABS}/mapsrc" "${MOD_ABS}/Editor"

cp -a "${SRC}/scripts/." "${MOD_ABS}/scripts/"
cp -a "${SRC}/Editor/." "${MOD_ABS}/Editor/"
cp "${SRC}/default_build_menu.xml" "${MOD_ABS}/default_build_menu.xml"

# Patch idtech3.game engine paths
GAME_FILE="${MOD_ABS}/idtech3.game"
cp "${SRC}/idtech3.game" "$GAME_FILE"
sed -i "s|__ENGINE_RELEASE__|${ENGINE}|g" "$GAME_FILE"
sed -i "s|__ENGINE_RELEASE_WIN__|${ENGINE}|g" "$GAME_FILE"

# Merge Radiant section into game.idproj if present
if [[ -f "${MOD_ABS}/game.idproj" ]]; then
  python3 - "$MOD_ABS" "$ENGINE" <<'PY'
import json, pathlib, sys
mod, engine = sys.argv[1:3]
p = pathlib.Path(mod) / "game.idproj"
data = json.loads(p.read_text())
data.setdefault("Radiant", {})
data["Radiant"].update({
    "EngineRelease": engine,
    "EngineBinary": f"{engine}/idtech3",
    "EntityDef": "scripts/entities_idtech3_bridge.def",
    "BuildMenu": "default_build_menu.xml",
    "MapSrcDir": "mapsrc",
    "MapsDir": "maps",
})
if not data.get("EditorScripts"):
    data["EditorScripts"] = [
        "Editor/bridge_tools.py",
        "Editor/watch_studio_export.py",
    ]
p.write_text(json.dumps(data, indent=2) + "\n")
PY
fi

echo "[radiant] installed gamepack into ${MOD_ABS}"
echo "[radiant]   entity defs: scripts/entities_idtech3_bridge.def"
echo "[radiant]   Editor/: bridge_tools.py, watch_studio_export.py"
echo "[radiant]   idtech3.game engine path -> ${ENGINE}"
echo "[radiant] next: copy idtech3.game to Radiant gamepacks/ or set engine path in Preferences"
