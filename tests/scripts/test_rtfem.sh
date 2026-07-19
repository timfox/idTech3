#!/usr/bin/env bash
# RTFEM (Parker & O'Brien SCA 2009) — unit + wiring smoke.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-${ROOT}/build-vk-Release}"
fail() { echo "FAIL: $*" >&2; exit 1; }

test -f "$ROOT/docs/RTFEM.md" || fail "docs/RTFEM.md missing"
test -f "$ROOT/extensions/research/rtfem/rtfem_model.c" || fail "rtfem_model.c missing"
test -f "$ROOT/examples/demo_game/mod/demo_rtfem.cfg" || fail "demo_rtfem.cfg missing"

rg -q 'SCA 2009' "$ROOT/docs/RTFEM.md" || fail "SCA 2009 citation missing"
rg -q 'rtfem_pipeline' "$ROOT/docs/RTFEM.md" || fail "rtfem_pipeline docs missing"
rg -q 'RtFem_ConsoleInit' "$ROOT/engine/core/common.c" || fail "RtFem_ConsoleInit not wired"
rg -q 'cl_rtfem_enable' "$ROOT/extensions/research/rtfem/rtfem_console.c" || fail "cl_rtfem_enable missing"
rg -q 'rtfem/rtfem_model.c' "$ROOT/cmake/IdTech3QcommonExtensions.cmake" || fail "CMake missing rtfem"
rg -q 'not.*FEM|≠|Not FEM' "$ROOT/docs/RTFEM.md" || fail "DMM≠FEM honesty missing"

if [[ -x "${BUILD}/unit_rtfem" ]]; then
  "${BUILD}/unit_rtfem"
else
  echo "SKIP: unit_rtfem not built yet"
fi

echo "test_rtfem.sh: OK"
