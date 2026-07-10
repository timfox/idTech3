#!/usr/bin/env bash
# BSP stream VBO + lightmap atlas wiring (symbol + behavior guards).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
cd "$ROOT"

STREAM="$(idtech3_file renderers/vulkan/tr_bsp_stream.c src/renderers/vulkan/tr_bsp_stream.c)"
BSP="$(idtech3_file renderers/vulkan/tr_bsp.c src/renderers/vulkan/tr_bsp.c)"
VBO="$(idtech3_file renderers/vulkan/vk_vbo.c src/renderers/vulkan/vk_vbo.c)"
TR_MAIN="$(idtech3_file renderers/vulkan/tr_main.c src/renderers/vulkan/tr_main.c)"

echo "[test_bsp_stream_vbo] checking sources..."
for f in "$STREAM" "$BSP" "$VBO" "$TR_MAIN"; do
	test -f "$f" || { echo "missing $f"; exit 1; }
done

echo "[test_bsp_stream_vbo] full VBO rebuild on merge/unmerge..."
rg -q 'R_BspStream_RebuildVbo' "$STREAM"
rg -q 'R_BspStream_CompactLightmaps' "$STREAM"
rg -q 'R_BspStream_RebuildVbo\(\)' "$STREAM"

echo "[test_bsp_stream_vbo] stream VBO upload + static shader gate..."
rg -q 'VBO_StreamUploadSurface' "$STREAM"
rg -q 'VBO_StreamFlushGpu' "$STREAM"
rg -q 'isStaticShader' "$VBO"

echo "[test_bsp_stream_vbo] lightmap atlas + deluxe + compaction..."
rg -q 'R_BspStreamLightmap_UploadTile' "$BSP"
rg -q 'R_BspStreamLightmap_UploadDeluxeTile' "$BSP"
rg -q 'R_BspStreamLightmap_ResetTiles' "$BSP"
rg -q 'R_BspStream_CompactLightmaps' "$STREAM"
rg -q 'R_BspStream_ReloadPatchFromDisk' "$STREAM"

echo "[test_bsp_stream_vbo] PBR parity hooks..."
rg -q 'R_BspGenerateFaceNormals' "$BSP"
rg -q 'vk_mikkt_bsp_face_generate' "$STREAM"
rg -q 'R_LightDirForPoint' "$STREAM"

echo "[test_bsp_stream_vbo] limits + residency cvar text..."
rg -q 'BSP_STREAM_MAX_PATCHES' "$STREAM"
rg -q 'BSP_STREAM_HASH_SIZE' "$STREAM"
rg -q 'patch hash table full' "$STREAM"
rg -q 'patch table full' "$STREAM"
rg -q 'stream lightmap atlas full' "$BSP"
if rg -q 'VBO deferred' "$STREAM"; then
	echo "stale r_bspStreamResident help still mentions VBO deferred" >&2
	exit 1
fi

echo "[test_bsp_stream_vbo] draw path..."
rg -q 'R_BspStream_AddSurfaces' "$TR_MAIN"
rg -q 'R_AddDrawSurf' "$STREAM"

echo "[test_bsp_stream_vbo] ok"
