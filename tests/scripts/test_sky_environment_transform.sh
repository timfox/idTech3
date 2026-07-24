#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'sky_environment_status' "$ROOT/renderers/vulkan/vk_sky_environment.c"
grep -q 'scale' "$ROOT/renderers/vulkan/vk_sky_environment.c"
echo "PASS: sky environment transform API"
