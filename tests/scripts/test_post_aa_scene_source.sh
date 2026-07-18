#!/usr/bin/env bash
# Wiring test: post-AA should only switch the final scene source when an AA pass actually ran.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
failures=0

check() {
  if ! grep -q "$2" "$1" 2>/dev/null; then
    echo "FAIL: $3"
    failures=$((failures + 1))
  else
    echo "PASS: $3"
  fi
}

POST_AA="$(idtech3_file renderers/vulkan/vk_post_aa.c src/renderers/vulkan/vk_post_aa.c)"

check "$POST_AA" 'static qboolean vk_run_smaa_pass' 'SMAA pass helper reports whether the pass executed'
check "$POST_AA" 'static qboolean vk_smaa_passes' 'SMAA chain reports whether all passes executed'
check "$POST_AA" 'static qboolean vk_fxaa_pass' 'FXAA pass helper reports whether the pass executed'
check "$POST_AA" 'qboolean aa_ran = qfalse' 'post-AA apply tracks whether AA produced output'
check "$POST_AA" 'aa_output = ( aa_ran && vk.smaa_output_image_view ) ? vk.smaa_output_image_view : vk.color_image_view;' 'scene source falls back to color_image when AA output is unavailable'

if [[ $failures -ne 0 ]]; then
  echo "$failures check(s) failed"
  exit 1
fi

echo "All post-AA scene source checks passed."
