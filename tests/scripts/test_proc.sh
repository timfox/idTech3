#!/usr/bin/env bash
# Procedural world pattern smoke checks (Voronoi, grid, hex, etc.).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
cd "$ROOT"

echo "[test_proc] checking sources..."
WP="$(idtech3_require_file modules/world/world_proc.cpp src/world/world_proc.cpp)"
WPH="$(idtech3_require_file modules/world/world_proc.h src/world/world_proc.h)"
CL_P="$(idtech3_require_file runtime/client/world/cl_proc.cpp src/client/world/cl_proc.cpp)"
idtech3_require_file runtime/client/world/cl_proc.h src/client/world/cl_proc.h >/dev/null
CL_MAIN="$(idtech3_require_file runtime/client/core/cl_main.c src/client/core/cl_main.c)"

echo "[test_proc] grep API symbols..."
rg -q 'WorldProc_SampleWorld' "$WP"
rg -q 'WorldProc_SampleVoronoi' "$WP"
rg -q 'WorldProc_ParsePattern' "$WP"
rg -q 'WPROC_VORONOI' "$WPH"
rg -q 'CL_Proc_Init' "$CL_P"
rg -q 'CL_Proc_Init' "$CL_MAIN"
rg -q 'proc_pattern' "$CL_P"
rg -q 'proc_map' "$CL_P"
rg -q 'world_proc.cpp' CMakeLists.txt
rg -q 'cl_proc.cpp' CMakeLists.txt

echo "[test_proc] demo cfg..."
test -f examples/demo_game/mod/demo_proc.cfg

echo "[test_proc] ok"
