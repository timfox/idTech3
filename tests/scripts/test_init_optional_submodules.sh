#!/usr/bin/env bash
# Smoke checks for scripts/init_optional_submodules.sh (no network clone required).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
INIT="$PROJECT_ROOT/scripts/init_optional_submodules.sh"
source "$PROJECT_ROOT/tests/scripts/idtech3_test_paths.sh"
idtech3_test_paths_init "$PROJECT_ROOT"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

[ -x "$INIT" ] || fail "missing or not executable: $INIT"

"$INIT" --help >/dev/null || fail "--help must exit 0"

if "$INIT" 2>/dev/null; then
	fail "must require --tiled, --svo, --freeusd, --backend, or --all"
fi

FREEUSD_PATH="$(idtech3_submodule_path freeusd)"
BACKEND_PATH="$(idtech3_submodule_path backend)"
SVO_PATH="$(idtech3_submodule_path svo)"

if ! grep -qF "path = $FREEUSD_PATH" "$PROJECT_ROOT/.gitmodules"; then
	fail ".gitmodules missing $FREEUSD_PATH"
fi

if ! grep -qF "path = $BACKEND_PATH" "$PROJECT_ROOT/.gitmodules"; then
	fail ".gitmodules missing $BACKEND_PATH"
fi

out="$("$INIT" --svo --dry-run 2>&1)" || fail "--svo --dry-run failed"
echo "$out" | grep -q 'dry-run: git' || fail "dry-run must print git submodule command"
echo "$out" | grep -q 'SparseVoxelOctree' || fail "dry-run must mention SparseVoxelOctree"

out="$("$INIT" --freeusd 2>&1)" || fail "--freeusd failed"
echo "$out" | grep -qE 'ok: FreeUSD|init: FreeUSD' || fail "--freeusd must init or report already initialized"

echo "PASS: init_optional_submodules.sh"
