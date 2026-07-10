#!/usr/bin/env bash
# Smoke check: NanoVDB CPU decode sources present and wired into vk_vdb load path.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

DECODE_C="$(idtech3_require_file renderers/vulkan/vk_nanovdb_decode.c src/renderers/vulkan/vk_nanovdb_decode.c)"
VDB_C="$(idtech3_require_file renderers/vulkan/vk_vdb.c src/renderers/vulkan/vk_vdb.c)"

grep -q 'VDB_NanoVDB_DecodeToDense' "$DECODE_C"
grep -q 'VDB_NanoVDB_GetIndexDims' "$DECODE_C"
grep -q 'VDB_NanoVDB_ResolveGrid' "$DECODE_C"
grep -q 'vk_nanovdb_decode.h' "$VDB_C"
grep -q 'VDB_AllocAndDecodeNanoVDB' "$VDB_C"
if [[ -x "$ROOT/build-vk-Release/unit_nanovdb_decode" ]]; then
	"$ROOT/build-vk-Release/unit_nanovdb_decode" || exit 1
elif [[ -x "$ROOT/build/unit_nanovdb_decode" ]]; then
	"$ROOT/build/unit_nanovdb_decode" || exit 1
fi
echo "test_nanovdb_decode: ok"
