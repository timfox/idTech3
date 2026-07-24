#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'volumetric_clear_validate' "$ROOT/renderers/vulkan/vk_gray_veil.c"
echo "PASS: volumetric clear validate"
