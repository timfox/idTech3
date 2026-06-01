#!/usr/bin/env bash
# Tier B validation using shipped minimal devdata (no retail Q3A/OA pk3 required).
# Proves dedicated map load + regression asset manifest when rtest_base is present.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DEV_BASE="$PROJECT_ROOT/docs/renderer_validation/devdata/rtest_base"
RELEASE_DIR="${RELEASE_DIR:-$PROJECT_ROOT/release}"

if [ ! -f "$DEV_BASE/vm/qagame.qvm" ]; then
	echo "Error: devdata missing qagame.qvm. Run: ./scripts/build_renderer_devdata.sh" >&2
	echo "  (requires ioquake3 qagame.qvm — see docs/renderer_validation/devdata/README.md)" >&2
	exit 2
fi

export GAME_BASE="$DEV_BASE"
export GAME_ASSETS_LIST="$PROJECT_ROOT/docs/renderer_validation/devdata/OPTIONAL_GAME_ASSETS.txt"
export RELEASE_DIR

echo "=== Tier B devdata (rtest_base) ==="
echo "  GAME_BASE=$GAME_BASE"
echo "  RELEASE_DIR=$RELEASE_DIR"
echo ""

"$PROJECT_ROOT/scripts/renderer_regression_check.sh"
echo ""
"$PROJECT_ROOT/scripts/renderer_regression_maps.sh"
echo ""
"$PROJECT_ROOT/scripts/tier_b_devdata_log_gate.sh"

echo ""
echo "TIER B DEVDATA PASSED"
