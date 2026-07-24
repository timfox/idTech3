#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'bloomMeterLuma' "$ROOT/renderers/vulkan/shaders/glsl/bloom.frag"
echo "PASS: bloom exposure relative"
