#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'r_bloomFireflyClamp' "$ROOT/renderers/vulkan/vk_renderer_iq_p1.c"
grep -q 'applyFireflyClamp' "$ROOT/renderers/vulkan/shaders/glsl/bloom.frag"
grep -q 'constant_id = 29' "$ROOT/renderers/vulkan/shaders/glsl/bloom.frag"
echo "test_bloom_firefly_control.sh OK"
