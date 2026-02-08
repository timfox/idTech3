#!/usr/bin/env bash

# Convenience wrapper so authors can run a single command from the repo root.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
exec "$ROOT_DIR/scripts/build_maps.sh" "$@"
