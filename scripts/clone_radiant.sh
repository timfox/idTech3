#!/usr/bin/env bash
# Clone idTech3Radiant beside the engine (optional; not a git submodule).
# Usage: ./scripts/clone_radiant.sh [target_dir]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET="${1:-${ROOT}/../idTech3Radiant}"
REPO="${RADIANT_REPO:-https://github.com/ArtmeScienceLab/idTech3Radiant.git}"

if [[ -d "$TARGET/.git" ]] || [[ -f "$TARGET/.git" ]]; then
  echo "[clone_radiant] already exists: $TARGET"
  exit 0
fi

echo "[clone_radiant] cloning $REPO -> $TARGET"
git clone --depth 1 "$REPO" "$TARGET"
echo "[clone_radiant] build: see $TARGET/COMPILING"
echo "[clone_radiant] then: ./scripts/install_radiant_gamepack.sh ./release/mygame"
