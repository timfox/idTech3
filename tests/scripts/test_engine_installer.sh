#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT

"$ROOT/scripts/install_engine.sh" --prefix /usr/local --destdir "$TMP/image" --sdk-only
ENGINE_CLI="$TMP/image/usr/local/bin/idtech3-engine"
[[ -x "$ENGINE_CLI" ]]
[[ -f "$TMP/image/usr/local/share/applications/idtech3-editor.desktop" ]]
[[ -f "$TMP/image/usr/local/share/idtech3/install-manifest.txt" ]]
IDTECH3_SDK_ROOT="$TMP/image/usr/local/share/idtech3/sdk" IDTECH3_ENGINE_DIR="$TMP/engine" "$ENGINE_CLI" new TestGame "$TMP/projects/TestGame"
grep -q '"Ident": "TestGame"' "$TMP/projects/TestGame/game.idproj"
IDTECH3_SDK_ROOT="$TMP/image/usr/local/share/idtech3/sdk" IDTECH3_ENGINE_DIR="$TMP/engine" "$ENGINE_CLI" help | grep -q 'idtech3-engine new'
if IDTECH3_SDK_ROOT="$TMP/image/usr/local/share/idtech3/sdk" IDTECH3_ENGINE_DIR="$TMP/engine" "$ENGINE_CLI" doctor; then
  echo "doctor unexpectedly accepted missing binaries" >&2; exit 1
fi
echo "OK: engine SDK installer and project front-end"
