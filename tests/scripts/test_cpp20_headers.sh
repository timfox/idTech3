#!/usr/bin/env bash
# Dual-compile selected public headers as C and as C++20.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
INC=(-I"$ROOT/engine/core" -I"$ROOT/renderers/vulkan" -I"$ROOT/renderers/common")
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
failures=0

HEADERS=(
  "q_compat.h"
  "cpp20_compat.h"
  "q_utf8.h"
  "vk_cluster_contract.h"
)

# Intentionally C-preferred heavy headers: compile with extern "C" wrap in the probe.
# qcommon.h requires q_shared.h first (documented include order).
WRAPPED=(
  "q_shared.h"
)

compile_one() {
  local hdr="$1" mode="$2" wrap="$3"
  local base
  base=$(basename "$hdr" | tr './' '__')
  local src="$TMP/${base}_${mode}.cc"
  if [[ "$wrap" == "1" ]]; then
    cat >"$src" <<EOF
#ifdef __cplusplus
extern "C" {
#endif
#include "$hdr"
#ifdef __cplusplus
}
#endif
int cpp20_header_probe_${base}=1;
EOF
  elif [[ "$hdr" == "qcommon.h" ]]; then
    cat >"$src" <<EOF
#ifdef __cplusplus
extern "C" {
#endif
#include "q_shared.h"
#include "qcommon.h"
#ifdef __cplusplus
}
#endif
int cpp20_header_probe_qcommon=1;
EOF
  else
    cat >"$src" <<EOF
#include "$hdr"
int cpp20_header_probe_${base}=1;
EOF
  fi
  if [[ "$mode" == "c" ]]; then
    if ! cc -std=c17 -x c "${INC[@]}" -c -o "$TMP/${base}_c.o" "$src" 2>"$TMP/${base}_c.err"; then
      echo "FAIL C include $hdr"; cat "$TMP/${base}_c.err"; failures=$((failures+1)); return
    fi
  else
    if ! c++ -std=c++20 -fno-exceptions -fno-rtti -x c++ "${INC[@]}" -c -o "$TMP/${base}_cxx.o" "$src" 2>"$TMP/${base}_cxx.err"; then
      echo "FAIL C++20 include $hdr"; cat "$TMP/${base}_cxx.err"; failures=$((failures+1)); return
    fi
  fi
  echo "PASS $mode include $hdr"
}

for h in "${HEADERS[@]}"; do
  compile_one "$h" c 0
  compile_one "$h" cxx 0
done
for h in "${WRAPPED[@]}"; do
  compile_one "$h" c 1
  compile_one "$h" cxx 1
done
compile_one "qcommon.h" c 0
compile_one "qcommon.h" cxx 0

if [[ $failures -ne 0 ]]; then echo "$failures failed"; exit 1; fi
echo "All cpp20 header dual-compile checks passed."
