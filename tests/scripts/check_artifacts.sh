#!/usr/bin/env bash
# Validates that build artifacts exist in release/.
# Used by CTest for make test / ctest.
set -euo pipefail

RELEASE_DIR="${1:-}"
if [ -z "$RELEASE_DIR" ]; then
  SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
  RELEASE_DIR="$PROJECT_ROOT/release"
fi
RELEASE_DIR="$(cd "$RELEASE_DIR" 2>/dev/null && pwd)" || true

if [ -z "${RELEASE_DIR:-}" ] || [ ! -d "$RELEASE_DIR" ]; then
  echo "Release dir not found"
  exit 1
fi

missing=0
for f in idtech3 idtech3_server; do
  if [ ! -f "$RELEASE_DIR/$f" ]; then
    echo "Missing: $RELEASE_DIR/$f"
    missing=1
  fi
done

if [ -d "$RELEASE_DIR/examples" ]; then
  for cfg in q3_vulkan_compat.cfg q3_classic_mod.cfg; do
    if [ ! -f "$RELEASE_DIR/examples/$cfg" ]; then
      echo "Missing: $RELEASE_DIR/examples/$cfg"
      missing=1
    fi
  done
fi

if [ "$missing" -ne 0 ]; then
  exit 1
fi
echo "Artifacts OK: $RELEASE_DIR"
