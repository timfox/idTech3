#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
"$ROOT/scripts/validate_assets.sh" "$ROOT/examples/demo_game/mod"
