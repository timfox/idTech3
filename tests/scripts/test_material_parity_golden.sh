#!/usr/bin/env bash
# Contract test for the cross-owner material parity capture.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CFG="$ROOT/examples/demo_game/mod/material_parity_golden.cfg"
MANIFEST="$ROOT/tests/data/golden/manifest.txt"
README="$ROOT/tests/data/golden/material_parity/README.txt"

fail() { echo "FAIL: $*" >&2; exit 1; }

[[ -f "$CFG" ]] || fail "material parity capture config missing"
[[ -f "$README" ]] || fail "material parity golden fixture missing"
grep -q 'map rtest_parity' "$CFG" || fail "capture must use the shared rtest_parity scene"
for name in forward deferred wboit ssr rtx; do
	grep -q "screenshot material_parity_${name}" "$CFG" || fail "missing ${name} screenshot"
done
grep -q 'material_parity/README.txt' "$MANIFEST" || fail "golden manifest has no material parity slot"

echo "Material parity golden capture contract passed."
