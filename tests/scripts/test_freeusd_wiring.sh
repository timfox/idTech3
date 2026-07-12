#!/usr/bin/env bash
# Wiring test: FreeUSD submodule path, CMake embed, client/renderer hooks.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT/tests/scripts/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
fail() { echo "FAIL: $*" >&2; exit 1; }

DOC="${ROOT}/docs/FREEUSD.md"
CMAKE_FU="${ROOT}/cmake/FreeUSD.cmake"
CL_USD="$(idtech3_require_file runtime/client/shell/cl_usd.cpp runtime/client/shell/cl_usd.cpp)"
CL_USD_H="$(idtech3_require_file runtime/client/shell/cl_usd.h runtime/client/shell/cl_usd.h)"
TR_REG="$(idtech3_require_file renderers/common/tr_model_freeusd_register.c src/renderers/common/tr_model_freeusd_register.c)"

[ -f "$DOC" ] || fail "missing docs/FREEUSD.md"
[ -f "$CMAKE_FU" ] || fail "missing cmake/FreeUSD.cmake"
rg -q 'third_party/FreeUSD' "$CMAKE_FU" || fail "FreeUSD.cmake must prefer third_party/FreeUSD"
rg -q 'path = third_party/FreeUSD' "${ROOT}/.gitmodules" || fail ".gitmodules path must be third_party/FreeUSD"
rg -q 'USE_FREEUSD' "${ROOT}/CMakeLists.txt" || fail "CMakeLists missing USE_FREEUSD"
rg -q '_IDTECH3_FREEUSD_DEFAULT' "${ROOT}/CMakeLists.txt" || fail "CMakeLists must set FreeUSD default via _IDTECH3_FREEUSD_DEFAULT (Android OFF)"
rg -q 'static cvar_t \*com_freeusd' "$CL_USD" || fail "com_freeusd static missing"
# Cvars must be declared before USE_FREEUSD block so MSVC stub CL_USD_Init compiles
python3 - <<PY
from pathlib import Path
text = Path("$CL_USD").read_text()
cvar = text.find("static cvar_t *com_freeusd")
ifdef = text.find("#ifdef USE_FREEUSD")
init = text.find("CL_USD_Init")
assert cvar >= 0 and ifdef >= 0 and init >= 0, "markers missing"
assert cvar < ifdef, "com_freeusd must be declared before #ifdef USE_FREEUSD (MSVC stub)"
assert "void CL_USD_Init" in Path("$CL_USD_H").read_text()
assert "static inline void CL_USD_Init" not in Path("$CL_USD_H").read_text(), "cl_usd.h must not inline CL_USD_Init (MSVC C2084)"
print("cl_usd FreeUSD stub layout ok")
PY
rg -q 'R_Freeusd|USE_FREEUSD' "$TR_REG" || fail "renderer FreeUSD register missing"
rg -q 'third_party/FreeUSD' "$DOC" || fail "docs must document third_party/FreeUSD"
rg -q 'MSVC' "$DOC" || fail "docs must mention MSVC stub vs CMake"

# Submodule tree optional in shallow CI without init — warn only
if [ -f "${ROOT}/third_party/FreeUSD/CMakeLists.txt" ]; then
  echo "FreeUSD submodule present"
else
  echo "WARN: third_party/FreeUSD not initialized (ok if CI inits later)"
fi

echo "test_freeusd_wiring: passed"
