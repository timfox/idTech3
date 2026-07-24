#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'r_sunDiffraction' "$ROOT/renderers/vulkan/vk_hdr_sun.c"
grep -q 'Default 0' "$ROOT/renderers/vulkan/vk_hdr_sun.c"
echo "PASS: sun diffraction policy default off"
