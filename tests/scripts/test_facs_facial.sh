#!/usr/bin/env bash
# Wiring test: FACS Action Unit facial animation APIs.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT/tests/scripts/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
fail() { echo "FAIL: $*" >&2; exit 1; }

FACE="$(idtech3_require_file runtime/game/g_facial.c src/game/g_facial.c)"
FACEH="$(idtech3_require_file runtime/game/g_facial.h src/game/g_facial.h)"
LUA="$(idtech3_require_file runtime/game/g_lua_bindings.c src/game/g_lua_bindings.c)"
LUA_REG="$(idtech3_require_file runtime/game/g_lua_registration.inc src/game/g_lua_registration.inc)"
CGAME="${IDTECH3_CLIENT}/core/cl_cgame.c"
DOC="${ROOT}/docs/FACS.md"

[ -f "$DOC" ] || fail "missing docs/FACS.md"
[ -f "$CGAME" ] || fail "missing cl_cgame.c"

rg -q 'FACS_AU12' "$FACEH" || fail "FACS_AU12 missing from header"
rg -q 'FACS_AU_COUNT' "$FACEH" || fail "FACS_AU_COUNT missing"
rg -q 'Face_SetAU' "$FACEH" || fail "Face_SetAU not declared"
rg -q 'Face_ApplyMorphsToEntity' "$FACEH" || fail "Face_ApplyMorphsToEntity not declared"
rg -q 'com_faceFacs' "$FACE" || fail "com_faceFacs cvar missing"
rg -q 's_facsTable' "$FACE" || fail "FACS mapping table missing"
rg -q 'l_face_setAU' "$LUA" || fail "Lua setAU binding missing"
rg -q 'SIDE_BOTH' "$LUA_REG" || fail "Lua FACS side constants missing"
rg -q 'Face_AUName' "$LUA" || fail "Lua AU name constants missing"
rg -q 'CL_Face_ApplyMorphs' "$CGAME" || fail "cgame morph apply missing"
rg -q 'AU12' "$DOC" || fail "docs missing AU12"

echo "test_facs_facial: passed"
