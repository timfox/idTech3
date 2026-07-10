#!/usr/bin/env bash
# Wiring test: client modularization split (cl_main.c slim + satellite modules).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
source "$ROOT/tests/scripts/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

failures=0
MAIN="${IDTECH3_CLIENT}/core/cl_main.c"
MAX_MAIN_LINES=320

check() {
  if ! grep -q "$2" "$1" 2>/dev/null; then
    echo "FAIL: $3"
    failures=$((failures + 1))
  else
    echo "PASS: $3"
  fi
}

[ -f "$MAIN" ] || { echo "FAIL: missing ${IDTECH3_CLIENT_REL}/core/cl_main.c"; exit 1; }

main_lines=$(wc -l < "$MAIN")
if [[ "$main_lines" -gt "$MAX_MAIN_LINES" ]]; then
  echo "FAIL: cl_main.c has $main_lines lines (max $MAX_MAIN_LINES)"
  failures=$((failures + 1))
else
  echo "PASS: cl_main.c slim ($main_lines lines)"
fi

for f in cl_lifecycle.c cl_frame.c cl_cvars.c cl_connect.c cl_cmds.c cl_ref.c cl_gameframe.c; do
  if [[ ! -f "${IDTECH3_CLIENT}/core/$f" ]]; then
    echo "FAIL: missing ${IDTECH3_CLIENT_REL}/core/$f"
    failures=$((failures + 1))
  else
    echo "PASS: ${IDTECH3_CLIENT_REL}/core/$f present"
  fi
done
for f in cl_demo.c cl_download.c; do
  if [[ ! -f "${IDTECH3_CLIENT}/media/$f" ]]; then
    echo "FAIL: missing ${IDTECH3_CLIENT_REL}/media/$f"
    failures=$((failures + 1))
  else
    echo "PASS: ${IDTECH3_CLIENT_REL}/media/$f present"
  fi
done

check "$MAIN" 'CL_InitCvars' 'cl_main delegates cvar init'
check "${IDTECH3_CLIENT}/core/cl_lifecycle.c" 'CL_ShutdownAll' 'lifecycle owns shutdown/memory'
check "${IDTECH3_CLIENT}/core/cl_frame.c" 'CL_Frame' 'frame loop split'
check "${IDTECH3_CLIENT}/core/cl_cvars.c" 'CL_InitCvars' 'cvar registration split'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All client modularization wiring checks passed."
