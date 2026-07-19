#!/usr/bin/env bash
# CEM (Xie et al. arXiv:2508.04076) — unit + wiring smoke.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-${ROOT}/build-vk-Release}"
fail() { echo "FAIL: $*" >&2; exit 1; }

test -f "$ROOT/docs/CEM.md" || fail "docs/CEM.md missing"
test -f "$ROOT/extensions/research/cem/cem_model.c" || fail "cem_model.c missing"
test -f "$ROOT/examples/demo_game/mod/demo_cem.cfg" || fail "demo_cem.cfg missing"

rg -q '2508.04076' "$ROOT/docs/CEM.md" || fail "arXiv citation missing"
rg -q 'cem_pipeline' "$ROOT/docs/CEM.md" || fail "cem_pipeline docs missing"
rg -q 'Graphical abstract' "$ROOT/docs/CEM.md" || fail "graphical abstract missing"
rg -q 'Cem_ConsoleInit' "$ROOT/engine/core/common.c" || fail "Cem_ConsoleInit not wired"
rg -q 'cl_cem_enable' "$ROOT/extensions/research/cem/cem_console.c" || fail "cl_cem_enable missing"
rg -q 'cem/cem_model.c' "$ROOT/cmake/IdTech3QcommonExtensions.cmake" || fail "CMake missing cem"
rg -q 'phys_dmm|DMM' "$ROOT/docs/CEM.md" || fail "DMM honesty missing"

if [[ -x "${BUILD}/unit_cem" ]]; then
  "${BUILD}/unit_cem"
else
  echo "SKIP: unit_cem not built yet"
fi

echo "test_cem.sh: OK"
