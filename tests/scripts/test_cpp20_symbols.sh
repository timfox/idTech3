#!/usr/bin/env bash
# Ensure critical C exports from converted leaves remain unmangled in object files.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
INC=(-I"$ROOT/engine/core" -I"$ROOT/renderers/vulkan")
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
failures=0

compile_check() {
  local src="$1" sym="$2"
  local obj="$TMP/$(basename "$src").o"
  if ! c++ -std=c++20 -fno-exceptions -fno-rtti "${INC[@]}" -c -o "$obj" "$src" 2>"$TMP/err"; then
    echo "FAIL compile $src"; cat "$TMP/err"; failures=$((failures+1)); return
  fi
  if nm -g --defined-only "$obj" 2>/dev/null | grep -q " T ${sym}$\| T ${sym}\b"; then
    echo "PASS unmangled $sym from $(basename "$src")"
  elif nm "$obj" | grep -q " ${sym}$"; then
    echo "PASS symbol $sym present in $(basename "$src")"
  else
    # Itanium mangling would look like _Z...
    if nm "$obj" | grep -q "_Z.*${sym}"; then
      echo "FAIL $sym appears mangled in $(basename "$src")"
      nm "$obj" | grep "$sym" || true
      failures=$((failures+1))
    else
      echo "FAIL $sym not found in $(basename "$src")"
      nm "$obj" | head -20
      failures=$((failures+1))
    fi
  fi
}

compile_check "$ROOT/engine/core/md4.cpp" "Com_BlockChecksum"
compile_check "$ROOT/engine/core/q_utf8.cpp" "Q_UTF8_Decode"
compile_check "$ROOT/renderers/vulkan/vk_cluster_math.cpp" "Cluster_DeriveLogZScaleBias"
compile_check "$ROOT/renderers/vulkan/vk_cluster_math.cpp" "Cluster_LightSliceSpan"

if [[ $failures -ne 0 ]]; then echo "$failures failed"; exit 1; fi
echo "All cpp20 symbol checks passed."
