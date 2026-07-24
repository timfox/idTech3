#!/usr/bin/env bash
# Static gates for mesh silhouette halo / screen-space edge fixes.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

BLOOM="$ROOT/renderers/vulkan/shaders/glsl/bloom.frag"
SMAA="$ROOT/renderers/vulkan/shaders/glsl/smaa_compose.frag"
AVF="$ROOT/renderers/vulkan/shaders/glsl/ambient_visibility/av_filter.comp"
WTAA="$ROOT/renderers/vulkan/shaders/glsl/weapon_taa.frag"
SSAO="$ROOT/renderers/vulkan/shaders/glsl/ssao.frag"
RCGI="$ROOT/renderers/vulkan/shaders/glsl/rcgi/rcgi_upscale.comp"
HALO="$ROOT/renderers/vulkan/vk_mesh_halo.c"
POSTAA="$ROOT/renderers/vulkan/vk_post_aa.c"
POSTFX="$ROOT/renderers/vulkan/vk_postfx_passes.c"

grep -q 'silhouetteExtractGate' "$BLOOM" || fail 'bloom silhouette extract gate'
grep -q 'Depth_BilateralWeight' "$BLOOM" || fail 'bloom depth-aware firefly'
grep -q 'int yMin = -1;' "$BLOOM" || fail 'bloom cross neighborhood must include vertical neighbors'
grep -q 'depthTexture' "$SMAA" || fail 'smaa compose depth binding'
grep -q 'Depth_LinearizeReversedZ' "$SMAA" || fail 'smaa compose view-depth reject'
grep -q 'texelFetch(avIn' "$AVF" || fail 'AV filter must texelFetch'
grep -q 'vec2 suv = (vec2(sp) + 0.5) / traceExtent' "$AVF" ||
	fail 'AV full-resolution guidance must use trace-relative texel-center UVs'
if grep -q 'texelFetch(depthTex, sp' "$AVF" || grep -q 'texelFetch(normalTex, sp' "$AVF"; then
	fail 'AV half-res coordinates must not integer-fetch full-res depth/normal'
fi
grep -q 'relativeDepthError' "$WTAA" || fail 'weapon TAA relative view-depth'
grep -q 'maxRel' "$SSAO" || fail 'SSAO relative silhouette skip'
grep -q 'texelFetch(gatherLow' "$RCGI" || fail 'RcGI texelFetch upsample'
grep -q 'mesh_halo_status' "$HALO" || fail 'mesh_halo_status command'
grep -q 'bindDepthForCompose' "$POSTAA" || fail 'SMAA compose depth bind'
grep -Fq 'push.misc[1] = ( ssaoTexW' "$POSTFX" ||
	fail 'SSAO blur must reset inverse width after HBAO generation'
grep -Fq 'push.misc[2] = ( ssaoTexH' "$POSTFX" ||
	fail 'SSAO blur must reset inverse height after HBAO generation'
[[ -f "$ROOT/docs/MESH_SILHOUETTE_HALO.md" ]] || fail 'docs/MESH_SILHOUETTE_HALO.md'
[[ -f "$ROOT/docs/GEOMETRY_EDGE_ARTIFACTS.md" ]] || fail 'docs/GEOMETRY_EDGE_ARTIFACTS.md'

[[ $failures -eq 0 ]] || { echo "$failures failed"; exit 1; }
echo "All mesh_halo_regression checks passed."
