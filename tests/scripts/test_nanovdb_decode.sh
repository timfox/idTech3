#!/usr/bin/env bash
# End-to-end check: build/run NanoVDB CPU decode against the real fixture file.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

DECODE_C="$(idtech3_require_file renderers/vulkan/vk_nanovdb_decode.c src/renderers/vulkan/vk_nanovdb_decode.c)"
VDB_C="$(idtech3_require_file renderers/vulkan/vk_vdb.c src/renderers/vulkan/vk_vdb.c)"
FIXTURE_NVDB="$ROOT/tests/data/fog_2cubed.nvdb"
BUILD="${BUILD_DIR:-$ROOT/build-vk-Release}"
BIN="$BUILD/unit_nanovdb_decode"

grep -q 'VDB_NanoVDB_DecodeToDense' "$DECODE_C"
grep -q 'VDB_NanoVDB_GetIndexDims' "$DECODE_C"
grep -q 'VDB_NanoVDB_ResolveGrid' "$DECODE_C"
grep -q 'vk_nanovdb_decode.h' "$VDB_C"
grep -q 'VDB_AllocAndDecodeNanoVDB' "$VDB_C"

test -f "$FIXTURE_NVDB"

if [[ ! -x "$BIN" ]]; then
	if [[ -f "$BUILD/build.ninja" || -f "$BUILD/Makefile" ]]; then
		cmake --build "$BUILD" --target unit_nanovdb_decode -j4
	else
		echo "SKIP: build dir missing ($BUILD)"
		exit 0
	fi
fi

TEST_DATA_DIR="$ROOT/tests/data" "$BIN"
echo "test_nanovdb_decode: ok"
