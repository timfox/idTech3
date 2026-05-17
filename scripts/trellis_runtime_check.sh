#!/usr/bin/env bash
# CI/local check: TRELLIS.2 runtime hook artifacts (no GPU inference).
# Usage: ./scripts/trellis_runtime_check.sh [release_dir] [trellis_repo]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RELEASE_DIR="${1:-$PROJECT_ROOT/release}"
TRELLIS_REPO="${2:-}"

fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "OK: $*"; }

[ -f "$RELEASE_DIR/idtech3" ] || [ -f "$RELEASE_DIR/idtech3.x86_64" ] || fail "release client not found under $RELEASE_DIR"
CLIENT="$RELEASE_DIR/idtech3"
[ -f "$CLIENT" ] || CLIENT="$RELEASE_DIR/idtech3.x86_64"

grep -q trellis_generate < <(strings "$CLIENT" 2>/dev/null) || fail "trellis_generate not linked in client"
grep -q cl_trellis_enable < <(strings "$CLIENT" 2>/dev/null) || fail "cl_trellis_enable missing"
ok "client has TRELLIS console symbols"

[ -f "$RELEASE_DIR/trellis_image_to_glb.py" ] || fail "missing release/trellis_image_to_glb.py"
python3 -m py_compile "$RELEASE_DIR/trellis_image_to_glb.py"
ok "trellis_image_to_glb.py syntax valid"

if [ -n "$TRELLIS_REPO" ]; then
  "$SCRIPT_DIR/trellis_check.sh" "$TRELLIS_REPO"
  ok "upstream TRELLIS.2 repo layout"
fi

echo "trellis_runtime_check: all checks passed"
