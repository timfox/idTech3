#!/usr/bin/env bash
# Wiring test: client modularization split (cl_main.c slim + satellite modules).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
MAIN="$ROOT/src/client/core/cl_main.c"
MAX_MAIN_LINES=320

check() {
  if ! grep -q "$2" "$1" 2>/dev/null; then
    echo "FAIL: $3"
    failures=$((failures + 1))
  else
    echo "PASS: $3"
  fi
}

main_lines=$(wc -l < "$MAIN")
if [[ "$main_lines" -gt "$MAX_MAIN_LINES" ]]; then
  echo "FAIL: cl_main.c has $main_lines lines (max $MAX_MAIN_LINES)"
  failures=$((failures + 1))
else
  echo "PASS: cl_main.c slim ($main_lines lines)"
fi

for f in cl_lifecycle.c cl_frame.c cl_cvars.c cl_connect.c cl_cmds.c cl_ref.c cl_gameframe.c; do
  if [[ ! -f "$ROOT/src/client/core/$f" ]]; then
    echo "FAIL: missing src/client/core/$f"
    failures=$((failures + 1))
  else
    echo "PASS: src/client/core/$f present"
  fi
done
for f in cl_demo.c cl_download.c; do
  if [[ ! -f "$ROOT/src/client/media/$f" ]]; then
    echo "FAIL: missing src/client/media/$f"
    failures=$((failures + 1))
  else
    echo "PASS: src/client/media/$f present"
  fi
done

check "$MAIN" 'CL_InitCvars' 'cl_main delegates cvar init'
check "$ROOT/src/client/core/cl_lifecycle.c" 'CL_ShutdownAll' 'lifecycle owns shutdown/memory'
check "$ROOT/src/client/core/cl_frame.c" 'CL_Frame' 'frame loop split'
check "$ROOT/src/client/core/cl_cvars.c" 'CL_InitCvars' 'cvar registration split'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All client modularization wiring checks passed."
