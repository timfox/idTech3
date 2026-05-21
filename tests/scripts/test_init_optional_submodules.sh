#!/usr/bin/env bash
# Smoke checks for scripts/init_optional_submodules.sh (no network clone required).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
INIT="$PROJECT_ROOT/scripts/init_optional_submodules.sh"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

[ -x "$INIT" ] || fail "missing or not executable: $INIT"

"$INIT" --help >/dev/null || fail "--help must exit 0"

if "$INIT" 2>/dev/null; then
	fail "must require --tiled, --svo, or --all"
fi

out="$("$INIT" --tiled --dry-run 2>&1)" || fail "--tiled --dry-run failed"
echo "$out" | grep -q 'dry-run: git' || fail "dry-run must print git submodule command"
echo "$out" | grep -q 'tools/tiled' || fail "dry-run must mention tools/tiled"

if ! grep -qF 'path = tools/tiled' "$PROJECT_ROOT/.gitmodules"; then
	fail ".gitmodules missing tools/tiled"
fi

echo "PASS: init_optional_submodules.sh"
