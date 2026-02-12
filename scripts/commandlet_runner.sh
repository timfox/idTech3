#!/usr/bin/env bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

usage() {
  cat <<'EOF'
Usage: ./scripts/commandlet_runner.sh [--build-dir path] <engine_tool command> [...]

Builds engine_tool if needed, then runs it with the provided arguments.
EOF
  exit 1
}

BUILD_DIR="$REPO_ROOT/build-vk-Release"
if [ "${1:-}" = "--build-dir" ]; then
  if [ "$#" -lt 3 ]; then
    usage
  fi
  BUILD_DIR="$2"
  shift 2
fi

if [ "$#" -lt 1 ]; then
  usage
fi

if [ ! -d "$BUILD_DIR" ]; then
  echo "commandlet_runner: build directory '$BUILD_DIR' not found"
  exit 1
fi

ENGINE_TOOL="$BUILD_DIR/tools/engine_tool"
cmake --build "$BUILD_DIR" --target engine_tool

cd "$REPO_ROOT"
"$ENGINE_TOOL" "$@"
