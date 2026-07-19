#!/usr/bin/env bash
# VGS (McGraw MIG 2024) — unit + wiring smoke.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-${ROOT}/build-vk-Release}"
fail() { echo "FAIL: $*" >&2; exit 1; }

test -f "$ROOT/docs/VGS.md" || fail "docs/VGS.md missing"
test -f "$ROOT/extensions/research/vgs/vgs_model.c" || fail "vgs_model.c missing"
test -f "$ROOT/examples/demo_game/mod/demo_vgs.cfg" || fail "demo_vgs.cfg missing"

rg -q '3677388.3696322' "$ROOT/docs/VGS.md" || fail "DOI citation missing"
rg -q 'vgs_pipeline' "$ROOT/docs/VGS.md" || fail "vgs_pipeline docs missing"
rg -q 'Vgs_ConsoleInit' "$ROOT/engine/core/common.c" || fail "Vgs_ConsoleInit not wired"
rg -q 'cl_vgs_enable' "$ROOT/extensions/research/vgs/vgs_console.c" || fail "cl_vgs_enable missing"
rg -q 'vgs/vgs_model.c' "$ROOT/cmake/IdTech3QcommonExtensions.cmake" || fail "CMake missing vgs"
rg -q 'softblob|XPBD' "$ROOT/docs/VGS.md" || fail "softblob honesty missing"

if [[ -x "${BUILD}/unit_vgs" ]]; then
  "${BUILD}/unit_vgs"
else
  echo "SKIP: unit_vgs not built yet"
fi

echo "test_vgs.sh: OK"
