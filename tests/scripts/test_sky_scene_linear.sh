#!/usr/bin/env bash
# Static gates: visible HDR sky must stay scene-linear (no Reinhard→RGBA8 flatten).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

HDR="$ROOT/renderers/vulkan/vk_skybox_hdr.c"
DOC="$ROOT/docs/HDR_SKY_RENDERING.md"
SKY="$ROOT/renderers/vulkan/tr_sky.c"

grep -q 'R_CreateImageRGBA32F' "$HDR" || fail 'visible sky must upload RGBA32F'
grep -q 'SkyboxHDR_VisibleRadianceScale' "$HDR" || fail 'EV radiance scale missing'
grep -q 'exp2f' "$HDR" || fail 'exp2 EV scale missing'
grep -q 'NONE_SCENE_LINEAR_RGBA32F' "$HDR" || fail 'flatten stage marker missing'
grep -q 'sky_hdr_status' "$HDR" || fail 'sky_hdr_status command'
grep -q 'sky_hdr_validate' "$HDR" || fail 'sky_hdr_validate command'
# Must not Reinhard-crush visible faces
if grep -n 'ToneMapChannel\|x / ( 1.0f + x )' "$HDR" | grep -v 'historical\|removed\|FIRST_STAGE' >/dev/null; then
	# Allow comments only; fail if active Reinhard in BuildDisplayFaces body
	if grep -A80 'SkyboxHDR_BuildDisplayFaces' "$HDR" | grep -q '1.0f + x'; then
		fail 'Reinhard still in BuildDisplayFaces'
	fi
fi
grep -q 'SCENE_HDR_SKY_ATMOSPHERE' "$SKY" || fail 'SceneHDR sky writer note missing'
grep -q 'FIRST_STAGE_FLATTENING_SKY' "$DOC" || fail 'HDR_SKY_RENDERING.md missing'

[[ "$failures" -eq 0 ]] || { echo "sky HDR decode/scene-linear: $failures failure(s)"; exit 1; }
echo "sky HDR decode/scene-linear: OK"
