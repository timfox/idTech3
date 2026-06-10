#!/usr/bin/env bash
# Sector stream matrix: verify CTest registration/labels, then run all stream-path tests.
# Invoked from the build directory (ctest WORKING_DIRECTORY = CMAKE_BINARY_DIR).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="$(pwd)"
RELEASE="${RELEASE_DIR:-$ROOT/release}"

# Required in every configure (wiring + scripts).
REQUIRED_TESTS=(
	test_openworld
	test_cm_stream_merge
	test_nav_bake
	test_openworld_sync
	test_bsp_stream_vbo
	test_openworld_residency
	test_graph_compute
	test_proc
	test_demo_openworld_pk3
	test_openworld_runtime
	test_sector_stream_fidelity
)

# Built when USE_RECAST_NAV + BUILD_UNIT_TESTS.
OPTIONAL_UNIT_TESTS=(
	unit_openworld_nav
	unit_world_residency
	unit_sector_graph
	unit_cluster_graph
)

# CMake must tag these with LABELS containing sector_stream (matrix drift guard).
REQUIRED_CMAKE_TESTS=(
	test_openworld
	test_cm_stream_merge
	test_nav_bake
	test_openworld_sync
	test_bsp_stream_vbo
	test_openworld_residency
	test_graph_compute
	test_proc
	test_demo_openworld_pk3
	test_openworld_runtime
	test_sector_stream_fidelity
	unit_world_residency
	unit_sector_graph
	unit_cluster_graph
)

echo "[sector_stream_matrix] build dir: $BUILD_DIR"
echo "[sector_stream_matrix] release dir: $RELEASE"

echo "[sector_stream_matrix] CTest inventory..."
if ! ctest -N >/tmp/sector_stream_ctest_n.txt 2>&1; then
	cat /tmp/sector_stream_ctest_n.txt >&2
	exit 1
fi

missing=0
for t in "${REQUIRED_TESTS[@]}"; do
	if ! rg -q "Test\\s+#\\d+: ${t}\$" /tmp/sector_stream_ctest_n.txt; then
		echo "FAIL: missing required CTest: $t" >&2
		missing=$((missing + 1))
	fi
done
if [[ $missing -ne 0 ]]; then
	exit 1
fi

echo "[sector_stream_matrix] CMake sector_stream label drift guard..."
for t in "${REQUIRED_CMAKE_TESTS[@]}"; do
	if ! rg -q "add_test\\(NAME ${t}" "$ROOT/CMakeLists.txt"; then
		echo "FAIL: CMakeLists missing add_test NAME ${t}" >&2
		exit 1
	fi
	if ! rg -A10 "add_test\\(NAME ${t}" "$ROOT/CMakeLists.txt" | rg -q 'sector_stream'; then
		echo "FAIL: ${t} missing LABELS sector_stream in CMakeLists.txt" >&2
		exit 1
	fi
done
if rg -q "Test\\s+#\\d+: unit_openworld_nav\$" /tmp/sector_stream_ctest_n.txt; then
	if ! rg -A12 "add_test\\(NAME unit_openworld_nav" "$ROOT/CMakeLists.txt" | rg -q 'sector_stream'; then
		echo "FAIL: unit_openworld_nav missing LABELS sector_stream" >&2
		exit 1
	fi
fi
echo "[sector_stream_matrix] label guard ok"

run_regex='test_openworld|test_cm_stream_merge|test_nav_bake|test_openworld_sync|test_bsp_stream_vbo|test_openworld_residency|test_graph_compute|test_proc|test_demo_openworld_pk3|test_openworld_runtime|test_sector_stream_fidelity'
for t in "${OPTIONAL_UNIT_TESTS[@]}"; do
	if rg -q "Test\\s+#\\d+: ${t}\$" /tmp/sector_stream_ctest_n.txt; then
		run_regex="${run_regex}|${t}"
	fi
done

export RELEASE_DIR="$RELEASE"
export BUILD_DIR="$BUILD_DIR"

echo "[sector_stream_matrix] running stream path tests (regex)..."
ctest -R "^(${run_regex})\$" --output-on-failure -E '^sector_stream_matrix$'

echo "[sector_stream_matrix] ok"
