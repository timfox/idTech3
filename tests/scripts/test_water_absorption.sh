#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'absorption' "$ROOT/renderers/vulkan/vk_world_presentation.h"
echo "PASS: water absorption fields"
