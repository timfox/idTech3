#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'vk_renderer_p1_failure_capture' "$ROOT/renderers/vulkan/vk_renderer_p1_failure.c"
grep -q 'renderer_p1_last_failure' "$ROOT/renderers/vulkan/vk_renderer_p1_failure.c"
grep -q 'render_cert/failures' "$ROOT/renderers/vulkan/vk_renderer_p1_failure.c"
echo "All p1_failure_bundles checks passed."
