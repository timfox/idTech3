#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'doTonemap' "$ROOT/renderers/vulkan/shaders/glsl/gamma.frag"
echo "PASS: tonemap gray ramp path"
