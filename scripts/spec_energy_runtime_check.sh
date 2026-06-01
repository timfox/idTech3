#!/usr/bin/env bash
# CI/local check: Spectral-Energy runtime hook artifacts (no GPU inference).
# Usage: ./scripts/spec_energy_runtime_check.sh [release_dir]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RELEASE_DIR="${1:-$PROJECT_ROOT/release}"

fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "OK: $*"; }

[ -f "$RELEASE_DIR/idtech3" ] || [ -f "$RELEASE_DIR/idtech3.x86_64" ] || fail "release client not found under $RELEASE_DIR"
CLIENT="$RELEASE_DIR/idtech3"
[ -f "$CLIENT" ] || CLIENT="$RELEASE_DIR/idtech3.x86_64"

grep -q spec_energy_generate < <(strings "$CLIENT" 2>/dev/null) || fail "spec_energy_generate not linked in client"
grep -q cl_spec_energy_enable < <(strings "$CLIENT" 2>/dev/null) || fail "cl_spec_energy_enable missing"
ok "client has spec_energy console symbols"

[ -f "$RELEASE_DIR/spec_energy_flux_generate.py" ] || fail "missing release/spec_energy_flux_generate.py"
python3 -m py_compile "$RELEASE_DIR/spec_energy_flux_generate.py"
ok "spec_energy_flux_generate.py syntax valid"

[ -d "$PROJECT_ROOT/external/flux_spec_energy/flux_sega" ] || fail "vendored external/flux_spec_energy/flux_sega missing"
ok "vendored upstream flux_sega present"

echo "spec_energy_runtime_check: all checks passed"
