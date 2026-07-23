#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'OIT_SOURCE_ALPHA_PREMULTIPLIED' "$ROOT/renderers/vulkan/shaders/glsl/oit_source_normalize.glsl"
grep -q 'decodedSource.rgb / a' "$ROOT/renderers/vulkan/shaders/glsl/oit_source_normalize.glsl"
grep -q 'OIT_SAMPLE_FLAG_CLAMPED_DIV' "$ROOT/renderers/vulkan/shaders/glsl/oit_source_normalize.glsl"
echo "OK: premul normalization"
