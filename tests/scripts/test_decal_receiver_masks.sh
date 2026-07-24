#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'targetMask' "$ROOT/renderers/vulkan/vk_world_presentation.h"
echo "PASS: decal receiver masks"
