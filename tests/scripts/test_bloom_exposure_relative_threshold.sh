#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'bloomMeterLuma' "$ROOT/renderers/vulkan/shaders/glsl/bloom.frag"
grep -q 'r_bloomThresholdEVRelative' "$ROOT/renderers/vulkan/vk_postfx_params.c"
echo "PASS: bloom EV-relative threshold"
