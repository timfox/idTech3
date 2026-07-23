#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'OIT_SOURCE_ALPHA_STRAIGHT\|encoding == OIT_SOURCE_ALPHA_STRAIGHT\|STRAIGHT' "$ROOT/renderers/vulkan/shaders/glsl/oit_source_normalize.glsl"
grep -q 'unassociatedRadiance = decodedSource.rgb' "$ROOT/renderers/vulkan/shaders/glsl/oit_source_normalize.glsl"
echo "OK: straight normalization"
