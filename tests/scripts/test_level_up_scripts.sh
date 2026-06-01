#!/usr/bin/env bash
# Fast gate: level-up scripts exist and bash syntax is valid (no release binaries required).
set -euo pipefail

ROOT="${1:-$(cd "$(dirname "$0")/../.." && pwd)}"

fail() {
	echo "test_level_up_scripts: $*" >&2
	exit 1
}

for f in \
	"$ROOT/scripts/level_up_validate.sh" \
	"$ROOT/scripts/lib/release_bin.sh" \
	"$ROOT/scripts/openarena_validate.sh" \
	"$ROOT/docs/LEVEL_UP.md"; do
	[ -f "$f" ] || fail "missing $f"
done

bash -n "$ROOT/scripts/level_up_validate.sh" || fail "level_up_validate.sh syntax"
bash -n "$ROOT/scripts/lib/release_bin.sh" || fail "release_bin.sh syntax"

grep -q 'test_beta_trace_format' "$ROOT/scripts/level_up_validate.sh" || \
	fail "level_up_validate must include beta trace step"
grep -q 'q3_openarena_compat_check' "$ROOT/scripts/level_up_validate.sh" || \
	fail "level_up_validate must include Q3/OA compat"

echo "test_level_up_scripts: ok"
