#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE_BIN="$ROOT_DIR/release/idtech3.x86_64"
MODS_DIR="${MODS_DIR:-$ROOT_DIR/mods}"

if [[ ! -f "$ENGINE_BIN" && ! -x "$ENGINE_BIN" ]]; then
  echo "Engine binary not found: $ENGINE_BIN" >&2
  exit 1
fi

if [[ ! -d "$MODS_DIR" ]]; then
  echo "Mods directory not found: $MODS_DIR" >&2
  exit 1
fi

echo "Launching engine with mods: $MODS_DIR"
exec "$ENGINE_BIN" -mods "$MODS_DIR"

