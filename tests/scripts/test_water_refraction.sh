#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'r_waterQuality' "$ROOT/renderers/vulkan/vk_water_presentation.c"
echo "PASS: water refraction quality tiers"
