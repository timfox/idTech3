#!/usr/bin/env bash
# Full "level up" validation: OpenArena/QVM, beta traces, generative hooks, CI mirrors.
# No retail game data required for most steps.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RELEASE_DIR="${1:-$PROJECT_ROOT/release}"

cd "$PROJECT_ROOT"

echo "=== id Tech 3 level-up validation ==="
echo "Release: $RELEASE_DIR"
echo ""

echo "--- 1/6 beta trace example format ---"
./tests/scripts/test_beta_trace_format.sh
echo ""

echo "--- 2/6 Q3 / OpenArena compat ---"
./scripts/q3_openarena_compat_check.sh "$RELEASE_DIR"
echo ""

echo "--- 3/6 OpenArena launcher regression ---"
./tests/scripts/test_run_openarena.sh "$PROJECT_ROOT/scripts/run_openarena.sh"
echo ""

if [ -f "$RELEASE_DIR/idtech3" ] || [ -f "$RELEASE_DIR/idtech3.exe" ] || \
   [ -f "$RELEASE_DIR/idtech3.x86_64" ] || [ -f "$RELEASE_DIR/idtech3.x86_64.exe" ]; then
	echo "--- 4/6 smoke test ---"
	./scripts/smoke_test.sh "$RELEASE_DIR"
	echo ""
else
	echo "--- 4/6 smoke test SKIPPED (no release client; run compile_engine.sh vulkan) ---"
	echo ""
fi

SPEC_REPO="${SPEC_ENERGY_REPO:-$PROJECT_ROOT/external/flux_spec_energy}"
if [ -x "$SCRIPT_DIR/spec_energy_check.sh" ] && [ -d "$SPEC_REPO" ]; then
	echo "--- 5/6 spec_energy static check ---"
	./scripts/spec_energy_check.sh "$SPEC_REPO"
	echo ""
else
	echo "--- 5/6 spec_energy SKIPPED (set SPEC_ENERGY_REPO or clone under external/flux_spec_energy) ---"
	echo ""
fi

if [ -d "$PROJECT_ROOT/build-vk-Release" ]; then
	echo "--- 6/6 CTest (subset: scripts + units) ---"
	( cd build-vk-Release && ctest -C Release --output-on-failure -j1 \
		-R 'test_beta_trace|test_level_up|test_run_openarena|unit_' )
	echo ""
else
	echo "--- 6/6 CTest SKIPPED (build-vk-Release missing) ---"
	echo ""
fi

echo "=== Level-up validation passed ==="
