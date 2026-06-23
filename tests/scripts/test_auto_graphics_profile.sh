#!/usr/bin/env bash
# Auto graphics profile: baseq3+qvm classic, native cgame modern.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

grep -q 'cl_autoGraphicsProfile' runtime/client/core/cl_cvars.c || fail 'missing cl_autoGraphicsProfile cvar'
grep -q 'CL_ApplyGraphicsProfile' runtime/client/core/cl_cgame.c || fail 'missing CL_ApplyGraphicsProfile'
grep -q 'CL_IsBaseQ3Game' runtime/client/core/cl_cgame.c || fail 'missing CL_IsBaseQ3Game'
grep -q 'classic_baseq3.cfg' runtime/client/core/cl_cgame.c || fail 'classic cfg reference missing'
grep -q 'modern_native.cfg' runtime/client/core/cl_cgame.c || fail 'modern cfg reference missing'

[ -f config/classic_baseq3.cfg ] || fail 'config/classic_baseq3.cfg missing'
[ -f config/modern_native.cfg ] || fail 'config/modern_native.cfg missing'
grep -q 'r_classicLighting 1' config/classic_baseq3.cfg || fail 'classic cfg must set r_classicLighting 1'
grep -q 'r_classicLighting 0' config/modern_native.cfg || fail 'modern cfg must set r_classicLighting 0'

grep -q 'classic_baseq3.cfg' scripts/compile_engine.sh || fail 'compile_engine must ship classic_baseq3.cfg'

pass "auto graphics profile contract"
