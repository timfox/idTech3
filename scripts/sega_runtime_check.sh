#!/usr/bin/env bash
# CI/local check: SEGA runtime hook artifacts (no GPU inference).
# Usage: ./scripts/sega_runtime_check.sh [release_dir]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RELEASE_DIR="${1:-$PROJECT_ROOT/release}"

fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "OK: $*"; }

[ -f "$RELEASE_DIR/idtech3" ] || [ -f "$RELEASE_DIR/idtech3.x86_64" ] || fail "release client not found under $RELEASE_DIR"
CLIENT="$RELEASE_DIR/idtech3"
[ -f "$CLIENT" ] || CLIENT="$RELEASE_DIR/idtech3.x86_64"

grep -q sega_generate < <(strings "$CLIENT" 2>/dev/null) || fail "sega_generate not linked in client"
grep -q cl_sega_enable < <(strings "$CLIENT" 2>/dev/null) || fail "cl_sega_enable missing"
ok "client has SEGA console symbols"

[ -f "$RELEASE_DIR/sega_flux_generate.py" ] || fail "missing release/sega_flux_generate.py"
python3 -m py_compile "$RELEASE_DIR/sega_flux_generate.py"
ok "sega_flux_generate.py syntax valid"

[ -d "$PROJECT_ROOT/external/sega/flux_sega" ] || fail "vendored external/sega/flux_sega missing"
ok "vendored upstream flux_sega present"

echo "sega_runtime_check: all checks passed"
