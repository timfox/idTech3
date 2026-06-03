#!/usr/bin/env bash
# Smoke check: NanoVDB CPU decode sources present and wired into vk_vdb load path.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DECODE_C="$ROOT/src/renderers/vulkan/vk_nanovdb_decode.c"
VDB_C="$ROOT/src/renderers/vulkan/vk_vdb.c"

test -f "$DECODE_C"
test -f "$VDB_C"
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
