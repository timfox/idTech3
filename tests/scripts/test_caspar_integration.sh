#!/usr/bin/env bash
# Static Caspar integration checks (no SymForce clone, no GPU).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

[ -f "$PROJECT_ROOT/docs/CASPAR.md" ] || fail "docs/CASPAR.md missing"
[ -f "$PROJECT_ROOT/external/README.md" ] || fail "external/README.md missing"

for s in build_symforce_caspar.sh caspar_check.sh run_caspar_kernel_example.sh run_caspar_bal_example.sh; do
	[ -x "$PROJECT_ROOT/scripts/$s" ] || fail "scripts/$s not executable"
	bash -n "$PROJECT_ROOT/scripts/$s" || fail "bash -n scripts/$s"
done

grep -q 'external/symforce' "$PROJECT_ROOT/.gitmodules" || fail ".gitmodules symforce entry"
grep -q '\-\-symforce' "$PROJECT_ROOT/scripts/init_optional_submodules.sh" || fail "init script missing --symforce"

echo "PASS: test_caspar_integration"
