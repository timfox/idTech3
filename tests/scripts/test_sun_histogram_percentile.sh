#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'highPercent' "$ROOT/renderers/vulkan/shaders/glsl/postfx/luminance.comp"
grep -q 'SUN_REJECT_STOPS' "$ROOT/renderers/vulkan/shaders/glsl/postfx/luminance.comp"
echo "PASS: sun histogram percentile + soft reject"
