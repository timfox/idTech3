#!/usr/bin/env bash
# Procedural world pattern smoke checks (Voronoi, grid, hex, etc.).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "[test_proc] checking sources..."
for f in \
	src/world/world_proc.cpp \
	src/world/world_proc.h \
	src/client/world/cl_proc.cpp \
	src/client/world/cl_proc.h
do
	test -f "$f" || { echo "missing $f"; exit 1; }
done

echo "[test_proc] grep API symbols..."
rg -q 'WorldProc_SampleWorld' src/world/world_proc.cpp
rg -q 'WorldProc_SampleVoronoi' src/world/world_proc.cpp
rg -q 'WorldProc_ParsePattern' src/world/world_proc.cpp
rg -q 'WPROC_VORONOI' src/world/world_proc.h
rg -q 'CL_Proc_Init' src/client/world/cl_proc.cpp
rg -q 'CL_Proc_Init' src/client/core/cl_main.c
rg -q 'proc_pattern' src/client/world/cl_proc.cpp
rg -q 'proc_map' src/client/world/cl_proc.cpp
rg -q 'world_proc.cpp' CMakeLists.txt
rg -q 'cl_proc.cpp' CMakeLists.txt

echo "[test_proc] demo cfg..."
test -f examples/demo_game/mod/demo_proc.cfg

echo "[test_proc] ok"
