#!/usr/bin/env bash
# Run all automated OpenArena / Q3 QVM gates (no retail pk3 required).
# Exit 0 only if every executed step succeeds.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RELEASE_DIR="${1:-$PROJECT_ROOT/release}"

cd "$PROJECT_ROOT"

echo "=== OpenArena / Q3 validation ==="
echo "Release: $RELEASE_DIR"
echo ""

echo "--- 1/5 beta trace format ---"
./tests/scripts/test_beta_trace_format.sh
echo ""

echo "--- 2/5 q3_openarena_compat_check ---"
./scripts/q3_openarena_compat_check.sh "$RELEASE_DIR"
echo ""

echo "--- 3/5 test_run_openarena (launcher regression) ---"
./tests/scripts/test_run_openarena.sh "$PROJECT_ROOT/scripts/run_openarena.sh"
echo ""

if [ -x "$RELEASE_DIR/idtech3" ] || [ -f "$RELEASE_DIR/idtech3" ]; then
	echo "--- 4/5 smoke_test (subset via full smoke) ---"
	if [ -x "$SCRIPT_DIR/smoke_test.sh" ]; then
		"$SCRIPT_DIR/smoke_test.sh" "$RELEASE_DIR"
	fi
	echo ""
else
	echo "--- 4/5 smoke_test SKIPPED (build release/ first) ---"
	echo ""
fi

DEVDATA="$PROJECT_ROOT/docs/renderer_validation/devdata/rtest_base/vm/qagame.qvm"
if [ -f "$DEVDATA" ]; then
	echo "--- 5/5 Tier B devdata map load ---"
	./scripts/run_renderer_tier_b_devdata.sh
	echo ""
else
	echo "--- 5/5 Tier B devdata SKIPPED (no qagame.qvm in devdata) ---"
	echo ""
fi

echo "=== OpenArena / Q3 validation passed ==="
