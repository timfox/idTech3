#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

[ -f scripts/release_bin_path.sh ] || fail "missing scripts/release_bin_path.sh"
grep -q 'release_bin_path' scripts/smoke_test.sh || fail "smoke_test must source release_bin_path.sh"
grep -q 'release_bin_path' scripts/q3_openarena_compat_check.sh || fail "q3_openarena_compat_check must source release_bin_path.sh"
grep -q '\$base\.arm' scripts/release_bin_path.sh || fail "release_bin_path must resolve .arm suffix"

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
touch "$tmpdir/idtech3.arm" "$tmpdir/idtech3_server.arm"

# shellcheck source=/dev/null
. "$ROOT/scripts/release_bin_path.sh"
[ "$(release_bin_path "$tmpdir" idtech3)" = "$tmpdir/idtech3.arm" ] || fail "idtech3.arm not resolved"
[ "$(release_bin_path "$tmpdir" idtech3_server)" = "$tmpdir/idtech3_server.arm" ] || fail "idtech3_server.arm not resolved"

pass "release_bin_path resolves ARM CI artifact names"
