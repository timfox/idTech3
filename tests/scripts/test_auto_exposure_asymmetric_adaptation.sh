#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'darkenForBrightSceneRate' "$ROOT/renderers/vulkan/vk_temporal.c"
grep -q 'brightenForDarkSceneRate' "$ROOT/renderers/vulkan/vk_temporal.c"
grep -q 'expf' "$ROOT/renderers/vulkan/vk_temporal.c"
echo "PASS: asymmetric exponential adaptation"
