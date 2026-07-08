#!/usr/bin/env bash
# Guard: C→C++20 world/open-world migration sources stay .cpp and CMake stays on C++20.
# CI toolchains (GCC 15+ / Clang 18+) expose cxx_std_20; C++17 fallback is not checked here.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CMAKE="$ROOT/CMakeLists.txt"

fail() { echo "[test_cpp20_sources] FAIL: $*" >&2; exit 1; }
ok() { echo "[test_cpp20_sources] ok: $*"; }

if command -v rg >/dev/null 2>&1; then
	search_q() {
		rg -q "$1" "$2"
	}
else
	search_q() {
		grep -Eq "$1" "$2"
	}
fi

MIGRATED_CPP=(
	src/world/world_open.cpp
	src/world/world_district.cpp
	src/world/world_proc.cpp
	src/world/sector_graph.cpp
	src/world/world_residency.cpp
	src/world/fog_biology.cpp
	src/world/genetic_gan.cpp
	src/qcommon/cluster_graph.cpp
	src/navigation/nav_bsp_extract.cpp
)

REVERTED_C=(
	src/world/world_open.c
	src/world/world_district.c
	src/world/world_proc.c
	src/world/sector_graph.c
	src/world/world_residency.c
	src/world/fog_biology.c
	src/world/genetic_gan.c
	src/qcommon/cluster_graph.c
	src/navigation/nav_bsp_extract.c
)

echo "[test_cpp20_sources] checking CMake C++20 standard..."
search_q 'set\(IDTECH3_CXX_STANDARD 20\)' "$CMAKE" \
	|| fail "CMakeLists.txt must set IDTECH3_CXX_STANDARD 20"
search_q 'CMAKE_CXX_STANDARD \$\{IDTECH3_CXX_STANDARD\}' "$CMAKE" \
	|| fail "CMakeLists.txt must wire CMAKE_CXX_STANDARD to IDTECH3_CXX_STANDARD"
ok "IDTECH3_CXX_STANDARD 20 wired in CMake"

echo "[test_cpp20_sources] checking open-world .cpp modules in IdTech3QcommonExtensions.cmake..."
QC_EXT="${ROOT}/cmake/IdTech3QcommonExtensions.cmake"
WORLD_QCOMMON=(
	world_open
	world_district
	world_residency
	sector_graph
	fog_biology
	genetic_gan
	world_proc
)
for mod in "${WORLD_QCOMMON[@]}"; do
	search_q "(${mod}\\.cpp|world/${mod}\\.cpp)" "$QC_EXT" \
		|| fail "open-world macro must list ${mod}.cpp"
done
QC_CORE="${ROOT}/cmake/EngineQcommonSources.cmake"
search_q 'src/qcommon/\*\.cpp' "$QC_CORE" \
	|| fail "EngineQcommonSources must glob src/qcommon/*.cpp (includes cluster_graph.cpp)"
[ -f "${ROOT}/src/qcommon/cluster_graph.cpp" ] || fail "missing cluster_graph.cpp"
ok "open-world qcommon modules are .cpp in IdTech3QcommonExtensions.cmake"

echo "[test_cpp20_sources] checking migrated .cpp sources..."
for rel in "${MIGRATED_CPP[@]}"; do
	path="$ROOT/$rel"
	[ -f "$path" ] || fail "missing $rel"
	search_q "$(basename "$rel")" "$CMAKE" \
		|| fail "$(basename "$rel") not referenced in CMakeLists.txt"
	if ! search_q 'extern "C"' "$path"; then
		fail "$rel missing extern \"C\" API boundary"
	fi
done
ok "${#MIGRATED_CPP[@]} migrated .cpp files present and wired"

echo "[test_cpp20_sources] checking no .c reverts..."
for rel in "${REVERTED_C[@]}"; do
	[ ! -f "$ROOT/$rel" ] || fail "reverted C source still present: $rel"
done
for rel in "${REVERTED_C[@]}"; do
	if search_q "${rel}([\" ;)]|$)" "$CMAKE" 2>/dev/null; then
		fail "CMakeLists.txt still references reverted $rel"
	fi
done
ok "no legacy .c siblings for migrated modules"

echo "[test_cpp20_sources] done"
