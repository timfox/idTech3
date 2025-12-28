#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REL_DIR="$ROOT_DIR/release"

cd "$REL_DIR"

# Default to release/ for both basepath and homepath so it behaves like other idtech3 forks.
exec ./idtech3.x86_64 +set fs_basepath . +set fs_homepath . "$@"

