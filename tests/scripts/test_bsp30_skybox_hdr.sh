#!/usr/bin/env bash
# Static gates for BSP30 sky + OpenEXR equirectangular skybox path.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

HDR="$ROOT/renderers/vulkan/vk_skybox_hdr.c"
HDRH="$ROOT/renderers/vulkan/vk_skybox_hdr.h"
BSP="$ROOT/renderers/vulkan/tr_bsp30.c"
SKY="$ROOT/renderers/vulkan/tr_sky.c"
SH="$ROOT/renderers/vulkan/tr_shader.c"
EXR="$ROOT/renderers/common/tr_image_exr.cpp"
DOC="$ROOT/docs/BSP30_FORMAT_SUPPORT.md"

grep -q 'SkyboxHDR_BuildDisplayFaces' "$HDRH" || fail 'BuildDisplayFaces declared'
grep -q 'SkyboxHDR_ConfigureFromMap' "$HDRH" || fail 'ConfigureFromMap declared'
grep -q 'SkyboxHDR_BuildDisplayFaces' "$HDR" || fail 'BuildDisplayFaces implemented'
grep -q 'equirectangular' "$HDR" || fail 'equirectangular path mentioned'
grep -q 'OpenEXR' "$HDR" || fail 'OpenEXR load messaging'
grep -q 'r_skyboxHDR_projection' "$HDR" || fail 'projection cvar'
grep -q 'GS_ParseWorldspawnSky' "$BSP" || fail 'worldspawn sky parse'
grep -q 'skybox_hdr' "$BSP" || fail 'skybox_hdr worldspawn key'
grep -q 'GS_IsSkyTextureName' "$BSP" || fail 'sky texture detection'
grep -q 'GS_CreateSkyShader' "$BSP" || fail 'sky shader creation'
grep -q 'R_CreateSkyShaderFromFaces' "$SH" || fail 'R_CreateSkyShaderFromFaces'
grep -q 'vk_sky_owner_wants_hdr_sky' "$SKY" || fail 'HDR sky draw gate in RB_StageIteratorSky'
grep -q 'LoadEXRFromMemory' "$EXR" || fail 'tinyexr OpenEXR loader'
grep -q 'GS_TryLoadSkyboxSidecar' "$BSP" || fail 'skybox_hdr sidecar loader'
grep -q 'skybox_hdr' "$DOC" || fail 'BSP30 sky docs'

if [[ "$failures" -ne 0 ]]; then
	echo "BSP30 skybox HDR regression: $failures failure(s)"
	exit 1
fi
echo "BSP30 skybox HDR regression: OK"
