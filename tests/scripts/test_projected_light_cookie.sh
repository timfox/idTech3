#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'projectedLight_t' "$ROOT/renderers/vulkan/vk_flashlight.h"
echo "PASS: projected light cookie"
