#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
chmod +x "$ROOT/scripts/check_loc_keys.sh"
"$ROOT/scripts/check_loc_keys.sh" "$ROOT/examples/demo_game/loc"
