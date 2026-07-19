#!/usr/bin/env bash
# How Dark is Dark (arXiv:2601.05094) — unit + wiring smoke.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-${ROOT}/build-vk-Release}"
fail() { echo "FAIL: $*" >&2; exit 1; }

test -f "$ROOT/docs/HOWDARK.md" || fail "docs/HOWDARK.md missing"
test -f "$ROOT/extensions/research/howdark/howdark_model.c" || fail "howdark_model.c missing"
test -f "$ROOT/examples/demo_game/mod/demo_howdark.cfg" || fail "demo_howdark.cfg missing"

rg -q 'arXiv:2601.05094' "$ROOT/docs/HOWDARK.md" || fail "arXiv citation missing from docs"
rg -q 'howdark_rank' "$ROOT/docs/HOWDARK.md" || fail "howdark_rank docs missing"
rg -q 'HowDark_ConsoleInit' "$ROOT/engine/core/common.c" || fail "HowDark_ConsoleInit not wired"
rg -q 'cl_howdark_enable' "$ROOT/extensions/research/howdark/howdark_console.c" || fail "cl_howdark_enable missing"
rg -q 'howdark/howdark_model.c' "$ROOT/cmake/IdTech3QcommonExtensions.cmake" || fail "CMake research sources missing howdark"
rg -q 'lighting ownership|not measured|measured EXR' "$ROOT/docs/HOWDARK.md" || fail "limitations / measured-EXR note missing"

if [[ -x "${BUILD}/unit_howdark" ]]; then
  "${BUILD}/unit_howdark"
else
  echo "SKIP: unit_howdark not built yet (configure/build unit target)"
fi

echo "test_howdark.sh: OK"
