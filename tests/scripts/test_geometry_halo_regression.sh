#!/usr/bin/env bash
# Static regression: geometry-edge halo fixes stay wired (view-depth bilateral).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

AV="$ROOT/renderers/vulkan/shaders/glsl/ambient_visibility"
GLSL="$ROOT/renderers/vulkan/shaders/glsl"
POSTFX="$ROOT/renderers/vulkan/vk_postfx_passes.c"

grep -q 'av_bilateral_depth_weight' "$AV/av_filter.comp" || fail 'AV filter uses view-depth bilateral'
grep -q 'traceExtent' "$AV/av_filter.comp" || fail 'AV filter aligns full-res guidance to trace extent'
! grep -q 'texelFetch(depthTex, sp' "$AV/av_filter.comp" || fail 'AV filter misaddresses full-res depth'
grep -q 'av_positive_view_depth' "$AV/av_temporal.comp" || fail 'AV temporal uses view-depth'
grep -q 'texelFetch(finalAV' "$AV/av_composite.comp" || fail 'AV composite uses texelFetch upsample'
grep -q 'Depth_BilateralWeight' "$GLSL/ssao_blur.frag" || fail 'SSAO blur is depth-aware'
grep -q 'Depth_BilateralWeight' "$GLSL/rcgi/rcgi_upscale.comp" || fail 'RcGI upscale view-depth'
grep -q 'Depth_BilateralWeight' "$GLSL/rcgi/rcgi_denoise.comp" || fail 'RcGI denoise view-depth'
grep -q 'Depth_BilateralWeight' "$GLSL/depth_view.glsl" || fail 'shared Depth_BilateralWeight'
grep -q 'edgeDepthGate' "$GLSL/gamma.frag" || fail 'sharpen depth gate'
grep -q 'maxRel' "$GLSL/ssr.frag" || fail 'SSR view-depth silhouette reject'
grep -Fq 'push.misc[1] = ( ssaoTexW' "$POSTFX" || fail 'HBAO must not leak step count into blur texel size'

# Must not regress to device-Z * 600 sharpness in C push fill.
! grep -q 'filterPush.p\[0\] = 600' "$ROOT/renderers/vulkan/vk_ambient_visibility.c" || fail 'AV filter still uses device-Z 600'
grep -q 'filterPush.p\[0\] = 48' "$ROOT/renderers/vulkan/vk_ambient_visibility.c" || fail 'AV filter sharpness 48'

# Docs present
[[ -f "$ROOT/docs/GEOMETRY_EDGE_ARTIFACTS.md" ]] || fail 'GEOMETRY_EDGE_ARTIFACTS.md'
[[ -f "$ROOT/docs/BILATERAL_UPSAMPLING.md" ]] || fail 'BILATERAL_UPSAMPLING.md'

[[ $failures -eq 0 ]] || { echo "$failures failed"; exit 1; }
echo "All geometry_halo_regression checks passed."
