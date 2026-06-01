#!/usr/bin/env bash
# CI/local check: Spectral-Energy runtime hook artifacts (no GPU inference).
# Usage: ./scripts/spec_energy_runtime_check.sh [release_dir] [upstream_repo]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RELEASE_DIR="${1:-$PROJECT_ROOT/release}"
UPSTREAM_REPO="${2:-$PROJECT_ROOT/external/flux_spec_energy}"
# shellcheck source=lib/release_bin.sh
source "$SCRIPT_DIR/lib/release_bin.sh"

fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "OK: $*"; }
skip() { echo "SKIP: $*"; exit 0; }

CLIENT="$(release_bin_path "$RELEASE_DIR" idtech3)"
[ -n "$CLIENT" ] || fail "release client not found under $RELEASE_DIR"

if [ ! -f "$RELEASE_DIR/spec_energy_flux_generate.py" ]; then
	skip "release/spec_energy_flux_generate.py missing (binary-only layout; run stage_ci_release.sh)"
fi

release_bin_has_text "$CLIENT" 'spec_energy_generate' || fail "spec_energy_generate not linked in client"
release_bin_has_text "$CLIENT" 'cl_spec_energy_enable' || fail "cl_spec_energy_enable missing"
ok "client has spec_energy console symbols"
python3 -m py_compile "$RELEASE_DIR/spec_energy_flux_generate.py"
ok "spec_energy_flux_generate.py syntax valid"

[ -d "$PROJECT_ROOT/external/flux_spec_energy/flux_sega" ] || fail "vendored external/flux_spec_energy/flux_sega missing"
ok "vendored upstream flux_sega present"

if [ -n "$UPSTREAM_REPO" ]; then
	"$SCRIPT_DIR/spec_energy_check.sh" "$UPSTREAM_REPO"
	ok "upstream Spectral-Energy repo layout"
fi

echo "spec_energy_runtime_check: all checks passed"
