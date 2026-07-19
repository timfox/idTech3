#!/usr/bin/env bash
# Neural Enhancement BRDF (arXiv:2604.24081) — unit + wiring smoke.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-${ROOT}/build-vk-Release}"
fail() { echo "FAIL: $*" >&2; exit 1; }

test -f "$ROOT/docs/NEBRDF.md" || fail "docs/NEBRDF.md missing"
test -f "$ROOT/extensions/research/nebrdf/nebrdf_model.c" || fail "nebrdf_model.c missing"
test -f "$ROOT/examples/demo_game/mod/demo_nebrdf.cfg" || fail "demo_nebrdf.cfg missing"

rg -q 'arXiv:2604.24081' "$ROOT/docs/NEBRDF.md" || fail "arXiv citation missing"
rg -q 'nebrdf_graph' "$ROOT/docs/NEBRDF.md" || fail "nebrdf_graph docs missing"
rg -q 'NeBrdf_ConsoleInit' "$ROOT/engine/core/common.c" || fail "NeBrdf_ConsoleInit not wired"
rg -q 'cl_nebrdf_enable' "$ROOT/extensions/research/nebrdf/nebrdf_console.c" || fail "cl_nebrdf_enable missing"
rg -q 'nebrdf/nebrdf_model.c' "$ROOT/cmake/IdTech3QcommonExtensions.cmake" || fail "CMake missing nebrdf"
rg -q 'no weights|not ship trained|No MERL' "$ROOT/docs/NEBRDF.md" || fail "limitations note missing"

if [[ -x "${BUILD}/unit_nebrdf" ]]; then
  "${BUILD}/unit_nebrdf"
else
  echo "SKIP: unit_nebrdf not built yet"
fi

echo "test_nebrdf.sh: OK"
