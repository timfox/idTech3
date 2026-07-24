#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'liftMax' "$ROOT/renderers/vulkan/shaders/glsl/gamma.frag"
grep -q 'r_localExposure", "0"' "$ROOT/renderers/vulkan/vk_skybox_hdr.c"
echo "PASS: scenehdr black level policy"
