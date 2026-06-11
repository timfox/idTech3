#!/usr/bin/env bash
# Validation: genetic GAN module wiring (genome API + client decode + Lua).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-$ROOT/build-vk-Release}"
cd "$ROOT"

fail() { echo "[test_genetic_gan] FAIL: $*" >&2; exit 1; }
ok() { echo "[test_genetic_gan] ok: $*"; }

echo "[test_genetic_gan] checking sources..."
for f in \
	src/world/genetic_gan.cpp \
	src/world/genetic_gan.h \
	src/extensions/generative/cl_genetic_gan.c \
	src/extensions/generative/cl_genetic_gan.h \
	src/extensions/generative/cl_generative.c \
	src/client/cl_main.c \
	scripts/genetic_gan_decode.py
do
	test -f "$f" || fail "missing $f"
done

rg -q 'genetic_gan.cpp' cmake/IdTech3QcommonExtensions.cmake CMakeLists.txt

echo "[test_genetic_gan] grep API symbols..."
rg -q 'GeneticGan_Init' src/world/genetic_gan.cpp
rg -q 'GeneticGan_Breed' src/world/genetic_gan.cpp
rg -q 'GeneticGan_GetPhenotype' src/world/genetic_gan.cpp
rg -q 'cl_geneticGan' src/world/genetic_gan.cpp
rg -q '"cl_geneticGan", "0"' src/world/genetic_gan.cpp
rg -q 'genetic_gan_status' src/world/genetic_gan.cpp
rg -q 'genome_create' src/world/genetic_gan.cpp
rg -q 'genome_breed' src/world/genetic_gan.cpp
rg -q 'cl_geneticGanSyncJob' src/world/genetic_gan.cpp

echo "[test_genetic_gan] grep cross-module wiring..."
rg -q 'CL_GeneticGan_Init' src/client/cl_main.c
rg -q 'CL_GeneticGan_Frame' src/extensions/generative/cl_generative.c
rg -q 'genome_generate' src/extensions/generative/cl_genetic_gan.c
rg -q 'GeneticGan_WriteGenomeJson' src/extensions/generative/cl_genetic_gan.c
rg -q 'l_genome_create' src/game/g_lua_bindings.c
rg -q 'l_genome_breed' src/game/g_lua_bindings.c
rg -q 'l_genome_getPhenotype' src/game/g_lua_bindings.c

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
