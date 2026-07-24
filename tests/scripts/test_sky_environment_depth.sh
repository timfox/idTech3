#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'depth-safe\|weapon depth\|main opaque' "$ROOT/renderers/vulkan/vk_sky_environment.c"
echo "PASS: sky environment depth policy documented"
