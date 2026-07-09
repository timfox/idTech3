#!/usr/bin/env bash
# Scaffold a mod from examples/templates/ (s&box new-project flow).
# Usage: ./scripts/new_mod_from_template.sh <template> <ident> <output_dir>
set -euo pipefail

if [[ $# -lt 3 ]]; then
  echo "Usage: $0 <template> <ident> <output_dir>" >&2
  echo "  template: game.minimal | game.starter | addon.minimal" >&2
  echo "  ident:    mod folder name (e.g. mygame)" >&2
  exit 1
fi

TEMPLATE="$1"
IDENT="$2"
OUT="$3"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${ROOT}/examples/templates/${TEMPLATE}"

if [[ ! -d "$SRC" ]]; then
  echo "Unknown template: $SRC" >&2
  exit 1
fi

mkdir -p "$OUT"
cp -a "${SRC}/." "$OUT/"
cp "${ROOT}/examples/templates/template.gitignore" "${OUT}/.gitignore"
mkdir -p "${OUT}/maps" "${OUT}/mapsrc" "${OUT}/Editor"

if [[ -f "${OUT}/game.idproj" ]]; then
  python3 - <<PY
import json, pathlib
p = pathlib.Path("${OUT}/game.idproj")
data = json.loads(p.read_text())
data["Ident"] = "${IDENT}"
data["Title"] = data.get("Title", "${IDENT}").replace("game.minimal", "${IDENT}").replace("addon.minimal", "${IDENT}")
p.write_text(json.dumps(data, indent=2) + "\n")
PY
fi

cat > "${OUT}/gameinfo.txt" <<EOF
title "${IDENT}"
requires_engine >= 1.0
EOF

echo "[new_mod] created ${OUT} from template ${TEMPLATE} (ident=${IDENT})"
echo "[new_mod] next:"
echo "[new_mod]   ./scripts/install_radiant_gamepack.sh ${OUT}"
echo "[new_mod]   ./scripts/generate_radiant_workspace.sh ${OUT}"
echo "[new_mod]   pack default.cfg into a .pk3, set fs_game ${IDENT}"
