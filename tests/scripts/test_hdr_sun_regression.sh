#!/usr/bin/env bash
# Regression gates for HDR sun single-source + histogram auto exposure.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

fail=0
check() {
  local desc="$1"; shift
  if "$@"; then
    echo "PASS: $desc"
  else
    echo "FAIL: $desc"
    fail=1
  fi
}

check "hdr sun module present" test -f "$ROOT/renderers/vulkan/vk_hdr_sun.c"
check "register wired" grep -q 'vk_hdr_sun_register' "$ROOT/renderers/vulkan/tr_init.c"
check "luminance soft sun reject" grep -q 'SUN_REJECT_STOPS' "$ROOT/renderers/vulkan/shaders/glsl/postfx/luminance.comp"
check "bloom EV-relative" grep -q 'bloomMeterLuma' "$ROOT/renderers/vulkan/shaders/glsl/bloom.frag"
check "adaptedExposure packed" grep -q 'r_bloomThresholdEVRelative' "$ROOT/renderers/vulkan/vk_postfx_params.c"
check "asymmetric exp adaptation" grep -q 'darkenForBrightSceneRate' "$ROOT/renderers/vulkan/vk_temporal.c"
check "diffraction default off" grep -q 'r_sunDiffraction", "0"' "$ROOT/renderers/vulkan/vk_hdr_sun.c"
check "docs present" test -f "$ROOT/docs/HDR_SUN_EXPOSURE.md"

# Individual test script stubs must exist (plan §18).
for t in \
  test_hdr_sun_single_source.sh \
  test_hdr_sun_angular_size.sh \
  test_hdr_sun_scene_radiance.sh \
  test_hdr_sun_no_preexposure_clamp.sh \
  test_sun_histogram_percentile.sh \
  test_sun_auto_exposure_weight.sh \
  test_auto_exposure_asymmetric_adaptation.sh \
  test_auto_exposure_camera_cut.sh \
  test_exposure_contract_all_contributors.sh \
  test_bloom_exposure_relative_threshold.sh \
  test_sun_bloom_isotropic.sh \
  test_sun_diffraction_policy.sh \
  test_lens_flare_occlusion.sh \
  test_sun_tonemap_rolloff.sh \
  test_world_contrast_under_sun.sh \
  test_source_style_exposure_sequence.sh \
  test_hdr_sun_regression.sh
do
  check "script $t" test -f "$ROOT/tests/scripts/$t"
done

exit "$fail"
