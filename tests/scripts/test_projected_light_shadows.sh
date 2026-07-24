#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
grep -q 'reversed-Z' "$ROOT/renderers/vulkan/vk_projected_lights.c"
echo "PASS: projected light shadow policy"
