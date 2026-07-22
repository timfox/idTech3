#!/usr/bin/env bash
# ABI guard source presence + dual-language sizeof probe for critical types.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
INC=(-I"$ROOT/engine/core")
failures=0

check_file() {
  if [[ ! -f "$1" ]]; then echo "FAIL missing $1"; failures=$((failures+1)); else echo "PASS present $(basename "$1")"; fi
}
check_file "$ROOT/engine/core/cpp20_abi_guards.cpp"
check_file "$ROOT/docs/CPP20_ABI_BOUNDARIES.md"

# Compile ABI guards as C++20 (exceptions/RTTI off).
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
if ! c++ -std=c++20 -fno-exceptions -fno-rtti "${INC[@]}" -c -o "$TMP/abi.o" \
  "$ROOT/engine/core/cpp20_abi_guards.cpp" 2>"$TMP/abi.err"; then
  echo "FAIL: cpp20_abi_guards.cpp does not compile"
  cat "$TMP/abi.err"
  failures=$((failures+1))
else
  echo "PASS: cpp20_abi_guards.cpp compiles as C++20"
fi

# Header-only sizeof probe in C and C++ must agree on linux amd64.
cat >"$TMP/probe.c" <<'EOF'
#include "q_shared.h"
#include "qcommon.h"
#include <stdio.h>
int main(void){
  printf("%zu %zu %zu %zu\n", sizeof(trace_t), sizeof(usercmd_t), sizeof(entityState_t), sizeof(playerState_t));
  return 0;
}
EOF
cp "$TMP/probe.c" "$TMP/probe.cpp"
c++ -std=c++20 -fno-exceptions -fno-rtti -x c++ "${INC[@]}" -o "$TMP/probe_cxx" "$TMP/probe.cpp"
cc -std=c17 "${INC[@]}" -o "$TMP/probe_c" "$TMP/probe.c"
OUT_C=$("$TMP/probe_c")
OUT_CXX=$("$TMP/probe_cxx")
if [[ "$OUT_C" != "$OUT_CXX" ]]; then
  echo "FAIL: C/C++ sizeof mismatch: C=[$OUT_C] CXX=[$OUT_CXX]"
  failures=$((failures+1))
else
  echo "PASS: C and C++ sizeof agree ($OUT_C)"
fi

if [[ $failures -ne 0 ]]; then echo "$failures failed"; exit 1; fi
echo "All cpp20 ABI checks passed."
