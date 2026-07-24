#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'vk_lens_flare_sample_visibility' "$ROOT/renderers/vulkan/vk_lens_flare.c"
grep -q 'r_lensFlare", "0"' "$ROOT/renderers/vulkan/tr_init.c"
echo "PASS: lens flare multi-sample occlusion + default off"
