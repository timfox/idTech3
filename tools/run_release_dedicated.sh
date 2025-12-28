#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REL_DIR="$ROOT_DIR/release"

cd "$REL_DIR"

exec ./idtech3.server.x86_64 +set fs_basepath . +set fs_homepath . "$@"

