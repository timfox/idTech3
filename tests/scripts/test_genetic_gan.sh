#!/usr/bin/env bash
# Validation: genetic GAN module + wrapper script present.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-$ROOT/build-vk-Release}"

fail() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "OK: $*"; }

[ -f "$ROOT/src/world/genetic_gan.c" ] || fail "missing genetic_gan.c"
[ -f "$ROOT/src/world/genetic_gan.h" ] || fail "missing genetic_gan.h"
[ -f "$ROOT/scripts/genetic_gan_decode.py" ] || fail "missing genetic_gan_decode.py"

if [ -x "$BUILD/unit_genetic_gan" ] || [ -f "$BUILD/unit_genetic_gan" ]; then
	"$BUILD/unit_genetic_gan" || fail "unit_genetic_gan failed"
	ok "unit_genetic_gan"
else
	echo "SKIP: unit_genetic_gan not built (run cmake in $BUILD)"
fi

python3 -m py_compile "$ROOT/scripts/genetic_gan_decode.py"
ok "genetic_gan_decode.py syntax"

exit 0
