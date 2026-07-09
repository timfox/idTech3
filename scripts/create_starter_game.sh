#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <ident> <output_dir> [engine_release_dir]" >&2
  exit 1
fi

IDENT="$1"
OUT="$2"
ENGINE_RELEASE="${3:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/release}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

"$ROOT/scripts/new_mod_from_template.sh" game.starter "$IDENT" "$OUT"
"$ROOT/scripts/generate_mod_workspace.sh" "$OUT" "$ENGINE_RELEASE"

if [[ -f "$OUT/game.idproj" ]]; then
  python3 - <<PY
import json, pathlib
p = pathlib.Path("$OUT/game.idproj")
data = json.loads(p.read_text())
data.setdefault("Metadata", {})
data["Metadata"].setdefault("StartupMap", "")
data["Metadata"]["Onboarding"] = {
  "ConsoleFirstSteps": [
    "starter_status",
    "lua_run starter_boot()",
    "lua_run starter_checkpoint()"
  ],
  "PackCommand": "./pack_game.sh",
  "LaunchCommand": "./run_dev.sh"
}
p.write_text(json.dumps(data, indent=2) + "\n")
PY
fi

cat > "$OUT/run_dev.sh" <<EOF
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")" && pwd)"
BASE_ROOT="\$(cd "\$SCRIPT_DIR/.." && pwd)"
ENGINE="\${IDTECH3_ENGINE_BIN:-$ENGINE_RELEASE/idtech3}"
if [[ ! -x "\$ENGINE" ]]; then
  echo "run_dev: missing engine binary at \$ENGINE" >&2
  exit 1
fi
exec "\$ENGINE" +set fs_basepath "\$BASE_ROOT" +set fs_game "$IDENT" +set com_scriptWatch 1 "\$@"
EOF

cat > "$OUT/pack_game.sh" <<EOF
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")" && pwd)"
ROOT="$ROOT"
exec "\$ROOT/scripts/asset_pipeline.sh" "\$SCRIPT_DIR" "\$@"
EOF

chmod +x "$OUT/run_dev.sh" "$OUT/pack_game.sh"

echo "[starter] created $OUT"
echo "[starter] next steps:"
echo "[starter]   1. ./run_dev.sh"
echo "[starter]   2. edit scripts/lua/main.lua"
echo "[starter]   3. ./pack_game.sh"
echo "[starter]   4. open game.idproj in your editor"
