#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
c="$ROOT/renderers/vulkan/vk_renderer_p1_live.c"
grep -q 'vk_renderer_p1_preflight' "$c"
grep -q 'iq_certify_preflight' "$c"
grep -q 'no world loaded' "$c"
grep -q 'r_gbufferQuality' "$c"
grep -q 'BloomSourceHDR\|bloom source' "$c"
echo "All p1_live_preflight checks passed."
