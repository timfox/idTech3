#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

STARTER="$TMP/release/mygame"

chmod +x "$ROOT/scripts/create_starter_game.sh"
"$ROOT/scripts/create_starter_game.sh" mygame "$STARTER"

for f in \
  "$STARTER/game.idproj" \
  "$STARTER/gameinfo.txt" \
  "$STARTER/default.cfg" \
  "$STARTER/autoexec.cfg" \
  "$STARTER/asset_pipeline.conf" \
  "$STARTER/loc/en.loc" \
  "$STARTER/scripts/lua/main.lua" \
  "$STARTER/run_dev.sh" \
  "$STARTER/pack_game.sh" \
  "$STARTER/.vscode/launch.json"; do
  [[ -f "$f" ]] || fail "missing scaffold file: $f"
done

grep -q '"Ident": "mygame"' "$STARTER/game.idproj" || fail "game.idproj ident not rewritten"
grep -q 'starter_boot' "$STARTER/scripts/lua/main.lua" || fail "starter Lua shell missing"
grep -q 'fs_game "mygame"' "$STARTER/run_dev.sh" || fail "run_dev.sh missing fs_game"

(
  cd "$STARTER"
  ./pack_game.sh --skip-shaders --output-root "$TMP/out" --release-root "$TMP/release-pack"
)

[[ -f "$TMP/out/mygame/package/mygame.pk3" ]] || fail "missing packaged starter pk3"
[[ -f "$TMP/release-pack/mygame/mygame.pk3" ]] || fail "missing copied starter pk3"

echo "OK: starter template scaffolds and packages"
