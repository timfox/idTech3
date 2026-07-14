#!/usr/bin/env bash
# Validation: genetic GAN module wiring (genome API + client decode + Lua).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
BUILD="${1:-$ROOT/build-vk-Release}"
cd "$ROOT"

fail() { echo "[test_genetic_gan] FAIL: $*" >&2; exit 1; }
ok() { echo "[test_genetic_gan] ok: $*"; }

echo "[test_genetic_gan] checking sources..."
GG="$(idtech3_require_file modules/world/genetic_gan.cpp src/world/genetic_gan.cpp)"
idtech3_require_file modules/world/genetic_gan.h src/world/genetic_gan.h >/dev/null
CL_GG="$(idtech3_require_file extensions/generative/cl_genetic_gan.c src/extensions/generative/cl_genetic_gan.c)"
idtech3_require_file extensions/generative/cl_genetic_gan.h src/extensions/generative/cl_genetic_gan.h >/dev/null
CL_GEN="$(idtech3_require_file extensions/generative/cl_generative.c src/extensions/generative/cl_generative.c)"
CL_MAIN="$(idtech3_require_file runtime/client/core/cl_main.c src/client/core/cl_main.c)"
LUA_B="$(idtech3_require_file runtime/game/g_lua_bindings.c src/game/g_lua_bindings.c)"
test -f scripts/genetic_gan_decode.py || fail "missing genetic_gan_decode.py"

rg -q 'genetic_gan.cpp' cmake/IdTech3QcommonExtensions.cmake CMakeLists.txt

echo "[test_genetic_gan] grep API symbols..."
rg -q 'GeneticGan_Init' "$GG"
rg -q 'GeneticGan_Breed' "$GG"
rg -q 'GeneticGan_GetPhenotype' "$GG"
rg -q 'cl_geneticGan' "$GG"
rg -q '"cl_geneticGan", "1"' "$GG"
rg -q 'genetic_gan_status' "$GG"
rg -q 'genome_create' "$GG"
rg -q 'genome_breed' "$GG"
rg -q 'cl_geneticGanSyncJob' "$GG"

echo "[test_genetic_gan] grep cross-module wiring..."
rg -q 'CL_GeneticGan_Init' "$CL_MAIN"
rg -q 'CL_GeneticGan_Frame' "$CL_GEN"
rg -q 'genome_generate' "$CL_GG"
rg -q 'GeneticGan_WriteGenomeJson' "$CL_GG"
rg -q 'l_genome_create' "$LUA_B"
rg -q 'l_genome_breed' "$LUA_B"
rg -q 'l_genome_getPhenotype' "$LUA_B"

if [ -x "$BUILD/unit_genetic_gan" ] || [ -f "$BUILD/unit_genetic_gan" ]; then
	echo "[test_genetic_gan] running unit_genetic_gan..."
	"$BUILD/unit_genetic_gan" || fail "unit_genetic_gan failed"
	ok "unit_genetic_gan"
else
	echo "[test_genetic_gan] SKIP: unit_genetic_gan not built (run cmake in $BUILD)"
fi

python3 -m py_compile "$ROOT/scripts/genetic_gan_decode.py"
ok "genetic_gan_decode.py syntax"

echo "[test_genetic_gan] done"
