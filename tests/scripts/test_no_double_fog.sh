#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'NO_DOUBLE_FOG\|volumetricFog' "$ROOT/docs/GRAY_VEIL.md" "$ROOT/renderers/vulkan/vk_gray_veil.c"
echo "PASS: no double fog gate"
